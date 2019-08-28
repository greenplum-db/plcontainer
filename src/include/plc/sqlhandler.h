/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */


#ifndef PLC_SQLHANDLER_H
#define PLC_SQLHANDLER_H

#include "common/messages/messages.h"
#include "plc/message_fns.h"
#include "executor/spi.h"

typedef struct plcPlan {
	Oid *argOids;
	SPIPlanPtr plan;
	int nargs;
} plcPlan;

plcMessage *handle_sql_message(plcMsgSQL *msg, plcContext *ctx, plcProcInfo *pinfo);



#endif /* PLC_SQLHANDLER_H */
