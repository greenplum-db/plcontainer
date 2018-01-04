

/**
 * SQL message handler implementation.
 *
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#include "postgres.h"
#include "executor/spi.h"
#include "parser/parse_type.h"
#include "access/xact.h"

#include "common/comm_utils.h"
#include "common/comm_channel.h"
#include "common/comm_connectivity.h"
#include "plc_typeio.h"
#include "sqlhandler.h"
#include "plcontainer_common.h"

static plcMsgResult *create_sql_result(bool isSelect);

static plcMsgRaw *create_prepare_result(int64 pplan, plcDatatype *type, int nargs);

void deinit_pplan_slots(plcConn *conn);

void init_pplan_slots(plcConn *conn);

static plcMsgResult *create_sql_result(bool isSelect) {
	plcMsgResult *result;
	uint32 i, j;
	plcTypeInfo *resTypes = NULL;

	result = palloc(sizeof(plcMsgResult));
	result->msgtype = MT_RESULT;
	result->rows = SPI_processed;

	if (!isSelect) {
		result->cols = 0;
		result->types = NULL;
		result->names = NULL;
		result->data = NULL;
		result->exception_callback = NULL;
		return result;

	} else if (SPI_tuptable == NULL) {
		plc_elog(ERROR, "Unexpected error: SPI returns NULL result");
	}

	result->cols = SPI_tuptable->tupdesc->natts;
	result->types = palloc(result->cols * sizeof(*result->types));
	result->names = palloc(result->cols * sizeof(*result->names));
	result->exception_callback = NULL;
	resTypes = palloc(result->cols * sizeof(plcTypeInfo));
	for (j = 0; j < result->cols; j++) {
		fill_type_info(NULL, SPI_tuptable->tupdesc->attrs[j]->atttypid, &resTypes[j]);
		copy_type_info(&result->types[j], &resTypes[j]);
		result->names[j] = SPI_fname(SPI_tuptable->tupdesc, j + 1);
	}

	if (result->rows == 0) {
		result->data = NULL;
	} else {
		bool isnull;
		Datum origval;

		result->data = palloc(sizeof(*result->data) * result->rows);
		for (i = 0; i < result->rows; i++) {
			result->data[i] = palloc(result->cols * sizeof(*result->data[i]));
			for (j = 0; j < result->cols; j++) {
				origval = SPI_getbinval(SPI_tuptable->vals[i],
				                        SPI_tuptable->tupdesc,
				                        j + 1,
				                        &isnull);
				if (isnull) {
					result->data[i][j].isnull = 1;
					result->data[i][j].value = NULL;
				} else {
					result->data[i][j].isnull = 0;
					result->data[i][j].value = resTypes[j].outfunc(origval, &resTypes[j]);
				}
			}
		}
	}

	for (i = 0; i < result->cols; i++) {
		free_type_info(&resTypes[i]);
	}
	pfree(resTypes);

	return result;
}

static plcMsgRaw *create_prepare_result(int64 pplan, plcDatatype *type, int nargs) {
	plcMsgRaw *result;
	unsigned int offset;

	result = palloc(sizeof(plcMsgRaw));
	result->msgtype = MT_RAW;
	result->size = sizeof(int32) + sizeof(int64) + sizeof(int32) + nargs * sizeof(plcDatatype);
	result->data = pmalloc(result->size);

	offset = 0;
	/* We need to transfer the state of plan (i.e. valid or not). */
	*((int32 *) (result->data + offset)) = !!(*(void **) pplan);
	offset += sizeof(int32);
	*((int64 *) (result->data + offset)) = pplan;
	offset += sizeof(int64);
	*((int32 *) (result->data + offset)) = nargs;
	offset += sizeof(int32);
	if (nargs > 0)
		memcpy(result->data + offset, type, nargs * sizeof(plcDatatype));

	return result;
}

static plcMsgRaw *create_unprepare_result(int32 retval) {
	plcMsgRaw *result;

	result = palloc(sizeof(plcMsgRaw));
	result->msgtype = MT_RAW;
	result->size = sizeof(int32);
	result->data = pmalloc(result->size);

	*((int32 *) result->data) = retval;

	return result;
}

static int search_pplan(plcConn *conn, int64 pplan) {
	int i;
	struct pplan_slots *pplans;

	if (pplan == 0)
		return -1;

	pplans = conn->pplans;

	for (i = 0; i < MAX_PPLAN; i++) {
		if (pplans[i].pplan == pplan)
			return i;
	}

	return -1;
}

static int insert_pplan(plcConn *conn, int64 pplan) {
	int slot;
	struct pplan_slots *pplans;

	if (pplan == 0)
		return -1;

	pplans = conn->pplans;

	slot = conn->head_free_pplan_slot;
	if (slot >= 0) {
			plc_elog(DEBUG1, "Inserting pplan 0x%llx at slot %d for container %d",
			        (long long) pplan, slot, conn->container_slot);
		pplans[slot].pplan = pplan;
		conn->head_free_pplan_slot = pplans[slot].next;
	}

	return slot;
}

static int delete_pplan(plcConn *conn, int64 pplan) {
	int slot;
	struct pplan_slots *pplans;

	slot = search_pplan(conn, pplan);

	if (slot >= 0) {
		pplans = conn->pplans;
			plc_elog(DEBUG1, "Removing pplan 0x%llx at slot %d, for container %d",
			        (long long) pplan, slot, conn->container_slot);
		pplans[slot].pplan = 0;
		pplans[slot].next = conn->head_free_pplan_slot;
		conn->head_free_pplan_slot = slot;
	}

	return slot;
}

static int free_plc_plan(plcConn *conn, int64 pplan) {
	plcPlan *plc_plan;
	int retval;

	retval = delete_pplan(conn, pplan);
	if (retval < 0)
		return retval;

	plc_plan = (plcPlan *) (pplan - offsetof(plcPlan, plan));
	if (plc_plan->argOids)
		pfree(plc_plan->argOids);
	if (plc_plan->plan)
		retval = SPI_freeplan(plc_plan->plan);
	pfree(plc_plan);

	return retval;
}

void deinit_pplan_slots(plcConn *conn) {
	int i;
	struct pplan_slots *pplans = conn->pplans;
	int64 pplan;

	for (i = 0; i < MAX_PPLAN; i++) {
		pplan = pplans[i].pplan;
		free_plc_plan(conn, pplan);
	}
}

void init_pplan_slots(plcConn *conn) {
	int i;
	struct pplan_slots *pplans = conn->pplans;

	for (i = 0; i < MAX_PPLAN - 1; i++) {
		pplans[i].pplan = 0;
		pplans[i].next = i + 1;
	}

	/* Process the last entry. */
	pplans[i].pplan = 0;
	pplans[i].next = -1;

	conn->head_free_pplan_slot = 0;
}


plcMessage *handle_sql_message(plcMsgSQL *msg, plcConn *conn, plcProcInfo *pinfo) {
	int i, retval;
	plcMessage *result = NULL;
	SPIPlanPtr tmpplan;
	plcPlan *plc_plan;
	Oid type_oid;
	plcDatatype *argTypes;
	int32 typemod;
	volatile MemoryContext oldcontext;
	volatile ResourceOwner oldowner;

	oldcontext = CurrentMemoryContext;
	oldowner = CurrentResourceOwner;
	
	/* 
	 * We need to make sure BeginInternalSubTransaction()
	 * is called before we enter into PG_TRY block, and the
	 * memory context is the function's memory context. Otherwise,
	 * RollbackAndReleaseCurrentSubTransaction will cause a FATAL
	 * error due to SubTransaction is not inited.
	 */
	BeginInternalSubTransaction(NULL);
	MemoryContextSwitchTo(oldcontext);

	PG_TRY();
	{

		switch (msg->sqltype) {
			case SQL_TYPE_STATEMENT:
			case SQL_TYPE_PEXECUTE:
				if (msg->sqltype == SQL_TYPE_PEXECUTE) {
					char *nulls;
					Datum *values;
					plcTypeInfo *pexecType;

					if (search_pplan(conn, (int64) msg->pplan) < 0)
						plc_elog(ERROR, "There is no such prepared plan: %p", msg->pplan);
					plc_plan = (plcPlan *) ((char *) msg->pplan - offsetof(plcPlan, plan));
					if (plc_plan->nargs != msg->nargs) {
						plc_elog(ERROR, "argument number wrong for execute with plan: "
									"Saved number (%d) vs transferred number (%d)",
								     plc_plan->nargs, msg->nargs);
					}

					if (msg->nargs > 0) {
						nulls = pmalloc(msg->nargs * sizeof(char));
						values = pmalloc(msg->nargs * sizeof(Datum));
					} else {
						nulls = NULL;
						values = NULL;
					}
					pexecType = palloc(sizeof(plcTypeInfo));

					for (i = 0; i < msg->nargs; i++) {
						if (msg->args[i].data.isnull) {
							/* all the build-in type is strict, so we set value to Datum 0. */
							values[i] = (Datum) 0;
							nulls[i] = 'n';
						} else {
							/* A bit heavy to populate plcTypeInfo. */
							fill_type_info(NULL, plc_plan->argOids[i], pexecType);
							values[i] = pexecType->infunc(msg->args[i].data.value, pexecType);
							nulls[i] = ' ';
						}
					}

					retval = SPI_execute_plan(plc_plan->plan, values, nulls,
					                          pinfo->fn_readonly, (long) msg->limit);
					if (values)
						pfree(values);
					if (nulls)
						pfree(nulls);
					pfree(pexecType);
				} else {
					retval = SPI_execute(msg->statement, pinfo->fn_readonly,
					                     (long) msg->limit);
				}
				switch (retval) {
					case SPI_OK_SELECT:
					case SPI_OK_INSERT_RETURNING:
					case SPI_OK_DELETE_RETURNING:
					case SPI_OK_UPDATE_RETURNING:
						/* some data was returned back */
						result = (plcMessage *) create_sql_result(true);
						break;
					case SPI_OK_INSERT:
					case SPI_OK_DELETE:
					case SPI_OK_UPDATE:
						/* only return number of rows that are processed */
						result = (plcMessage *) create_sql_result(false);
						break;
					default:
						plc_elog(ERROR, "Cannot handle sql ('%s') with fn_readonly (%d) "
									"and limit ("
									INT64_FORMAT
									"). Returns %d", msg->statement,
								     pinfo->fn_readonly, msg->limit, retval);
						break;
				}
				SPI_freetuptable(SPI_tuptable);
				break;
			case SQL_TYPE_PREPARE:
				plc_plan = plc_top_alloc(sizeof(plcPlan));

				if (msg->nargs > 0) {
					plc_plan->argOids = plc_top_alloc(msg->nargs * sizeof(Oid));
					argTypes = pmalloc(msg->nargs * sizeof(plcDatatype));
				} else {
					plc_plan->argOids = NULL;
					argTypes = NULL;
				}
				for (i = 0; i < msg->nargs; i++) {
					if (msg->args[i].type.type == PLC_DATA_TEXT) {
						parseTypeString(msg->args[i].type.typeName, &type_oid, &typemod);
						plc_plan->argOids[i] = type_oid;
					} else if (msg->args[i].type.type == PLC_DATA_INT4) {
						/*for R only*/
						plc_plan->argOids[i] = (Oid) strtol(msg->args[i].type.typeName, NULL, 10);
						if (errno == ERANGE) {
							plc_elog(ERROR, "unable to parse the given OID %s", msg->args[i].type.typeName);
						}
					} else {
						plc_elog(ERROR, "prepare type is bad, unexpected prepare sql type %d",
								        msg->args[i].type.type);
					}
					argTypes[i] = plc_get_datatype_from_oid(plc_plan->argOids[i]);
				}
				plc_plan->nargs = msg->nargs;
				plc_plan->plan = SPI_prepare(msg->statement, plc_plan->nargs, plc_plan->argOids);
				if (plc_plan->plan != NULL) {
					/* plan needs to survive cross memory context. */
					tmpplan = plc_plan->plan;
					plc_plan->plan = SPI_saveplan(tmpplan);
					SPI_freeplan(tmpplan);

					if (insert_pplan(conn, (int64) &plc_plan->plan) < 0) {
						SPI_freeplan(plc_plan->plan);
						plc_elog(ERROR, "Can not insert new prepared plan.");
					}
				} else {
					/* Log the prepare failure but let the backend handle. */
					plc_elog(LOG, "SPI_prepare() fails for '%s', with %d arguments: %s",
							     msg->statement, plc_plan->nargs, SPI_result_code_string(SPI_result));
					}
				result = (plcMessage *) create_prepare_result((int64) &plc_plan->plan, argTypes,
				                                              plc_plan->nargs);
				break;
			case SQL_TYPE_UNPREPARE:
				retval = free_plc_plan(conn, (int64) msg->pplan);
				result = (plcMessage *) create_unprepare_result(retval);
				break;
			default:
				plc_elog(ERROR, "Cannot handle sql type %d", msg->sqltype);
				break;
		}

		ReleaseCurrentSubTransaction();
		MemoryContextSwitchTo(oldcontext);
		CurrentResourceOwner = oldowner;
	}
	PG_CATCH();
	{
		/* Make sure the memroy context is in correct position */
		MemoryContextSwitchTo(oldcontext);
		RollbackAndReleaseCurrentSubTransaction();
		MemoryContextSwitchTo(oldcontext);
		CurrentResourceOwner = oldowner;
		
		PG_RE_THROW();
	}
	PG_END_TRY();

	return result;
}


