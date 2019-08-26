/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016, Pivotal.
 *
 *------------------------------------------------------------------------------
 */
#ifndef PLC_MESSAGE_CALLREQ_H
#define PLC_MESSAGE_CALLREQ_H

#include "message_base.h"

typedef struct {
	char *src;  // source code of the procedure
	char *name; // name of procedure
} plcProcSrc;

typedef struct plcMsgCallreq {
	base_message_content;    // message_type ID
	uint32_t objectid;   // OID of the function in GPDB
	int32_t hasChanged; // flag signaling the function has changed in GPDB
	plcProcSrc proc;       // procedure - its name and source code
	int32_t logLevel;      // log level at client side
	plcType retType;    // function return type
	int32_t retset;     // whether the function is set-returning
	int32_t nargs;      // number of function arguments
	char *serverenc; //db_encoding
	plcArgument *args;       // function arguments
} plcMsgCallreq;

void free_arguments(plcArgument *args, int nargs, bool isShared, bool isSender);

/*
  Frees a callreq and all subfields of the struct, this function
  assumes ownership of all pointers in the struct and substructs
*/
void free_callreq(plcMsgCallreq *req, bool isShared, bool isSender);

#endif /* PLC_MESSAGE_CALLREQ_H */
