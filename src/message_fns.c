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
#include "executor/spi.h"
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

static void fill_callreq_arguments(FunctionCallInfo fcinfo, plcProcInfo *pinfo, plcMsgCallreq *req);

plcProcInfo *get_proc_info(FunctionCallInfo fcinfo) {
	int len;
	Datum *argnames = NULL;
	bool *argnulls = NULL;
	Datum argnamesArray;
	Datum srcdatum, namedatum;
	bool isnull;
	Oid procoid;
	Form_pg_proc procTup;
	HeapTuple procHeapTup,
		textHeapTup = NULL;
	Form_pg_type typeTup;
	plcProcInfo *pinfo = NULL;
	Form_pg_proc procStruct;

	procoid = fcinfo->flinfo->fn_oid;
	procHeapTup = SearchSysCache(PROCOID, procoid, 0, 0, 0);
	if (!HeapTupleIsValid(procHeapTup)) {
		plc_elog(ERROR, "cannot find proc with oid %u", procoid);
	}

	pinfo = function_cache_get(procoid);
	/*
	 * All the catalog operations are done only if the cached function
	 * information has changed in the catalog
	 */
	if (!plc_procedure_valid(pinfo, procHeapTup)) {

		/*
		 * Here we are using plc_top_alloc as the function structure should be
		 * available across the function handler call
		 *
		 * Note: we free the procedure from within function_put_cache below
		 */
		pinfo = plc_top_alloc(sizeof(plcProcInfo));
		if (pinfo == NULL) {
			plc_elog(FATAL, "Cannot allocate memory for plcProcInfo structure");
		}
		/* Remember transactional information to allow caching */
		pinfo->funcOid = procoid;
		pinfo->fn_xmin = HeapTupleHeaderGetXmin(procHeapTup->t_data);
		pinfo->fn_tid = procHeapTup->t_self;
		pinfo->retset = fcinfo->flinfo->fn_retset;
		procStruct = (Form_pg_proc) GETSTRUCT(procHeapTup);
		pinfo->fn_readonly = (procStruct->provolatile != PROVOLATILE_VOLATILE);
		pinfo->hasChanged = 1;

		procTup = (Form_pg_proc) GETSTRUCT(procHeapTup);
		fill_type_info(fcinfo, procTup->prorettype, &pinfo->rettype);

		pinfo->nargs = procTup->pronargs;
		if (pinfo->nargs > 0) {
			// This is required to avoid the cycle from being removed by optimizer
			int volatile j;

			pinfo->argtypes = plc_top_alloc(pinfo->nargs * sizeof(plcTypeInfo));
			for (j = 0; j < pinfo->nargs; j++) {
				fill_type_info(fcinfo, procTup->proargtypes.values[j], &pinfo->argtypes[j]);
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
				                  &argnames, &argnulls, &len);
				if (len != pinfo->nargs) {
					plc_elog(FATAL, "something bad happened, nargs (%d) != len (%d)",
					     pinfo->nargs, len);
				}
			}

			pinfo->argnames = plc_top_alloc(pinfo->nargs * sizeof(char *));
			for (j = 0; j < pinfo->nargs; j++) {
				if (!isnull && !argnulls[j]) {
					pinfo->argnames[j] =
						plc_top_strdup(DatumGetCString(
							DirectFunctionCall1(textout, argnames[j])
						));
					if (strlen(pinfo->argnames[j]) == 0) {
						pfree(pinfo->argnames[j]);
						pinfo->argnames[j] = NULL;
					}
				} else {
					pinfo->argnames[j] = NULL;
				}
			}

			if (textHeapTup != NULL) {
				ReleaseSysCache(textHeapTup);
			}
		} else {
			pinfo->argtypes = NULL;
			pinfo->argnames = NULL;
		}

		/* Get the text and name of the function */
		srcdatum = SysCacheGetAttr(PROCOID, procHeapTup, Anum_pg_proc_prosrc, &isnull);
		if (isnull)
			plc_elog(ERROR, "null prosrc");
		pinfo->src = plc_top_strdup(DatumGetCString(DirectFunctionCall1(textout, srcdatum)));
		namedatum = SysCacheGetAttr(PROCOID, procHeapTup, Anum_pg_proc_proname, &isnull);
		if (isnull)
			plc_elog(ERROR, "null proname");
		pinfo->name = plc_top_strdup(DatumGetCString(DirectFunctionCall1(nameout, namedatum)));

		/* Cache the function for later use */
		function_cache_put(pinfo);
	} else {
		pinfo->hasChanged = 0;
	}
	ReleaseSysCache(procHeapTup);
	return pinfo;
}

void free_proc_info(plcProcInfo *proc) {
	int i;
	for (i = 0; i < proc->nargs; i++) {
		if (proc->argnames[i] != NULL) {
			pfree(proc->argnames[i]);
		}
		free_type_info(&proc->argtypes[i]);
	}
	if (proc->nargs > 0) {
		pfree(proc->argnames);
		pfree(proc->argtypes);
	}
	pfree(proc);
}

plcMsgCallreq *plcontainer_create_call(FunctionCallInfo fcinfo, plcProcInfo *pinfo) {
	plcMsgCallreq *req;

	req = pmalloc(sizeof(plcMsgCallreq));
	req->msgtype = MT_CALLREQ;
	req->proc.name = pinfo->name;
	req->proc.src = pinfo->src;
	req->logLevel = log_min_messages;
	req->objectid = pinfo->funcOid;
	req->hasChanged = pinfo->hasChanged;
	copy_type_info(&req->retType, &pinfo->rettype);

	fill_callreq_arguments(fcinfo, pinfo, req);

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
				valid = plc_type_valid(&proc->argtypes[i]);
			}

			// Also check for composite output type
			if (valid) {
				valid = plc_type_valid(&proc->rettype);
			}
		}
	}
	return valid;
}

static void fill_callreq_arguments(FunctionCallInfo fcinfo, plcProcInfo *pinfo, plcMsgCallreq *req) {
	int i;

	req->nargs = pinfo->nargs;
	req->retset = pinfo->retset;
	req->args = pmalloc(sizeof(*req->args) * pinfo->nargs);

	for (i = 0; i < pinfo->nargs; i++) {
		req->args[i].name = pinfo->argnames[i];
		copy_type_info(&req->args[i].type, &pinfo->argtypes[i]);

		if (fcinfo->argnull[i]) {
			req->args[i].data.isnull = 1;
			req->args[i].data.value = NULL;
		} else {
			req->args[i].data.isnull = 0;
			req->args[i].data.value = pinfo->argtypes[i].outfunc(fcinfo->arg[i], &pinfo->argtypes[i]);
		}
	}
}
