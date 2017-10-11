

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
    result->size    = sizeof(int64) + sizeof(int32) + nargs * sizeof(plcDatatype);
	result->data    = pmalloc(result->size);

	offset = 0;
	*((int64 *) (result->data + offset)) = pplan; offset += sizeof(int64);
	*((int32 *)(result->data + offset)) = nargs; offset += sizeof(int32);
	if (nargs > 0)
		memcpy(result->data + offset, type, nargs * sizeof(plcDatatype));

    return result;
}

plcMessage *handle_sql_message(plcMsgSQL *msg, plcProcInfo *pinfo) {
    int i, retval;
    plcMessage   *result = NULL;
	plcPlan      *plc_plan;
	Oid type_oid;
	plcDatatype *argTypes = NULL;
	int32 typemod;

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
				plcPlan     *plc_plan;

				/* FIXME: Sanity-check needed! Maybe hash-store plan pointers! */
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
				result = (plcMessage*)create_sql_result();
				break;
			default:
				lprintf(ERROR, "Cannot handle sql ('%s') with fn_readonly (%d) "
						"and limit (%lld). Returns %d", msg->statement,
						pinfo->fn_readonly, msg->limit, retval);
				break;
			}

			SPI_freetuptable(SPI_tuptable);
			break;
		case SQL_TYPE_PREPARE:
			plc_plan = pmalloc(sizeof(plcPlan));
			if (msg->nargs > 0) {
				plc_plan->argOids = pmalloc(msg->nargs * sizeof(Oid));
				argTypes = pmalloc(msg->nargs * sizeof(plcDatatype));
			}
			for (i = 0; i < msg->nargs; i++) {
				if (msg->args[i].type.type != PLC_DATA_TEXT) {
					lprintf(ERROR, "prepare type is bad, expect prepare sql type %d", msg->args[i].type.type);
				}
				parseTypeString(msg->args[i].type.typeName, &type_oid, &typemod);

				plc_plan->argOids[i] = type_oid;
				argTypes[i] = plc_get_datatype_from_oid(type_oid);
			}
			plc_plan->nargs = msg->nargs;
			plc_plan->plan = SPI_prepare(msg->statement, msg->nargs, plc_plan->argOids);

			/* We just send the plan pointer only. Save Oids for execute. */
			if (plc_plan->plan == NULL) {
				/* Log the prepare failure but let the backend handle. */
				lprintf(LOG, "SPI_prepare() fails for '%s', with %d arguments."
					" SPI_result is %d.", msg->statement, msg->nargs, SPI_result);
			}
			result = (plcMessage*) create_prepare_result((int64) &plc_plan->plan, argTypes, msg->nargs);
			break;
		default:
			lprintf(ERROR, "Cannot handle sql type %d", msg->sqltype);
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
