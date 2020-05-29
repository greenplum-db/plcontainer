/*
* Portions Copyright 1994-2004 The PL-J Project. All rights reserved.
* Portions Copyright © 2016-Present Pivotal Software, Inc.
*/


/* Postgres Headers */
#include "postgres.h"
#include "utils/builtins.h"
#include "utils/syscache.h"
#include "catalog/pg_proc.h"
#ifdef PLC_PG
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include "commands/trigger.h"
#include "executor/spi.h"
#ifdef PLC_PG
#pragma GCC diagnostic pop
#endif

#include "storage/ipc.h"
#include "funcapi.h"
#include "miscadmin.h"
#ifndef PLC_PG
  #include "utils/faultinjector.h"
#endif
#include "utils/memutils.h"
#include "utils/guc.h"
/* PLContainer Headers */
#include "common/comm_dummy.h"
#include "common/messages/messages.h"
#include "plc/containers.h"
#include "plc/message_fns.h"
#include "plc/plcontainer.h"
#include "plc/plc_configuration.h"
#include "plc/plc_typeio.h"

#include "interface.h"

PG_MODULE_MAGIC;

/* exported functions */
Datum plcontainer_validator(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(plcontainer_validator);

PG_FUNCTION_INFO_V1(plcontainer_call_handler);

PG_FUNCTION_INFO_V1(plcontainer_inline_handler);

static void plpython_error_callback(void *arg);

static char * PLy_procedure_name(plcProcInfo *proc);

/*
 * Currently active plpython function
 */
static plcProcInfo *PLy_curr_procedure = NULL;

//static int plcCallRecursiveLevel = 0;         // TODO will be enabled later

/* this is saved and restored by plcontainer_call_handler */
MemoryContext pl_container_caller_context = NULL;
extern char *plcontainer_service_address;
extern int plc_client_timeout;
extern bool is_plcontainer_for_k8s;
void _PG_init(void);

/*
 * _PG_init() - library load-time initialization
 *
 * DO NOT make this static nor change its name!
 */
void
_PG_init(void) {
	DefineCustomStringVariable("plcontainer.service_address",
							   "Set the address of the pl4k service",
							   NULL,
							   &plcontainer_service_address,
							   "none",
							   PGC_SIGHUP,
							   0,
							   NULL,
							   NULL,
							   NULL);
	DefineCustomIntVariable("plcontainer.plc_client_timeout",
							"The plcontainer client timeout for function call",
							NULL,
							&plc_client_timeout,
							60, -1, 3600,
							PGC_USERSET,
							0,
							NULL,
							NULL,
							NULL);

	if (strcmp(plcontainer_service_address, "none") == 0) {
		is_plcontainer_for_k8s = false;
	} else {
		is_plcontainer_for_k8s = true;
	}
	plc_elog(DEBUG1, "PL/Container initialize with address %s, pl4k_mode:%d", plcontainer_service_address, is_plcontainer_for_k8s);
	
	/* Be sure we do initialization only once (should be redundant now) */
	static bool inited = false;
	if (inited)
		return;

	inited = true;
}

static bool
PLy_procedure_is_trigger(Form_pg_proc procStruct)
{
	return (procStruct->prorettype == TRIGGEROID ||
			(procStruct->prorettype == OPAQUEOID &&
			 procStruct->pronargs == 0));
}

Datum
plcontainer_validator(PG_FUNCTION_ARGS)
{
	Oid			funcoid = PG_GETARG_OID(0);
	HeapTuple	tuple;
	Form_pg_proc procStruct;
	bool		is_trigger;

	if (!CheckFunctionValidatorAccess(fcinfo->flinfo->fn_oid, funcoid))
		PG_RETURN_VOID();

	if (!check_function_bodies)
	{
		PG_RETURN_VOID();
	}

	/* Get the new function's pg_proc entry */
	tuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcoid));
	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "cache lookup failed for function %u", funcoid);
	procStruct = (Form_pg_proc) GETSTRUCT(tuple);

	is_trigger = PLy_procedure_is_trigger(procStruct);

	ReleaseSysCache(tuple);

	/* We can't validate triggers against any particular table ... */
	plcontainer_procedure_get(fcinfo);

	PG_RETURN_VOID();
}

/*
 * Get the name of the last procedure called by the backend (the
 * innermost, if a plpython procedure call calls the backend and the
 * backend calls another plpython procedure).
 *
 * NB: this returns the SQL name, not the internal Python procedure name
 */
static char *
PLy_procedure_name(plcProcInfo *proc)
{
	if (proc == NULL)
		return "<unknown procedure>";
	return proc->proname;
}

static void
plpython_error_callback(void __attribute__((__unused__)) *arg)
{
	if (PLy_curr_procedure)
		errcontext("PLContainer function \"%s\"",
				   PLy_procedure_name(PLy_curr_procedure));
}

Datum plcontainer_call_handler(PG_FUNCTION_ARGS) {
	Datum datumreturn = (Datum) 0;
	int ret;
	plcProcInfo *save_curr_proc;
	ErrorContextCallback plerrcontext;

	/* TODO: handle trigger requests as well */
	if (CALLED_AS_TRIGGER(fcinfo)) {
		plc_elog(ERROR, "PL/Container does not support triggers");
		return datumreturn;
	}

	/* pl_container_caller_context refer to the CurrentMemoryContext(e.g. ExprContext)
	 * since SPI_connect() will switch memory context to SPI_PROC, we need
	 * to switch back to the pl_container_caller_context at plcontainer_get_result*/
	pl_container_caller_context = CurrentMemoryContext;

	ret = SPI_connect();
	if (ret != SPI_OK_CONNECT)
		plc_elog(ERROR, "[plcontainer] SPI connect error: %d (%s)", ret,
		     SPI_result_code_string(ret));


	plc_elog(DEBUG1, "Entering call handler with  PLy_curr_procedure");

	save_curr_proc = PLy_curr_procedure;
	/*
	 * Setup error traceback support for ereport()
	 */
	plerrcontext.callback = plpython_error_callback;
	plerrcontext.previous = error_context_stack;
	error_context_stack = &plerrcontext;

	/* We need to cover this in try-catch block to catch the even of user
	 * requesting the query termination. In this case we should forcefully
	 * kill the container and reset its information
	 */
	PG_TRY();
	{


		plcProcInfo *proc;
		/*TODO By default we return NULL */
		fcinfo->isnull = true;

		/* Get procedure info from cache or compose it based on catalog */
		proc = plcontainer_procedure_get(fcinfo);

		PLy_curr_procedure = proc;

		plc_elog(DEBUG1, "Calling python proc @ address: %p", proc);

		datumreturn = plcontainer_function_handler(fcinfo, proc, pl_container_caller_context);
	}
	PG_CATCH();
	{
		reset_containers();
		/* If the reason is Cancel or Termination or Backend error. */
		if (InterruptPending || QueryCancelPending || QueryFinishPending) {
			plc_elog(DEBUG1, "Terminating containers due to user request reason("
				"Flags for debugging: %d %d %d", InterruptPending,
			     QueryCancelPending, QueryFinishPending);
		}
		error_context_stack = plerrcontext.previous;
		PLy_curr_procedure = save_curr_proc;
		PG_RE_THROW();
	}
	PG_END_TRY();

	/**
	 *  SPI_finish() will clear the old memory context. Upstream code place it at earlier
	 *  part of code, but we need to place it here.
	 */
	ret = SPI_finish();
	if (ret != SPI_OK_FINISH)
		plc_elog(ERROR, "[plcontainer] SPI finish error: %d (%s)", ret,
		     SPI_result_code_string(ret));

	/* Pop the error context stack */
	error_context_stack = plerrcontext.previous;

	PLy_curr_procedure = save_curr_proc;


	return datumreturn;
}

Datum plcontainer_inline_handler(PG_FUNCTION_ARGS)
{
	int ret;
	ErrorContextCallback plerrcontext;

	/* pl_container_caller_context refer to the CurrentMemoryContext(e.g. ExprContext)
	 * since SPI_connect() will switch memory context to SPI_PROC, we need
	 * to switch back to the pl_container_caller_context at plcontainer_inline_functin_hanndler*/
	pl_container_caller_context = CurrentMemoryContext;

	ret = SPI_connect();
	if (ret != SPI_OK_CONNECT)
		plc_elog(ERROR, "[plcontainer] SPI connect error: %d (%s)", ret,
			SPI_result_code_string(ret));

	/*
	 * Setup error traceback support for ereport()
	 */
	plerrcontext.callback = plpython_error_callback;
	plerrcontext.previous = error_context_stack;
	error_context_stack = &plerrcontext;

	/* We need to cover this in try-catch block to catch the even of user
	 * requesting the query termination. In this case we should forcefully
	 * kill the container and reset its information
	 */
	PG_TRY();
	{
		plcontainer_inline_function_handler(fcinfo, pl_container_caller_context);
	}
	PG_CATCH();
	{
		reset_containers();
		/* If the reason is Cancel or Termination or Backend error. */
		if (InterruptPending || QueryCancelPending || QueryFinishPending) {
			plc_elog(DEBUG1, "Terminating containers due to user request reason("
				"Flags for debugging: %d %d %d", InterruptPending,
				QueryCancelPending, QueryFinishPending);
		}
		error_context_stack = plerrcontext.previous;
		PG_RE_THROW();
	}
	PG_END_TRY();

	/**
	 *  SPI_finish() will clear the old memory context. Upstream code place it at earlier
	 *  part of code, but we need to place it here.
	 */
	ret = SPI_finish();
	if (ret != SPI_OK_FINISH)
		plc_elog(ERROR, "[plcontainer] SPI finish error: %d (%s)", ret,
			SPI_result_code_string(ret));

	/* Pop the error context stack */
	error_context_stack = plerrcontext.previous;

	PG_RETURN_VOID();
}

