/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016, Pivotal.
 *
 *------------------------------------------------------------------------------
 */
#ifndef PLC_MESSAGE_RESULT_H
#define PLC_MESSAGE_RESULT_H

#include "message_base.h"

typedef struct plcMsgResult {
	base_message_content;
	uint32 rows;
	uint32 cols;
	plcType *types;
	char **names;
	rawdata **data;

	/*
	 * Callback called from message sending function to return the error message
	 * generated during the period engine could not send it
	 */
	void *(*exception_callback)(void);
} plcMsgResult;

void free_result(plcMsgResult *res, bool isSender);

#endif /* PLC_MESSAGE_RESULT_H */
