

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

#include "common/comm_utils.h"
#include "common/comm_channel.h"
#include "plc_typeio.h"
#include "sqlhandler.h"

static plcMsgResult *create_sql_result(void);
static plcMsgRaw *create_prepare_result(int64 pplan, plcDatatype *type, int nargs);

static plcMsgResult *create_sql_result() {
    plcMsgResult  *result;
    int            i, j;
    plcTypeInfo   *resTypes;

    result          = palloc(sizeof(plcMsgResult));
    result->msgtype = MT_RESULT;
    result->cols    = SPI_tuptable->tupdesc->natts;
    result->rows    = SPI_processed;
    result->types   = palloc(result->cols * sizeof(*result->types));
    result->names   = palloc(result->cols * sizeof(*result->names));
    result->exception_callback = NULL;
    resTypes        = palloc(result->cols * sizeof(plcTypeInfo));
    for (j = 0; j < result->cols; j++) {
        fill_type_info(NULL, SPI_tuptable->tupdesc->attrs[j]->atttypid, &resTypes[j]);
        copy_type_info(&result->types[j], &resTypes[j]);
        result->names[j] = SPI_fname(SPI_tuptable->tupdesc, j + 1);
    }

    if (result->rows == 0) {
        result->data = NULL;
    } else {
        bool  isnull;
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

    result          = palloc(sizeof(plcMsgRaw));
    result->msgtype = MT_RAW;
    result->size    = sizeof(int32) + sizeof(int64) + sizeof(int32) + nargs * sizeof(plcDatatype);
	result->data    = pmalloc(result->size);

	offset = 0;
	/* We need to transfer the state of plan (i.e. valid or not). */
	*((int32 *) (result->data + offset)) = !!(*(int64 **)pplan); offset += sizeof(int32);
	*((int64 *) (result->data + offset)) = pplan; offset += sizeof(int64);
	*((int32 *)(result->data + offset)) = nargs; offset += sizeof(int32);
	if (nargs > 0)
		memcpy(result->data + offset, type, nargs * sizeof(plcDatatype));

    return result;
}

static plcMsgRaw *create_unprepare_result(int32 retval) {
    plcMsgRaw *result;

    result          = palloc(sizeof(plcMsgRaw));
    result->msgtype = MT_RAW;
    result->size    = sizeof(int32);
	result->data    = pmalloc(result->size);

	*((int32 *) result->data) = retval;

    return result;
}

plcMessage *handle_sql_message(plcMsgSQL *msg, plcProcInfo *pinfo) {
    int           i, retval;
    plcMessage   *result = NULL;
	int64         *tmpplan;
	plcPlan      *plc_plan;
	Oid          type_oid;
	plcDatatype *argTypes;
	int32        typemod;

    PG_TRY();
    {
        BeginInternalSubTransaction(NULL);

		switch (msg->sqltype) {
		case SQL_TYPE_STATEMENT:
		case SQL_TYPE_PEXECUTE:
			if (msg->sqltype == SQL_TYPE_PEXECUTE) {
				char        *nulls;
				Datum       *values;
				plcTypeInfo *pexecType;

				/* FIXME: Sanity-check is needed!
				 * Maybe hash-store plan pointers for quick search?
				 * Or use array since we need to free all plans when backend quits.
				 * Or both?
				 */
				plc_plan = (plcPlan *) ((char *) msg->pplan - offsetof(plcPlan, plan));
				if (plc_plan->nargs != msg->nargs) {
					elog(ERROR, "argument number wrong for execute with plan: "
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
						nulls[i] = 'n';
					} else {
						/* A bit heavy to populate plcTypeInfo. */
						fill_type_info(NULL, plc_plan->argOids[i], pexecType);
						values[i] = pexecType->infunc(msg->args[i].data.value, pexecType);
						nulls[i] = ' ';
					}
				}

				retval = SPI_execute_plan((SPIPlanPtr) plc_plan->plan, values, nulls,
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
				result = (plcMessage*)create_sql_result();
				break;
			default:
				elog(ERROR, "Cannot handle sql ('%s') with fn_readonly (%d) "
					 "and limit (%lld). Returns %d", msg->statement,
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
							lprintf(ERROR, "unable to parse the given OID %s", msg->args[i].type.typeName);
					}
				} else {
						lprintf(ERROR, "prepare type is bad, unexpected prepare sql type %d",
						        msg->args[i].type.type);
				}
				argTypes[i] = plc_get_datatype_from_oid(plc_plan->argOids[i]);
			}
			plc_plan->nargs = msg->nargs;

			plc_plan->plan = (int64 *) SPI_prepare(msg->statement, plc_plan->nargs, plc_plan->argOids);
			/* plan needs to survive cross memory context. */
			tmpplan = plc_plan->plan;
			plc_plan->plan = (int64 *) SPI_saveplan((SPIPlanPtr) tmpplan);
			SPI_freeplan((SPIPlanPtr) tmpplan);

			/* We just send the plan pointer only. Save Oids for execute. */
			if (plc_plan->plan == NULL) {
				/* Log the prepare failure but let the backend handle. */
				elog(LOG, "SPI_prepare() fails for '%s', with %d arguments: %s",
				     msg->statement, plc_plan->nargs, SPI_result_code_string(SPI_result));
			}
			result = (plcMessage *) create_prepare_result((int64) &plc_plan->plan, argTypes, plc_plan->nargs);
			break;
		case SQL_TYPE_UNPREPARE:
			plc_plan = (plcPlan *) ((char *) msg->pplan - offsetof(plcPlan, plan));

			/* FIXME: Sanity check needed. See comment for SQL_TYPE_PEXECUTE. */
			retval = 0;
			if (plc_plan->argOids)
				pfree(plc_plan->argOids);
			if (plc_plan->plan)
				retval = SPI_freeplan((SPIPlanPtr) plc_plan->plan);
			pfree(plc_plan);
			result = (plcMessage *) create_unprepare_result(retval);
			break;
		default:
			elog(ERROR, "Cannot handle sql type %d", msg->sqltype);
			break;
		}

        ReleaseCurrentSubTransaction();
    }
    PG_CATCH();
    {
        RollbackAndReleaseCurrentSubTransaction();
        PG_RE_THROW();
    }
    PG_END_TRY();

    return result;
}
