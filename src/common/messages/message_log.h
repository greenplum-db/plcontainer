/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016, Pivotal.
 *
 *------------------------------------------------------------------------------
 */
#ifndef PLC_MESSAGE_LOG_H
#define PLC_MESSAGE_LOG_H

#include "message_base.h"

typedef struct plcMsgLog {
	base_message_content;
	int32 level;
	char *message;
} plcMsgLog;

#endif /* PLC_MESSAGE_LOG_H */
