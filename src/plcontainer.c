/*
* Portions Copyright 1994-2004 The PL-J Project. All rights reserved.
* Portions Copyright © 2016-Present Pivotal Software, Inc.
*/


/* Postgres Headers */
#include "postgres.h"

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
#include "common/comm_channel.h"
#include "common/messages/messages.h"
#include "containers.h"
#include "message_fns.h"
#include "plcontainer.h"
#include "plc_configuration.h"
#include "plc_typeio.h"
#include "sqlhandler.h"
#include "subtransaction_handler.h"

#ifdef PG_MODULE_MAGIC

PG_MODULE_MAGIC;
#endif

#ifdef PLC_PG
    volatile bool QueryFinishPending = false;
#endif

PG_FUNCTION_INFO_V1(plcontainer_call_handler);

static Datum plcontainer_function_handler(FunctionCallInfo fcinfo, plcProcInfo *proc);

static plcProcResult *plcontainer_get_result(FunctionCallInfo fcinfo,
                                             plcProcInfo *proc);

static Datum plcontainer_process_result(FunctionCallInfo fcinfo,
                                        plcProcInfo *proc,
                                        plcProcResult *presult);

static void plcontainer_process_exception(plcMsgError *msg);

static void plcontainer_process_sql(plcMsgSQL *msg, plcConn *conn, plcProcInfo *proc);

static void plcontainer_process_log(plcMsgLog *log);

static volatile bool DeleteBackendsWhenError;

/* this is saved and restored by plcontainer_call_handler */
MemoryContext pl_container_caller_context = NULL;

void _PG_init(void);

static void
plcontainer_cleanup(pg_attribute_unused() int code, pg_attribute_unused() Datum arg) {
	delete_containers();
}

/*
 * _PG_init() - library load-time initialization
 *
 * DO NOT make this static nor change its name!
 */
void
_PG_init(void) {
	/* Be sure we do initialization only once (should be redundant now) */
	static bool inited = false;
	if (inited)
		return;

	on_proc_exit(plcontainer_cleanup, 0);
	explicit_subtransactions = NIL;
	inited = true;
}

Datum plcontainer_call_handler(PG_FUNCTION_ARGS) {
	Datum datumreturn = (Datum) 0;
	int ret;

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

		plc_elog(DEBUG1, "Calling python proc @ address: %p", proc);

		datumreturn = plcontainer_function_handler(fcinfo, proc);
	}
	PG_CATCH();
	{
		/* If the reason is Cancel or Termination or Backend error. */
		if (InterruptPending || QueryCancelPending || QueryFinishPending ||
		    DeleteBackendsWhenError) {
			plc_elog(DEBUG1, "Terminating containers due to user request reason("
				"Flags for debugging: %d %d %d %d", InterruptPending,
			     QueryCancelPending, QueryFinishPending, DeleteBackendsWhenError);
			delete_containers();
			DeleteBackendsWhenError = false;
		}
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

	return datumreturn;
}

static plcProcResult *plcontainer_get_result(FunctionCallInfo fcinfo,
                                             plcProcInfo *proc) {
	char *runtime_id;
	plcConn *conn;
	int message_type;
	plcMsgCallreq *req = NULL;
	plcProcResult *result;
	int volatile save_subxact_level = list_length(explicit_subtransactions);

	PG_TRY();
	{
		runtimeConfEntry *runtime_conf_entry = NULL;
		result = NULL;

		req = plcontainer_generate_call_request(fcinfo, proc);
		runtime_id = parse_container_meta(req->proc.src);
		
		runtime_conf_entry = plc_get_runtime_configuration(runtime_id);

		if (runtime_conf_entry == NULL) {
			plc_elog(ERROR, "Runtime '%s' is not defined in configuration "
						"and cannot be used", runtime_id);
		} 
		/*
		 * We need to check the privilege in each run
		 */
		if (runtime_conf_entry->useUserControl) {
			if (!plc_check_user_privilege(runtime_conf_entry->roles)){
				plc_elog(ERROR, "Current user does not have privilege to use runtime %s", runtime_id);
			}
		}

		conn = get_container_conn(runtime_id);
		if (conn == NULL) {
			/* TODO: We could only remove this backend when error occurs. */
			DeleteBackendsWhenError = true;
			conn = start_backend(runtime_conf_entry);
			DeleteBackendsWhenError = false;
		}

		pfree(runtime_id);

		DeleteBackendsWhenError = true;
		if (conn != NULL) {
			int res;

			res = plcontainer_channel_send(conn, (plcMessage *) req);
#ifndef PLC_PG				
			SIMPLE_FAULT_NAME_INJECTOR("plcontainer_after_send_request");
#endif

			if (res < 0) {
				plc_elog(ERROR, "Error sending data to the client. "
							"Maybe retry later.");
				return NULL;
			}
			free_callreq(req, true, true);

			while (1) {
				plcMessage *answer;

				res = plcontainer_channel_receive(conn, &answer, MT_ALL_BITS);
#ifndef PLC_PG					
				SIMPLE_FAULT_NAME_INJECTOR("plcontainer_after_recv_request");
#endif				
				if (res < 0) {
					plc_elog(ERROR, "Error receiving data from the client. "
								"Maybe retry later.");
					break;
				}

				message_type = answer->msgtype;
				switch (message_type) {
					case MT_RESULT:
						result = (plcProcResult *) pmalloc(sizeof(plcProcResult));
						result->resmsg = (plcMsgResult *) answer;
						result->resrow = 0;
						break;
					case MT_EXCEPTION:
						/* For exception, no need to delete containers. */
						DeleteBackendsWhenError = false;
						plcontainer_process_exception((plcMsgError *) answer);
						break;
					case MT_SQL:
						plcontainer_process_sql((plcMsgSQL *) answer, conn, proc);
						break;
					case MT_LOG:
						plcontainer_process_log((plcMsgLog *) answer);
						break;
					case MT_SUBTRANSACTION:
						plcontainer_process_subtransaction(
								(plcMsgSubtransaction *) answer, conn);
						break;
					default:
						plc_elog(ERROR, "Received unhandled message with type id %d "
								"from client", message_type);
						break;
				}

				if (message_type != MT_SQL && message_type != MT_LOG
				    && message_type != MT_SUBTRANSACTION)
					break;
			}
		} else {
			/* If conn == NULL, it should have longjump-ed earlier. */
			plc_elog(ERROR, "Could not create or connect to container.");
		}
		/*
		 * Since plpy will only let you close subtransactions that you
		 * started, you cannot *unnest* subtransactions, only *nest* them
		 * without closing.
		 */
		Assert(list_length(explicit_subtransactions) >= save_subxact_level);
	}
	PG_CATCH();
	{
		plcontainer_abort_open_subtransactions(save_subxact_level);
		PG_RE_THROW();
	}
	PG_END_TRY();

	plcontainer_abort_open_subtransactions(save_subxact_level);

	DeleteBackendsWhenError = false;
	return result;
}

/*
 * Processing client results message
 */
static Datum plcontainer_process_result(FunctionCallInfo fcinfo,
                                        plcProcInfo *proc,
                                        plcProcResult *presult) {
	Datum result = (Datum) 0;
	plcMsgResult *resmsg = presult->resmsg;

	if (resmsg->cols > 1) {
		plc_elog(ERROR, "Functions returning multiple columns are not supported yet");
		return result;
	}

	if (resmsg->rows == 0) {
		return result;
	}

	if (presult->resrow >= resmsg->rows) {
		ereport(ERROR,
		        (errcode(ERRCODE_CARDINALITY_VIOLATION),
			        errmsg("Trying to access result row %d of the %d-rows result set",
			               presult->resrow, resmsg->rows)));
		return result;
	}

	if (resmsg->data[presult->resrow][0].isnull == 0) {
		fcinfo->isnull = false;
		result = proc->result.infunc(resmsg->data[presult->resrow][0].value, &proc->result);
	}

	return result;
}

/*
 * Processing client log message
 */
static void plcontainer_process_log(plcMsgLog *log) {
	ereport(log->level,
	        (errcode(ERRCODE_NO_DATA),
		        errmsg("%s", log->message)));
	if (log->message != NULL)
		pfree(log->message);
}

/*
 * Processing client SQL query message
 */
static void plcontainer_process_sql(plcMsgSQL *msg, plcConn *conn, plcProcInfo *proc) {
	plcMessage *res;
	volatile MemoryContext oldcontext;
	volatile ResourceOwner oldowner;
	int retval;

	oldcontext = CurrentMemoryContext;
	oldowner = CurrentResourceOwner;

	res = handle_sql_message(msg, conn, proc);
	if (res != NULL) {
		retval = plcontainer_channel_send(conn, res);
		if (retval < 0) {
			plc_elog(ERROR, "Error sending data to the client. "
				"Maybe retry later.");
			return;
		}
		switch (res->msgtype) {
			case MT_RESULT:
				free_result((plcMsgResult *) res, true);
				break;
			case MT_CALLREQ:
				free_callreq((plcMsgCallreq *) res, true, true);
				break;
			case MT_RAW:
				free_rawmsg((plcMsgRaw *) res);
				break;
			default:
				ereport(ERROR,
				        (errcode(ERRCODE_RAISE_EXCEPTION),
					        errmsg("Returning message type '%c' from SPI call is not implemented", res->msgtype)));
		}
	}

	MemoryContextSwitchTo(oldcontext);
	CurrentResourceOwner = oldowner;

	/*
	 * AtEOSubXact_SPI() should not have popped any SPI context, but just
	 * in case it did, make sure we remain connected.
	 */
	SPI_restore_connection();
}

/*
 * Processing client exception message
 */
static void plcontainer_process_exception(plcMsgError *msg) {
	if (msg->stacktrace != NULL) {
		ereport(ERROR,
		        (errcode(ERRCODE_RAISE_EXCEPTION),
			        errmsg("PL/Container client exception occurred: \n %s \n %s", msg->message, msg->stacktrace)));
	} else {
		ereport(ERROR,
		        (errcode(ERRCODE_RAISE_EXCEPTION),
			        errmsg("PL/Container client exception occurred: \n %s", msg->message)));
	}
	free_error(msg);
}

/* function handler and friends */
static Datum
plcontainer_function_handler(FunctionCallInfo fcinfo, plcProcInfo *proc)
{
	Datum						datumreturn;
	plcProcResult * volatile 	presult = 		NULL;
	MemoryContext volatile 		oldcontext = 	CurrentMemoryContext;
	FuncCallContext	* volatile	funcctx =		NULL;
	bool	 volatile 				bFirstTimeCall = false;

	PG_TRY();
	{
		plc_elog(DEBUG1, "fcinfo->flinfo->fn_retset: %d", fcinfo->flinfo->fn_retset);

		if (fcinfo->flinfo->fn_retset)
		{
			/* First Call setup */
			if (SRF_IS_FIRSTCALL())
			{
				funcctx = SRF_FIRSTCALL_INIT();
				bFirstTimeCall = true;

				plc_elog(DEBUG1, "The funcctx pointer returned by SRF_FIRSTCALL_INIT() is: %p", funcctx);
			}

			/* Every call setup */
			funcctx = SRF_PERCALL_SETUP();
			plc_elog(DEBUG1, "The funcctx pointer returned by SRF_PERCALL_SETUP() is: %p", funcctx);

			Assert(funcctx != NULL);
			/* SRF uses multi_call_memory_ctx context shared between function calls,
			 * since EXPR etc. context will be cleared after one of the SRF calls.
			 * Note that plpython doesn't need it, because it doesn't use palloc to store
			 * the SRF result.
			 */
			oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
		} else {
			oldcontext = MemoryContextSwitchTo(pl_container_caller_context);
		}

		/* First time call for SRF or just a call of scalar function */
		if (!fcinfo->flinfo->fn_retset || bFirstTimeCall) {
			presult = plcontainer_get_result(fcinfo, proc);
			if (!fcinfo->flinfo->fn_retset) {
				/*
				 * SETOF function parameters will be deleted when last row is
				 * returned
				 */
				//TODO: delete proc->global when support it.
				//PLy_function_delete_args(proc);
			}
		}

		if (fcinfo->flinfo->fn_retset) {
			ReturnSetInfo *rsi = (ReturnSetInfo *) fcinfo->resultinfo;

			if (funcctx->user_fctx == NULL) {
				plc_elog(DEBUG1, "first time call, preparing the result set...");

				/* first time -- do checks and setup */
				if (!rsi || !IsA(rsi, ReturnSetInfo)
						|| (rsi->allowedModes & SFRM_ValuePerCall) == 0) {
					ereport(ERROR,
							(errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg(
									"unsupported set function return mode"), errdetail(
									"PL/Python set-returning functions only support returning only value per call.")));
				}
				rsi->returnMode = SFRM_ValuePerCall;

				funcctx->user_fctx = (void *) presult;

				if (funcctx->user_fctx == NULL)
					ereport(ERROR,
							(errcode(ERRCODE_DATATYPE_MISMATCH), errmsg(
									"returned object cannot be iterated"), errdetail(
									"PL/Python set-returning functions must return an iterable object.")));
			}

			presult = (plcProcResult *) funcctx->user_fctx;

			if (presult->resrow < presult->resmsg->rows)
				rsi->isDone = ExprMultipleResult;
			else {
				rsi->isDone = ExprEndResult;
			}

			if (rsi->isDone == ExprEndResult) {
				free_result(presult->resmsg, false);
				pfree(presult);
				MemoryContextSwitchTo(oldcontext);


				funcctx->user_fctx = NULL;

				//TODO: delete proc->global when support it.
				//PLy_function_delete_args(proc);

				SRF_RETURN_DONE(funcctx);
			}
		}

		/* Process the result message from client */
		datumreturn = plcontainer_process_result(fcinfo, proc, presult);
		presult->resrow += 1;
		MemoryContextSwitchTo(oldcontext);

	}
	PG_CATCH();
	{

		/*
		 * If there was an error the iterator might have not been exhausted
		 * yet. Set it to NULL so the next invocation of the function will
		 * start the iteration again.
		 */
		if (fcinfo->flinfo->fn_retset && funcctx->user_fctx != NULL) {
			funcctx->user_fctx = NULL;
		}
		if(presult && presult->resmsg) {
			free_result(presult->resmsg, false);
			pfree(presult);
		}
		MemoryContextSwitchTo(oldcontext);
		PG_RE_THROW();
	}
	PG_END_TRY();

	if (fcinfo->flinfo->fn_retset) {
		SRF_RETURN_NEXT(funcctx, datumreturn);
	} else {
		free_result(presult->resmsg, false);
		pfree(presult);
	}

#ifndef PLC_PG
		SIMPLE_FAULT_NAME_INJECTOR("plcontainer_before_udf_finish");
#endif

	return datumreturn;
}




