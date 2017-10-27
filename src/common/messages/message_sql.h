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
	int64         limit;        /* For execute_query and execute_plan */
	plcArgument  *args;         /* For prepare and execute_plan */
	plcDatatype  *argtypes;     /* For prepare */
	void         *pplan;        /* For prepare and execute_plan. pointer to plan */
	char         *statement;    /* For prepare and execute_query/execute_plan */
	int32         nargs;        /* For prepare and execute_plan */
} plcMsgSQL;

#endif /* PLC_MESSAGE_SQL_H */
