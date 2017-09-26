

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

#include "common/comm_utils.h"
#include "common/comm_channel.h"
#include "plc_typeio.h"
#include "sqlhandler.h"

static plcMsgResult *create_sql_result(void);

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

plcMessage *handle_sql_message(plcMsgSQL *msg, plcProcInfo *pinfo) {
    int retval;
    plcMessage   *result = NULL;

    PG_TRY();
    {
        BeginInternalSubTransaction(NULL);
        retval = SPI_execute(msg->statement, pinfo->fn_readonly, (long) msg->limit);
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
