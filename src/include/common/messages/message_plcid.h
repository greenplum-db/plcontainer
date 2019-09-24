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
	int ccnt;
	int action;
	char *runtimeid;
} plcMsgPLCId;

typedef struct plcMsgContainer {
	base_message_content;
	int status;
	char *msg;
} plcMsgContainer;
#endif /* PLC_MESSAGE_PLCID_H */
