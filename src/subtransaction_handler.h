/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */
#ifndef PLC_SUBTRANSACTION_HANDLER_H_
#define PLC_SUBTRANSACTION_HANDLER_H_

#include "common/messages/messages.h"
#include "message_fns.h"

extern void plcontainer_abort_open_subtransactions(int save_subxact_level);

extern void plcontainer_process_subtransaction(plcMsgSubtransaction *msg, plcConn *conn);

#endif /* PLC_SUBTRANSACTION_HANDLER_H_ */
