/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016, Pivotal.
 *
 *------------------------------------------------------------------------------
 */
#ifndef PLC_MESSAGE_ERROR_H
#define PLC_MESSAGE_ERROR_H

#include "message_base.h"

typedef struct plcMsgError {
	base_message_content;
	char *message;
	char *stacktrace;
} plcMsgError;

void free_error(plcMsgError *msg);

#endif /* PLC_MESSAGE_ERROR_H */
