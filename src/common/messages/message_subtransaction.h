/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#ifndef PLC_MESSAGE_SUBTRANSACTION_H_
#define PLC_MESSAGE_SUBTRANSACTION_H_

#include "message_base.h"

enum SUBTRANSACTION_RETURN_TYPE{
	NO_SUBTRANSACTION_ERROR = 0,
	CREATE_SUBTRANSACTION_ERROR,
	RELEASE_SUBTRANSACTION_ERROR,
	SUCCESS,
};

typedef struct plcMsgSubtransaction {
	base_message_content;
	char         action;  /* subtransaction action, 'n' for enter and 'x' for exit */
	/* Subtransaction exception type, 'e' for Py_None
	 * If no exception we just commit the transaction, or we need to do rollback.
	 * */
	char         type;
} plcMsgSubtransaction;

typedef struct plcMsgSubtransactionResult {
	base_message_content;
	int16          result;    /* subtransaction execute result on QE side*/
} plcMsgSubtransactionResult;

#endif /* PLC_MESSAGE_SUBTRANSACTION_H_ */
