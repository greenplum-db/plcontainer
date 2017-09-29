/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */
#ifndef PLC_MESSAGE_SQL_H
#define PLC_MESSAGE_SQL_H

#include "message_base.h"

typedef enum {
    SQL_TYPE_INVALID = 0,
    SQL_TYPE_STATEMENT,
    SQL_TYPE_CURSOR_CLOSE,
    SQL_TYPE_FETCH,
    SQL_TYPE_CURSOR_OPEN,
    SQL_TYPE_PREPARE,
    SQL_TYPE_PEXECUTE,
    SQL_TYPE_UNPREPARE,
    SQL_TYPE_MAX
} plcSqlType;

typedef struct plcMsgSQL {
    base_message_content;
    plcSqlType    sqltype;
	long long     limit;        /* For sqi execcute . */
	char         *argtypes;     /* For prepare */
	plcArgument  *args;         /* For execute_plan */
	void         *plan;         /* For prepare and execute_plan */
    char         *statement;
	int           nargs;        /* For prepare and execute_plan */
} plcMsgSQL;

#endif /* PLC_MESSAGE_SQL_H */
