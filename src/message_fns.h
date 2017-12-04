/*
Copyright 1994 The PL-J Project. All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE PL-J PROJECT ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
THE PL-J PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
   OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
   OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those of the authors and should not be
interpreted as representing official policies, either expressed or implied, of the PL-J Project.
*/

#ifndef PLC_MESSAGE_FNS_H
#define PLC_MESSAGE_FNS_H

#include "postgres.h"
#include "fmgr.h"

#include "common/messages/messages.h"
#include "plc_typeio.h"

/* Structure representing function result data */
typedef struct {
	plcMsgResult *resmsg;
	uint32 resrow;
} plcProcResult;

typedef struct {
	/* Greenplum Function Information */
	Oid funcOid;
	TransactionId fn_xmin; /* Transaction ID that created this function in catalog */
	ItemPointerData fn_tid;  /* ItemPointer for the function row in catalog */
	/* Universal Function Information */
	char *name;
	char *src;
	int hasChanged; /* Whether the function has changed since last call */
	plcTypeInfo rettype;
	int retset;
	int nargs;
	char **argnames;
	plcTypeInfo *argtypes;
	bool fn_readonly;
} plcProcInfo;

plcProcInfo *get_proc_info(FunctionCallInfo fcinfo);

void free_proc_info(plcProcInfo *proc);

plcMsgCallreq *plcontainer_create_call(FunctionCallInfo fcinfo, plcProcInfo *pinfo);

#endif /* PLC_MESSAGE_FNS_H */
