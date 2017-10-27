/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */
#ifndef PLC_MESSAGE_RAW_H
#define PLC_MESSAGE_RAW_H

#include "message_base.h"

typedef struct plcMsgRaw {
    base_message_content;
	int32 size;
    char *data;
} plcMsgRaw;

void free_rawmsg(plcMsgRaw *msg);

#endif /* PLC_MESSAGE_RAW_H */
