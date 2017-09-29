/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */
#ifndef PLC_MESSAGE_PREPARE_H
#define PLC_MESSAGE_PREPARE_H

#include "message_base.h"

typedef struct plcMsgPrepare {
    base_message_content;
    void *plan;
} plcMsgPrepare;

#endif /* PLC_MESSAGE_PREPARE_H */
