/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */
#ifndef PLC_MESSAGE_QUOTE_H
#define PLC_MESSAGE_QUOTE_H

#include "message_base.h"

typedef enum {
	QUOTE_TYPE_LITERAL = 0,
	QUOTE_TYPE_NULLABLE,
	QUOTE_TYPE_IDENT
} plcQuoteType;

typedef struct plcMsgQuote {
	base_message_content;
	plcQuoteType quote_type;
	char * msg;
} plcMsgQuote;

typedef struct plcMsgQuoteResult {
	base_message_content;
	plcQuoteType quote_type;
	char *result;
} plcMsgQuoteResult;

#endif /* PLC_MESSAGE_QUOTE_H */
