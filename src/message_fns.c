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


/**
 * file name:        plj-callmkr.c
 * description:        PL/pgJ call message creator routine. This file
 *                    was renamed from plpgj_call_maker.c
 *                    It is replaceable with pljava-way of declaring java
 *                    method calls. (read the readme!)
 * author:        Laszlo Hornyak
 */

/*
 * Portions Copyright © 2016-Present Pivotal Software, Inc.
 */

/* Greenplum headers */
#include "postgres.h"

#ifdef PLC_PG
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include "executor/spi.h"
#ifdef PLC_PG
#pragma GCC diagnostic pop
#endif

#include "access/transam.h"
#include "catalog/pg_proc.h"
#include "utils/syscache.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/guc.h"

/* message and function definitions */
#include "common/comm_utils.h"
#include "common/messages/messages.h"
#include "message_fns.h"
#include "function_cache.h"
#include "plc_typeio.h"

#ifdef PLC_PG
  #include "catalog/pg_type.h"
  #include "access/htup_details.h"
#endif

static bool plc_procedure_valid(plcProcInfo *proc, HeapTuple procTup);

static bool plc_type_valid(plcTypeInfo *type);

static void fill_callreq_arguments(FunctionCallInfo fcinfo, plcProcInfo *proc, plcMsgCallreq *req);

plcProcInfo *plcontainer_procedure_get(FunctionCallInfo fcinfo) {
	int lenOfArgnames;
	Datum *argnames = NULL;
	bool *argnulls = NULL;
	Datum argnamesArray;
	Datum srcdatum, namedatum;
	Oid procoid;
	HeapTuple procHeapTup,
		textHeapTup = NULL;
	Form_pg_type typeTup;
	plcProcInfo * volatile proc = NULL;


	procoid = fcinfo->flinfo->fn_oid;
	procHeapTup = SearchSysCache(PROCOID, procoid, 0, 0, 0);
	if (!HeapTupleIsValid(procHeapTup)) {
		plc_elog(ERROR, "cannot find proc with oid %u", procoid);
	}

	proc = function_cache_get(procoid);
	/*
	 * All the catalog operations are done only if the cached function
	 * information has changed in the catalog
	 */
	if (!plc_procedure_valid(proc, procHeapTup)) {

		char procName[NAMEDATALEN + 256];
		Form_pg_proc procStruct;
		bool isnull;
		int rv;

		procStruct = (Form_pg_proc) GETSTRUCT(procHeapTup);
		rv = snprintf(procName, sizeof(procName), "__plpython_procedure_%s_%u",
				NameStr(procStruct->proname), procoid/*TODOfn_oid*/);
		if (rv < 0 || (unsigned int)rv >= sizeof(procName))
			elog(ERROR, "procedure name would overrun buffer");

		/*
		 * Here we are using plc_top_alloc as the function structure should be
		 * available across the function handler call
		 *
		 * Note: we free the procedure from within function_put_cache below
		 */
		proc = PLy_malloc(sizeof(plcProcInfo));
		if (proc == NULL) {
			plc_elog(FATAL, "Cannot allocate memory for plcProcInfo structure");
		}

		proc->proname = PLy_strdup(NameStr(procStruct->proname));
		proc->pyname = PLy_strdup(procName);
		proc->funcOid = procoid;
		proc->fn_xmin = HeapTupleHeaderGetXmin(procHeapTup->t_data);
		proc->fn_tid = procHeapTup->t_self;
		/* Remember if function is STABLE/IMMUTABLE */
		proc->fn_readonly = (procStruct->provolatile != PROVOLATILE_VOLATILE);

		proc->retset = fcinfo->flinfo->fn_retset;

		proc->hasChanged = 1;

		procStruct = (Form_pg_proc) GETSTRUCT(procHeapTup);
		fill_type_info(fcinfo, procStruct->prorettype, &proc->result);

		proc->nargs = procStruct->pronargs;
		if (proc->nargs > 0) {
			// This is required to avoid the cycle from being removed by optimizer
			int volatile j;

			proc->args = PLy_malloc(proc->nargs * sizeof(plcTypeInfo));
			for (j = 0; j < proc->nargs; j++) {
				fill_type_info(fcinfo, procStruct->proargtypes.values[j], &proc->args[j]);
			}

			argnamesArray = SysCacheGetAttr(PROCOID, procHeapTup,
			                                Anum_pg_proc_proargnames, &isnull);
			/* If at least some arguments have names */
			if (!isnull) {
				textHeapTup = SearchSysCache(TYPEOID, ObjectIdGetDatum(TEXTOID), 0, 0, 0);
				if (!HeapTupleIsValid(textHeapTup)) {
					plc_elog(FATAL, "cannot find text type in cache");
				}
				typeTup = (Form_pg_type) GETSTRUCT(textHeapTup);
				deconstruct_array(DatumGetArrayTypeP(argnamesArray), TEXTOID,
				                  typeTup->typlen, typeTup->typbyval, typeTup->typalign,
				                  &argnames, &argnulls, &lenOfArgnames);
				/* UDF may contain OUT parameter, which is not considered as 
				 * arguement number. So the length of argname list(container both INPUT and OUTPUT) 
				 * maybe smaller than arguement number. There is no need to pass OUTPUT name to container.
				 */
				if (lenOfArgnames < proc->nargs) {
					plc_elog(ERROR, "Length of argname list(%d) should be equal to or larger than \
							number of args(%d)", lenOfArgnames, proc->nargs);
				}
			}

			proc->argnames = PLy_malloc(proc->nargs * sizeof(char *));
			for (j = 0; j < proc->nargs; j++) {
				if (!isnull && !argnulls[j]) {
					proc->argnames[j] =
						plc_top_strdup(DatumGetCString(
							DirectFunctionCall1(textout, argnames[j])
						));
					if (strlen(proc->argnames[j]) == 0) {
						pfree(proc->argnames[j]);
						proc->argnames[j] = NULL;
					}
				} else {
					proc->argnames[j] = NULL;
				}
			}

			if (textHeapTup != NULL) {
				ReleaseSysCache(textHeapTup);
			}
		} else {
			proc->args = NULL;
			proc->argnames = NULL;
		}

		/* Get the text and name of the function */
		srcdatum = SysCacheGetAttr(PROCOID, procHeapTup, Anum_pg_proc_prosrc, &isnull);
		if (isnull)
			plc_elog(ERROR, "null prosrc");
		proc->src = plc_top_strdup(DatumGetCString(DirectFunctionCall1(textout, srcdatum)));
		namedatum = SysCacheGetAttr(PROCOID, procHeapTup, Anum_pg_proc_proname, &isnull);
		if (isnull)
			plc_elog(ERROR, "null proname");
		proc->name = plc_top_strdup(DatumGetCString(DirectFunctionCall1(nameout, namedatum)));

		/* Cache the function for later use */
		function_cache_put(proc);
	} else {
		proc->hasChanged = 0;
	}
	ReleaseSysCache(procHeapTup);
	return proc;
}

void free_proc_info(plcProcInfo *proc) {
	int i;
	for (i = 0; i < proc->nargs; i++) {
		if (proc->argnames[i] != NULL) {
			pfree(proc->argnames[i]);
		}
		free_type_info(&proc->args[i]);
	}
	if (proc->nargs > 0) {
		pfree(proc->argnames);
		pfree(proc->args);
	}
	pfree(proc);
}

plcMsgCallreq *plcontainer_generate_call_request(FunctionCallInfo fcinfo, plcProcInfo *proc) {
	plcMsgCallreq *req;

	req = pmalloc(sizeof(plcMsgCallreq));
	req->msgtype = MT_CALLREQ;
	req->proc.name = proc->name;
	req->proc.src = proc->src;
	req->logLevel = log_min_messages;
	req->objectid = proc->funcOid;
	req->hasChanged = proc->hasChanged;
	copy_type_info(&req->retType, &proc->result);

	fill_callreq_arguments(fcinfo, proc, req);

	return req;
}

static bool plc_type_valid(plcTypeInfo *type) {
	bool valid = true;
	int i;

	for (i = 0; i < type->nSubTypes && valid; i++) {
		valid = plc_type_valid(&type->subTypes[i]);
	}

	/* We exclude record from testing here, as it would change only if the function
	 * changes itself, which would be caugth by checking function creation xid */
	if (valid && type->is_rowtype && !type->is_record) {
		HeapTuple relTup;

		Assert(OidIsValid(type->typ_relid));
		Assert(TransactionIdIsValid(type->typrel_xmin));
		Assert(ItemPointerIsValid(&type->typrel_tid));

		// Get the pg_class tuple for the argument type
		relTup = SearchSysCache1(RELOID, ObjectIdGetDatum(type->typ_relid));
		if (!HeapTupleIsValid(relTup))
			plc_elog(ERROR, "PL/Container cache lookup failed for relation %u", type->typ_relid);

		// If commit transaction ID has changed or relation was moved within table
		// our type information is no longer valid
		if (type->typrel_xmin != HeapTupleHeaderGetXmin(relTup->t_data) ||
		    !ItemPointerEquals(&type->typrel_tid, &relTup->t_self)) {
			valid = false;
		}

		ReleaseSysCache(relTup);
	}

	return valid;
}

/*
 * Decide whether a cached PLyProcedure struct is still valid
 */
static bool plc_procedure_valid(plcProcInfo *proc, HeapTuple procTup) {
	bool valid = false;

	if (proc != NULL) {
		/* If the pg_proc tuple has changed, it's not valid */
		if (proc->fn_xmin == HeapTupleHeaderGetXmin(procTup->t_data) &&
		    ItemPointerEquals(&proc->fn_tid, &procTup->t_self)) {
			int i;

			valid = true;

			// If there are composite input arguments, they might have changed
			for (i = 0; i < proc->nargs && valid; i++) {
				valid = plc_type_valid(&proc->args[i]);
			}

			// Also check for composite output type
			if (valid) {
				valid = plc_type_valid(&proc->result);
			}
		}
	}
	return valid;
}

static void fill_callreq_arguments(FunctionCallInfo fcinfo, plcProcInfo *proc, plcMsgCallreq *req) {
	int i;

	req->nargs = proc->nargs;
	req->retset = proc->retset;
	req->args = pmalloc(sizeof(*req->args) * proc->nargs);

	for (i = 0; i < proc->nargs; i++) {
		req->args[i].name = proc->argnames[i];
		copy_type_info(&req->args[i].type, &proc->args[i]);

		if (fcinfo->argnull[i]) {
			req->args[i].data.isnull = 1;
			req->args[i].data.value = NULL;
		} else {
			req->args[i].data.isnull = 0;
			req->args[i].data.value = proc->args[i].outfunc(fcinfo->arg[i], &proc->args[i]);
		}
	}
}
