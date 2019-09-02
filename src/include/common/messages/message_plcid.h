/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2010 - Present, Pivotal.
 *
 *------------------------------------------------------------------------------
 */
#ifndef PLC_MESSAGE_PLCID_H
#define PLC_MESSAGE_PLCID_H

#include "message_base.h"

typedef struct plcMsgPLCId {
    base_message_content;
    int sessionid;
    int pid;
    char *runtimeid;
} plcMsgPLCId;

#endif /* PLC_MESSAGE_PLCID_H */
