/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */
/* Global header files */
#include <assert.h>
#include <errno.h>
#include <netinet/ip.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <signal.h>
#include <limits.h>
#include <unistd.h>

/* R header files */
#include <R.h>
#include <Rversion.h>
#include <Rembedded.h>
#include <Rinternals.h>
#include <R_ext/Parse.h>
#include <Rdefines.h>

/* PL/Container header files */
#include "common/comm_channel.h"
#include "common/messages/messages.h"
#include "common/comm_utils.h"
#include "common/comm_connectivity.h"
#include "common/comm_server.h"
#include "rcall.h"
#include "rconversions.h"
#include "rlogging.h"

#define ERR_MSG_LENGTH 512

#if (R_VERSION >= 132352) /* R_VERSION >= 2.5.0 */
#define R_PARSEVECTOR(a_, b_, c_)  R_ParseVector(a_, b_, (ParseStatus *) c_, R_NilValue)
#else /* R_VERSION < 2.5.0 */
#define R_PARSEVECTOR(a_, b_, c_)  R_ParseVector(a_, b_, (ParseStatus *) c_)
#endif /* R_VERSION >= 2.5.0 */

#ifndef PATH_MAX
#define PATH_MAX 2048
#endif

#define TYPE_ID_LENGTH 12

#define OPTIONS_NULL_CMD    "options(error = expression(NULL))"

/* install the error handler to call our throw_r_error */
#define THROWRERROR_CMD \
		"pg.throwrerror <-function(msg) " \
		"{" \
		"  msglen <- nchar(msg);" \
		"  if (substr(msg, msglen, msglen + 1) == \"\\n\")" \
		"    msg <- substr(msg, 1, msglen - 1);" \
		"  .C(\"throw_r_error\", as.character(msg));" \
		"}"
#define OPTIONS_THROWRERROR_CMD \
		"options(error = expression(pg.throwrerror(geterrmessage())))"

/* install the notice handler to call our throw_r_notice */
#define THROWNOTICE_CMD \
		"pg.thrownotice <-function(msg) " \
		"{.C(\"throw_pg_notice\", as.character(msg))}"
#define THROWERROR_CMD \
		"pg.throwerror <-function(msg) " \
		"{stop(msg, call. = FALSE)}"
#define OPTIONS_THROWWARN_CMD \
		"options(warning.expression = expression(pg.thrownotice(last.warning)))"

#define SPI_EXEC_CMD \
		"pg.spi.exec <-function(sql) {.Call(\"plr_SPI_exec\", sql)}"

#define SPI_DBGETQUERY_CMD \
		"dbGetQuery <-function(sql) {\n" \
		"data <- pg.spi.exec(sql)\n" \
		"return(data)\n" \
		"}"

#define SPI_PREPARE_CMD \
		"pg.spi.prepare <-function(sql, argtypes = NA) " \
		"{.Call(\"plr_SPI_prepare\", sql, argtypes)}"

#define SPI_EXECP_CMD \
		"pg.spi.execp <-function(sql, argvalues = NA) " \
		"{.Call(\"plr_SPI_execp\", sql, argvalues)}"

#define PG_LOG_DEBUG_CMD \
		"plr.debug <- function(msg) {.Call(\"plr_debug\",msg)}"
#define PG_LOG_LOG_CMD \
		"plr.log <- function(msg) {.Call(\"plr_log\",msg)}"
#define PG_LOG_INFO_CMD \
		"plr.info <- function(msg) {.Call(\"plr_info\",msg)}"
#define PG_LOG_NOTICE_CMD \
		"plr.notice <- function(msg) {.Call(\"plr_notice\",msg)}"
#define PG_LOG_WARNING_CMD \
		"plr.warning <- function(msg) {.Call(\"plr_warning\",msg)}"
#define PG_LOG_ERROR_CMD \
		"plr.error <- function(msg) {.Call(\"plr_error\",msg)}"
#define PG_LOG_FATAL_CMD \
		"plr.fatal <- function(msg) {.Call(\"plr_fatal\",msg)}"

// init variable
plcConn *plcconn_global;

int plc_is_execution_terminated;

/* R interface */
void throw_pg_notice(const char **msg);

void throw_r_error(const char **msg);

SEXP plr_SPI_exec(SEXP rsql);

SEXP plr_SPI_prepare(SEXP rsql, SEXP rargtypes);

SEXP plr_SPI_execp(SEXP rsaved_plan, SEXP rargvalues);

/* Function definitions */
static char *get_load_self_ref_cmd(void);

static int load_r_cmd(const char *cmd);

static void send_error(plcConn *conn, char *msg);

static SEXP parse_r_code(const char *code, plcConn *conn, int *errorOccurred);

static char *create_r_func(plcMsgCallreq *req);

static int handle_matrix_set(SEXP retval, plcRFunction *r_func, plcMsgResult *res);

static int handle_retset(SEXP retval, plcRFunction *r_func, plcMsgResult *res);

static int process_call_results(plcConn *conn, SEXP retval, plcRFunction *r_func);

static SEXP arguments_to_r(plcRFunction *r_func);

static void pg_get_one_r(char *value, plcDatatype column_type, SEXP *obj, int elnum);

static SEXP process_SPI_results();

/* Globals */

/* Exposed in R_interface.h */
int R_SignalHandlers = 1;

/* set by hook throw_r_error */
static char *last_R_error_msg,
	*last_R_notice;

/* Global PL/Container connection */
plcConn *plcconn_global;
plcMsgError *plcLastErrMessage = NULL;

/* R objects */
typedef struct r_saved_plan {
	void *pplan; /* Store the pointer to plan on the QE side. */
	plcDatatype *argtypes;
	int nargs;
} r_saved_plan;

int r_init(void) {
	char *rargv[] = {"rclient", "--slave", "--silent", "--no-save", "--no-restore"};
	char *buf;
	char *r_home;
	int rargc;
	int status;
	int i;
	char *cmd;
	char *cmds[] =
		{
			/* first turn off error handling by R */
			OPTIONS_NULL_CMD,

			/* set up the postgres error handler in R */
			THROWRERROR_CMD,
			OPTIONS_THROWRERROR_CMD,
			THROWNOTICE_CMD,
			THROWERROR_CMD,
			OPTIONS_THROWWARN_CMD,

			/* install the commands for SPI support in the interpreter */
			SPI_EXEC_CMD,
			SPI_PREPARE_CMD,
			SPI_EXECP_CMD,
			SPI_DBGETQUERY_CMD,

			/* setup debug log to greenplum db */
			PG_LOG_DEBUG_CMD,
			PG_LOG_LOG_CMD,
			PG_LOG_INFO_CMD,
			PG_LOG_NOTICE_CMD,
			PG_LOG_WARNING_CMD,
			PG_LOG_ERROR_CMD,
			PG_LOG_FATAL_CMD,
			SPI_DBGETQUERY_CMD,

			/* terminate */
			NULL
		};


	r_home = getenv("R_HOME");
	/*
	 * Stop R using its own signal handlers Otherwise, R will prompt the user for what to do and
		 will hang in the container
	*/
	R_SignalHandlers = 0;
	if (r_home == NULL) {
		plc_elog(ERROR, "R_HOME is not set, please check and set the R_HOME");
		return -1;
	}

	rargc = sizeof(rargv) / sizeof(rargv[0]);

	if (!Rf_initEmbeddedR(rargc, rargv)) {
		//TODO: return an error
		plc_elog(ERROR, "can not start Embedded R");
	}

	/*
	 * temporarily turn off R error reporting -- it will be turned back on
	 * once the custom R error handler is installed from the plr library
	 */

	status = load_r_cmd(cmds[0]);

	if (status < 0) {
		return -1;
	}

	status = load_r_cmd(buf = get_load_self_ref_cmd());
	pfree(buf);

	if (status < 0) {
		return -1;
	}

	for (i = 1; (cmd = cmds[i]); i++) {
		status = load_r_cmd(cmds[i]);
		if (status < 0) {
			return -1;
		}
	}

	return 0;
}

static char *get_load_self_ref_cmd() {
	char *buf = (char *) pmalloc(PATH_MAX);

#ifdef __linux__
	char path[PATH_MAX];
	char *p = NULL;
	int size;
	/* next load the plr library into R */
	size = readlink("/proc/self/exe", path, PATH_MAX);
	if (size == -1) {
		plc_elog(ERROR, "can not read execute path");
	} else {
		path[size] = '\0';
	}

	plc_elog(DEBUG1, "Current R client path is %s", path);
	p = strrchr(path, '/');
	if(p) {
		*(p+1) = '\0';
	} else {
		plc_elog(ERROR, "can not read execute directory %s", path);
	}

	plc_elog(DEBUG1, "Split path by '/'. Get the path: %s", path);
	snprintf(buf, PATH_MAX, "dyn.load(\"%s/%s\")", path, "librcall.so");
#else
	snprintf(buf, PATH_MAX, "dyn.load(\"%s\")", "librcall.so");
#endif
	return buf;
}

static int load_r_cmd(const char *cmd) {
	SEXP cmdSexp,
		cmdexpr;
	int i,
		status = 0;


	PROTECT(cmdSexp = NEW_CHARACTER(1));
	SET_STRING_ELT(cmdSexp, 0, COPY_TO_USER_STRING(cmd));
	PROTECT(cmdexpr = R_PARSEVECTOR(cmdSexp, -1, &status));
	if (status != PARSE_OK) {
		goto error;
	}

	/* Loop is needed here as EXPSEXP may be of length > 1 */
	for (i = 0; i < length(cmdexpr); i++) {
		R_tryEval(VECTOR_ELT(cmdexpr, i), R_GlobalEnv, &status);
		if (status != 0) {
			goto error;
		}
	}

	UNPROTECT(2);
	return 0;

	error:

	UNPROTECT(2);
	raise_execution_error("Error evaluating function %s", cmd);

	return -1;
}

void handle_call(plcMsgCallreq *req, plcConn *conn) {
	SEXP r,
		strres,
		call,
		rargs;

	int errorOccurred;

	char *func,
		*errmsg;

	client_log_level = req->logLevel;
	plc_elog(DEBUG1, "R client receives a call");
	/*
	 * Keep our connection for future calls from R back to us.
	*/
	plcconn_global = conn;

	/* wrap the input in a function and evaluate the result */

	func = create_r_func(req);

	plcRFunction *r_func = plc_R_init_function(req);
	PROTECT(r = parse_r_code(func, conn, &errorOccurred));

	pfree(func);

	if (errorOccurred) {
		//TODO send real error message
		/* run_r_code will send an error back */
		UNPROTECT(1); //r
		return;
	}

	if (req->nargs > 0) {
		rargs = arguments_to_r(r_func);
		PROTECT(call = lcons(r, rargs));
	} else {
		PROTECT(call = lcons(r, R_NilValue));
	}

	/* call the function */
	plc_is_execution_terminated = 0;

	PROTECT(strres = R_tryEval(call, R_GlobalEnv, &errorOccurred));

	if (errorOccurred) {
		UNPROTECT(3); //r, strres, call
		//TODO send real error message
		if (last_R_error_msg) {
			errmsg = strdup(last_R_error_msg);
		} else {
			errmsg = strdup("Error executing\n");
			errmsg = realloc(errmsg, strlen(errmsg) + strlen(req->proc.src));
			errmsg = strcat(errmsg, req->proc.src);
		}
		send_error(conn, errmsg);
		free(errmsg);
		plc_r_free_function(r_func);
		return;
	}

	if (plc_is_execution_terminated == 0) {
		process_call_results(conn, strres, r_func);
	}

	plc_r_free_function(r_func);

	UNPROTECT(3); //r, strres, call
	plc_elog(DEBUG1, "R client finished processing this call");

	return;
}

static void send_error(plcConn *conn, char *msg) {
	/* an exception was thrown */
	plcMsgError *err;
	err = pmalloc(sizeof(plcMsgError));
	err->msgtype = MT_EXCEPTION;
	err->message = msg;
	err->stacktrace = NULL;

	/* send the result back */
	plcontainer_channel_send(conn, (plcMessage *) err);

	/* free the objects */
	free(err);
}

static SEXP parse_r_code(const char *code, plcConn *conn, int *errorOccurred) {
	/* int hadError; */
	ParseStatus status;
	char *errmsg;
	SEXP tmp,
		rbody,
		fun;

	PROTECT(rbody = mkString(code));
	/*
	  limit the number of expressions to be parsed to 2:
		- the definition of the function, i.e. f <- function() {...}
		- the call to the function f()

	  kind of useful to prevent injection, but pointless since we are
	  running in a container. I think -1 is equivalent to no limit.
	*/
	PROTECT(tmp = R_PARSEVECTOR(rbody, -1, &status));

	if (tmp != R_NilValue) {
		PROTECT(fun = VECTOR_ELT(tmp, 0));
	} else {
		PROTECT(fun = R_NilValue);
	}

	if (status != PARSE_OK) {
		if (last_R_error_msg != NULL) {
			errmsg = strdup(last_R_error_msg);
		} else {
			errmsg = strdup("Parse Error\n");
			errmsg = realloc(errmsg, strlen(errmsg) + strlen(code) + 1);
			errmsg = strcat(errmsg, code);
		}
		goto error;
	}

	UNPROTECT(3);
	*errorOccurred = 0;
	return fun;

	error:
	UNPROTECT(3);
	/*
	 * set the global error flag
	 */
	*errorOccurred = 1;
	send_error(conn, errmsg);
	free(errmsg);
	return NULL;
}

static char *create_r_func(plcMsgCallreq *req) {
	int plen;
	char *mrc;
	size_t mlen = 0;

	int i;

	/* calculate space required for args */
	mlen = 5; // for args,
	for (i = 0; i < req->nargs; i++) {
		/* +4 for , and space */
		if (req->args[i].name != NULL) {
			mlen += strlen(req->args[i].name) + 4;
		}
	}
	/*
	 * room for function source and function call
	 */
	mlen += strlen(req->proc.src) + strlen(req->proc.name) + 40 + strlen("gpdb.");

	mrc = pmalloc(mlen);

	/* create the first part of the function name and add the args array */
	plen = snprintf(mrc, mlen, "gpdb.%s <- function(args", req->proc.name);

	for (i = 0; i < req->nargs; i++) {

		if (req->args[i].name != NULL) {
			/*
			 * add a comma, note if there are no args this will not be added
			 * and if there are some it will be added before the arg
			 */
			strcat(mrc, ", ");
			plen += 2;

			strcat(mrc, req->args[i].name);

			/* keep track of where we are copying */
			plen += strlen(req->args[i].name);
		}
	}

	/* finish the function definition from where we left off */
	plen = snprintf(mrc + plen, mlen, ") {%s}", req->proc.src);
	assert(plen >= 0 && ((size_t) plen) < mlen);
	return mrc;
}

static int handle_frame(SEXP df, plcRFunction *r_func, plcMsgResult *res) {
	uint32 row, col, cols;

	/* a data frame is an array of columns, the length of which is the number of columns */
	res->cols = 1;
	cols = length(df);
	SEXP dfcol = VECTOR_ELT(df, 0);
	res->rows = length(dfcol);
	res->data = pmalloc(res->rows * sizeof(rawdata *));


	plc_r_copy_type(&res->types[0], &r_func->res);
	res->names[0] = strdup(r_func->res.argName);

	for (row = 0; row < res->rows; row++) {
		plcUDT *udt;

		// allocate space for the data
		res->data[row] = pmalloc(sizeof(rawdata));

		// allocate space for the UDT
		udt = pmalloc(sizeof(plcUDT));

		// allocate space for the columns of the UDT
		udt->data = pmalloc(cols * sizeof(rawdata));

		for (col = 0; col < cols; col++) {

			PROTECT(dfcol = VECTOR_ELT(df, col));
			/*
			* R stores characters in factors for efficiency...
			*/
			if (isFactor(dfcol)) {
				/*
				 * a factor is a special type of integer
				 * but must check for NA value first
				 */
				if (INTEGER(dfcol)[row] != NA_INTEGER) {
					SEXP c;
					PROTECT(c = Rf_asCharacterFactor(dfcol));

					rawdata *datum = plc_r_vector_element_rawdata(c, row, &r_func->res.subTypes[col]);
					udt->data[col].isnull = datum->isnull;
					udt->data[col].value = datum->value;

					UNPROTECT(1);
					free(datum);
				} else {
					udt->data[col].isnull = TRUE;
					udt->data[col].value = NULL;
				}
			} else {
				rawdata *datum = plc_r_vector_element_rawdata(dfcol, row, &r_func->res.subTypes[col]);
				udt->data[col].isnull = datum->isnull;
				udt->data[col].value = datum->value;
				free(datum);
			}
			UNPROTECT(1);
		}
		res->data[row]->value = (char *) udt;
		res->data[row]->isnull = FALSE;
	}
	return 0;
}

static int handle_matrix_set(SEXP retval, plcRFunction *r_func, plcMsgResult *res) {
	int cols, start = 0;
	uint32 i;
	SEXP rdims;
	PROTECT(rdims = getAttrib(retval, R_DimSymbol));
	// get the number of rows
	if (rdims != R_NilValue) {
		res->rows = INTEGER(rdims)[0];
		cols = INTEGER(rdims)[1];
	} else {
		UNPROTECT(1);
		return -1;
	}
	UNPROTECT(1);

	// this is a matrix of vectors but we only handle one column in set of right now
	res->cols = 1;
	res->data = malloc(res->rows * sizeof(rawdata *));

	for (i = 0; i < res->rows; i++) {
		res->data[i] = malloc(cols * sizeof(rawdata));
	}
	plc_r_copy_type(&res->types[0], &r_func->res);
	res->names[0] = strdup(r_func->res.argName);

	start = 0;

	for (i = 0; i < res->rows; i++) {
		res->data[i][0].isnull = 0;
		if (plc_r_matrix_as_setof(retval, start, cols, &res->data[i][0].value, &r_func->res) != 0) {
			free_result(res, true);
			return -1;
		}
		start = start + cols;
	}
	return 0;
}

static int handle_retset(SEXP retval, plcRFunction *r_func, plcMsgResult *res) {
	uint32 i = 0;
	rawdata *raw;

	/*
	 *  we check for the dims here to find arrays of text
	 *  a simple one dimensional array of text will be handled below
	 *  an n dimensional array of text will be handle in handle_matrix_set
	 *  having a dimension should guarantee that it is an array of text
	 */
	if (isMatrix(retval) || (IS_CHARACTER(retval) && getAttrib(retval, R_DimSymbol) != R_NilValue)) {
		handle_matrix_set(retval, r_func, res);
	} else if (isFrame(retval)) {
		handle_frame(retval, r_func, res);
	} else {
		res->rows = length(retval);
		res->cols = 1;
		res->data = malloc(res->rows * sizeof(rawdata *));

		for (i = 0; i < res->rows; i++) {
			res->data[i] = NULL;
		}
		plc_r_copy_type(&res->types[0], &r_func->res);
		res->names[0] = strdup(r_func->res.argName);

		for (i = 0; i < res->rows; i++) {

			if (r_func->res.conv.outputfunc == NULL) {
				raise_execution_error("Type %d is not yet supported by R container",
				                      (int) res->types[0].type);
				free_result(res, true);
				return -1;
			}
			raw = plc_r_vector_element_rawdata(retval, i, &r_func->res);
			if (raw == NULL) {
				free_result(res, true);
				return -1;
			} else {
				res->data[i] = raw;
			}

		}
	}
	return 0;
}

static int process_call_results(plcConn *conn, SEXP retval, plcRFunction *r_func) {
	plcMsgResult *res;
	uint32 i = 0;
	int ret = 0;


	/* allocate a result */
	res = malloc(sizeof(plcMsgResult));
	res->msgtype = MT_RESULT;
	res->names = malloc(1 * sizeof(char *));
	res->types = malloc(1 * sizeof(plcType));
	res->exception_callback = NULL;


	if (r_func->retset != 0) {
		if (handle_retset(retval, r_func, res) != 0) {
			free_result(res, true);
			return -1;
		}
	} else {

		res->rows = 1;
		res->cols = 1;

		res->data = malloc(res->rows * sizeof(rawdata *));
		for (i = 0; i < res->rows; i++) {
			res->data[i] = malloc(res->cols * sizeof(rawdata));
		}
		plc_r_copy_type(&res->types[0], &r_func->res);
		res->names[0] = strdup(r_func->res.argName);

		if (retval == R_NilValue) {
			res->data[0][0].isnull = 1;
			res->data[0][0].value = NULL;

		} else {
			for (i = 0; i < res->rows; i++) {

				res->data[i][0].isnull = 0;
				if (r_func->res.conv.outputfunc == NULL) {
					raise_execution_error("Type %d is not yet supported by R container",
					                      (int) res->types[0].type);
					free_result(res, true);
					return -1;
				}

				ret = r_func->res.conv.outputfunc(retval, &res->data[i][0].value, &r_func->res);

				if (ret != 0) {
					raise_execution_error("Exception raised converting function output to function output type %d",
					                      (int) res->types[0].type);
					free_result(res, true);
					return -1;
				}
			}
		}
	}
	/* send the result back */
	plcontainer_channel_send(conn, (plcMessage *) res);

	free_result(res, true);

	return 0;
}

static SEXP arguments_to_r(plcRFunction *r_func) {
	SEXP r_args, r_curarg, allargs, element;
	int i, notnull = 0;

	/* number of arguments that have names and should make it to the input tuple */
	for (i = 0; i < r_func->nargs; i++) {
		if (r_func->call->args[i].name != NULL) {
			notnull += 1;
		}
	}

	/* create the argument list plus 1 for the unnamed args list */
	PROTECT(r_args = r_curarg = allocList(notnull + 1));
	PROTECT(allargs = allocList(r_func->nargs));

	/* all argument vector is the 1st argument */
	SETCAR(r_curarg, allargs);
	r_curarg = CDR(r_curarg);

	for (i = 0; i < r_func->nargs; i++) {

		if (r_func->call->args[i].data.isnull) {
			PROTECT(element = R_NilValue);
		} else {

			if (r_func->args[i].conv.inputfunc == NULL) {
				raise_execution_error("Parameter '%s' type %d is not supported",
				                      r_func->args[i].argName,
				                      r_func->args[i].type);
				UNPROTECT(2);
				return NULL;
			}

			//  this is returned protected by the input function
			element = r_func->args[i].conv.inputfunc(r_func->call->args[i].data.value, &r_func->args[i]);
		}

		if (element == NULL) {
			raise_execution_error("Converting parameter '%s' to R type failed",
			                      r_func->args[i].argName);

			/* we've made it to the i'th argument */
			UNPROTECT(2 + i - 1);
			return NULL;
		}

		if (r_func->call->args[i].name != NULL) {
			SETCAR(r_curarg, element);
			r_curarg = CDR(r_curarg);
		}

		/* all arguments named or otherwise go in here */
		SETCAR(allargs, element);
		allargs = CDR(allargs);
	}
	/* the input function above returns args protected */
	UNPROTECT(r_func->nargs + 2);
	return r_args;
}

/*
 * given a single non-array pg value, convert to its R value representation
 */
static void pg_get_one_r(char *value, plcDatatype column_type, SEXP *obj, int elnum) {

	int bsize;
	switch (column_type) {

		/* 2 and 4 byte integer pgsql datatype => use R INTEGER */
		case PLC_DATA_INT2:
			INTEGER_DATA(*obj)[elnum] = *((int16 *) value);
			break;
		case PLC_DATA_INT4:
			INTEGER_DATA(*obj)[elnum] = *((int32 *) value);
			break;

			/*
			 * Other numeric types => use R REAL
			 * Note pgsql int8 is mapped to R REAL
			 * because R INTEGER is only 4 byte
			 */
		case PLC_DATA_INT8:
			NUMERIC_DATA(*obj)[elnum] = (int64) (*((float8 *) value));
			break;
		case PLC_DATA_FLOAT4:
			NUMERIC_DATA(*obj)[elnum] = *((float4 *) value);
			break;
		case PLC_DATA_FLOAT8:
			NUMERIC_DATA(*obj)[elnum] = *((float8 *) value);
			break;

		case PLC_DATA_INT1:
			LOGICAL_DATA(*obj)[elnum] = *((int8 *) value);
			break;

		case PLC_DATA_UDT:
		case PLC_DATA_INVALID:
		case PLC_DATA_ARRAY:
			raise_execution_error("unhandled type %s [%d]",
			                      plc_get_type_name(column_type), column_type);
			break;

		case PLC_DATA_BYTEA:
			/*
			 * for bytea type, we first get its size then do copy
			 * based on upstream, we trade it as TEXT, so '\0' is
			 * not accepted in bytea type
			 */
			if (value[0] != '\0') {
				bsize = *((int *) value);
				SET_STRING_ELT(*obj, elnum, mkCharLen(value + 4, bsize));
				break;
			}
			// fallthrough
		case PLC_DATA_TEXT:
		default:
			/* Everything else is defaulted to string */
			if (value) {
				SET_STRING_ELT(*obj, elnum, COPY_TO_USER_STRING(value));
			} else {
				SET_STRING_ELT(*obj, elnum, NA_STRING);
			}
	}
}

/*
 * common function for SPI exec and SPI execp to extract returned results
 */
static SEXP process_SPI_results() {
	plcMsgResult *result;
	plcMessage *resp;
	SEXP r_result = NULL,
		names,
		row_names,
		fldvec;

	uint32 i, j;
	int res = 0;

	char buf[256];

receive:
	res = plcontainer_channel_receive(plcconn_global, &resp, MT_PING_BIT | MT_CALLREQ_BIT | MT_RESULT_BIT| MT_EXCEPTION_BIT);
	if (res < 0) {
		raise_execution_error("Error receiving data from the backend, %d", res);
		return NULL;
	}
	switch (resp->msgtype) {
		case MT_CALLREQ:
			handle_call((plcMsgCallreq *) resp, plcconn_global);
			free_callreq((plcMsgCallreq *) resp, false, false);
			goto receive;
		case MT_PING:
			plcontainer_channel_send(plcconn_global, resp);
			goto receive;
		case MT_EXCEPTION:
			if (((plcMsgError *) resp)->message != NULL) {
				raise_execution_error("SPI process_SPI_results failed due to %s", ((plcMsgError *) resp)->message);
			} else {
				raise_execution_error("SPI process_SPI_results failed.");
			}
			break;
		case MT_RESULT:
			break;
		default:
			raise_execution_error("didn't receive result back %c", resp->msgtype);
			return NULL;
	}

	result = (plcMsgResult *) resp;

	/*
	 * If result->cols=0, it should be the INSERT, UPDATE or DELETE statment
	 */
	if (result->cols == 0) {
		char buffer[64];
		PROTECT(r_result = NEW_CHARACTER(1));
		snprintf(buf, sizeof(buffer), "%d", result->rows);
		SET_STRING_ELT(r_result, 0, COPY_TO_USER_STRING(buf));
		UNPROTECT(1);
		free_result(result, false);
		return r_result;
	} else if (result->rows == 0) {
		free_result(result, false);
		return R_NilValue;
	}

	/*
	 * r_result is a list of columns
	 */
	PROTECT(r_result = NEW_LIST(result->cols));

	/*
	 * names for each column
	 */
	PROTECT(names = NEW_CHARACTER(result->cols));

	/*
	 * we store everything in columns because vectors can only have one type
	 * normally we get tuples back in rows with each column possibly a different type,
	 * instead we store each column in a single vector
	 */

	for (j = 0; j < result->cols; j++) {
		/*
		 * set the names of the column
		 */
		SET_STRING_ELT(names, j, Rf_mkChar(result->names[j]));

		/*
		 * create a vector of the type that is rows long
		 * For type BYTEA, we process it as TEXT
		 */
		if (result->types[j].type == PLC_DATA_BYTEA) {
			PROTECT(fldvec = get_r_vector(PLC_DATA_TEXT, result->rows));
		} else {
			PROTECT(fldvec = get_r_vector(result->types[j].type, result->rows));
		}

		for (i = 0; i < result->rows; i++) {
			/*
			 * store the value
			 */
			pg_get_one_r(result->data[i][j].value, result->types[j].type, &fldvec, i);
		}

		UNPROTECT(1);
		SET_VECTOR_ELT(r_result, j, fldvec);
	}

	/* attach the column names */
	setAttrib(r_result, R_NamesSymbol, names);

	/* attach row names - basically just the row number, zero based */
	PROTECT(row_names = allocVector(STRSXP, result->rows));

	for (i = 0; i < result->rows; i++) {
		sprintf(buf, "%d", i + 1);
		SET_STRING_ELT(row_names, i, COPY_TO_USER_STRING(buf));
	}

	setAttrib(r_result, R_RowNamesSymbol, row_names);

	/* finally, tell R we are a data.frame */
	setAttrib(r_result, R_ClassSymbol, mkString("data.frame"));

	/*
	 * result has an attribute names which is a vector of names
	 * a vector of vectors num columns long by num rows
	 */
	free_result(result, false);

	UNPROTECT(3);
	return r_result;

}

/*
 * plr_SPI_exec - The builtin SPI_exec command for the R interpreter
 */
SEXP plr_SPI_exec(SEXP rsql) {
	const char *sql;
	plcMsgSQL *msg;

	PROTECT(rsql = AS_CHARACTER(rsql));
	sql = CHAR(STRING_ELT(rsql, 0));
	UNPROTECT(1);

	if (sql == NULL) {
		raise_execution_error("R client cannot execute empty query");
		return NULL;
	}

	/* If the execution was terminated we don't need to proceed with SPI */
	if (plc_is_execution_terminated != 0) {
		return NULL;
	}

	msg = pmalloc(sizeof(plcMsgSQL));
	msg->msgtype = MT_SQL;
	msg->sqltype = SQL_TYPE_STATEMENT;
	msg->limit = 0;    /* No limit for R. */

	/*
	 * satisfy compiler
	 */
	msg->statement = (char *) sql;

	plcontainer_channel_send(plcconn_global, (plcMessage *) msg);

	/* we don't need it anymore */
	pfree(msg);

	return process_SPI_results();

}

/*
 * plr_SPI_prepare - The builtin SPI_prepare command for the R interpreter
 */
SEXP plr_SPI_prepare(SEXP rsql, SEXP rargtypes) {
	const char *query;
	int nargs;
	int res;
	int i;
	plcConn *conn = plcconn_global;
	r_saved_plan *r_plan;

	SEXP r_result;

	plcMsgSQL msg;
	plcMessage *resp;

	char *start;
	int offset = 0, tx_len = 0;
	int is_plan_valid;

	PROTECT(rsql = AS_CHARACTER(rsql));
	query = CHAR(STRING_ELT(rsql, 0));
	UNPROTECT(1);

	if (query == NULL) {
		raise_execution_error("R client cannot execute empty query");
		return NULL;
	}

	PROTECT(rargtypes = AS_INTEGER(rargtypes));
	if (!isVector(rargtypes) || !isInteger(rargtypes)) {
		raise_execution_error("second parameter must be a vector of PostgreSQL datatypes");
	}

	/* deal with case of no parameters for the prepared query */
	if (rargtypes == R_MissingArg || INTEGER(rargtypes)[0] == NA_INTEGER) {
		nargs = 0;
	} else {
		nargs = length(rargtypes);
	}

	if (nargs < 0) {
		raise_execution_error("second parameter must be a vector of PostgreSQL datatypes");
	}

	msg.msgtype = MT_SQL;
	msg.sqltype = SQL_TYPE_PREPARE;
	msg.nargs = nargs;
	msg.statement = strdup(query);
	msg.args = malloc(msg.nargs * sizeof(plcArgument));

	for (i = 0; i < nargs; i++) {
		char typeid[TYPE_ID_LENGTH];
		sprintf(typeid, "%d", INTEGER(rargtypes)[i]);
		fill_prepare_argument(&msg.args[i], typeid, PLC_DATA_INT4);
	}

	UNPROTECT(1);

	plcontainer_channel_send(conn, (plcMessage *) &msg);
	free_arguments(msg.args, msg.nargs, false, false);

	res = plcontainer_channel_receive(conn, &resp, MT_RAW_BIT | MT_EXCEPTION_BIT);

	if (resp->msgtype == MT_EXCEPTION) {
			if (((plcMsgError *) resp)->message != NULL) {
				raise_execution_error("SPI process_SPI_results failed due to %s", ((plcMsgError *) resp)->message);
			} else {
				raise_execution_error("SPI process_SPI_results failed.");
			}
	}
	if (res < 0) {
		raise_execution_error("Error receiving data from the frontend, %d", res);
		return NULL;
	}

	start = ((plcMsgRaw *) resp)->data;
	tx_len = ((plcMsgRaw *) resp)->size;

	r_plan = (r_saved_plan *) malloc(sizeof(r_saved_plan));
	is_plan_valid = (*((int32 *) (start + offset)));
	offset += sizeof(int32);

	if (!is_plan_valid) {
		raise_execution_error("plpy.prepare failed. See backend for details.");
		return NULL;
	}

	r_plan->pplan = (int64 *) (*((long long *) (start + offset)));
	offset += sizeof(int64);
	r_plan->nargs = *((int *) (start + offset));
	offset += sizeof(int32);


	if (r_plan->nargs != nargs) {
		raise_execution_error("plpy.prepare: bad argument number: %d "
			                      "(returned) vs %d (expected).", r_plan->nargs, nargs);
		return NULL;
	}

	if (nargs > 0) {
		if (offset + (signed int) sizeof(plcDatatype) * nargs != tx_len) {
			raise_execution_error("Client format error for spi prepare. "
				                      "calculated length (%d) vs transferred length (%d)",
			                      offset + sizeof(plcDatatype) * nargs, tx_len);
			return NULL;
		}

		r_plan->argtypes = malloc(sizeof(plcDatatype) * nargs);
		if (r_plan->argtypes == NULL) {
			raise_execution_error("Could not allocate %d bytes for argtypes"
				                      " in py_plan", sizeof(plcDatatype) * nargs);
			return NULL;
		}
		memcpy(r_plan->argtypes, start + offset, sizeof(plcDatatype) * nargs);
	}

	r_result = R_MakeExternalPtr(r_plan, R_NilValue, R_NilValue);

	free_rawmsg((plcMsgRaw *) resp);

	return r_result;
}

/*
 * plr_SPI_execp - The builtin SPI_execp command for the R interpreter
 */
SEXP plr_SPI_execp(SEXP rsaved_plan, SEXP rargvalues) {
	r_saved_plan *r_plan = (r_saved_plan *) R_ExternalPtrAddr(rsaved_plan);
	plcArgument *args;
	plcMsgSQL msg;


	int nargs, i;

	SEXP obj;

	if (r_plan == NULL) {
		raise_execution_error("SPI plan does not found");
		return NULL;
	}

	nargs = r_plan->nargs;
	args = pmalloc(sizeof(plcArgument) * nargs);
	if (nargs > 0) {
		if (!Rf_isVectorList(rargvalues)) {
			raise_execution_error("second parameter must be a list of arguments to the prepared plan");
		}

		if (length(rargvalues) != nargs) {
			raise_execution_error("list of arguments (%d) is not the same length "
				                      "as that of the prepared plan (%d)",
			                      length(rargvalues), nargs);
		}
	}

	for (i = 0; i < nargs; i++) {
		args[i].type.type = r_plan->argtypes[i];
		args[i].name = NULL; /* We do not need name */
		args[i].type.nSubTypes = 0;
		args[i].type.typeName = NULL;
		args[i].data.value = NULL;

		PROTECT(obj = VECTOR_ELT(rargvalues, i));

		if (obj != NULL && !isNull(obj) && obj != R_NilValue) {
			args[i].data.isnull = 0;
			plc_get_output_function(r_plan->argtypes[i])(obj, &args[i].data.value, NULL);
		} else {
			/* follow python client */
			args[i].data.isnull = 1;
			args[i].data.value = NULL;
		}

		UNPROTECT(1);
	}

	msg.msgtype = MT_SQL;
	msg.sqltype = SQL_TYPE_PEXECUTE;
	msg.pplan = r_plan->pplan;
	msg.limit = 0;
	msg.nargs = nargs;
	msg.args = args;

	plcontainer_channel_send(plcconn_global, (plcMessage *) &msg);
	free_arguments(args, nargs, false, false);

	return process_SPI_results();
}

void raise_execution_error(const char *format, ...) {
	char *msg = NULL;

	if (format == NULL) {
		msg = strdup("Error message cannot be NULL in raise_execution_error()");
	} else {
		va_list args;
		int len, res;

		va_start(args, format);
		len = ERR_MSG_LENGTH + 2 * strlen(format);
		msg = (char *) malloc(len + 1);
		res = vsnprintf(msg, len, format, args);
		if (res < 0 || res >= len) {
			msg = strdup("Error formatting error message string in raise_execution_error()");
		}
		plc_elog(WARNING, "R client caught an error: %s", msg);
	}

	if (plcLastErrMessage == NULL && plc_is_execution_terminated == 0) {
		plcMsgError *err;

		/* an exception to be thrown */
		err = malloc(sizeof(plcMsgError));
		err->msgtype = MT_EXCEPTION;
		err->message = msg;
		err->stacktrace = "";

		/* When no connection available - keep the error message in stack */
		plcLastErrMessage = err;
		plc_raise_delayed_error(plcconn_global);
	} else {
		plc_elog(WARNING, "Cannot send second subsequent error message to backend:");
		plc_elog(WARNING, "%s", msg);
		free(msg);
	}

}

void plc_raise_delayed_error(plcConn *conn) {
	if (plcLastErrMessage != NULL) {
		if (plc_is_execution_terminated == 0 && conn != NULL) {
			plcontainer_channel_send(conn, (plcMessage *) plcLastErrMessage);
			free_error(plcLastErrMessage);
			plcLastErrMessage = NULL;
			plc_is_execution_terminated = 1;
		} else if (conn == NULL) {
			plc_elog(ERROR, "client caught an error: %s", plcLastErrMessage->message);
		}
	}
}

void throw_pg_notice(const char **msg) {
	if (msg && *msg)
		last_R_notice = strdup(*msg);
}

void throw_r_error(const char **msg) {
	if (msg && *msg)
		last_R_error_msg = strdup(*msg);
	else
		last_R_error_msg = strdup("caught error calling R function");
}


#ifdef DEBUGPROTECT
int balance=0;
SEXP
pg_protect(SEXP s, char *fn, int ln)
{
	balance++;
	plc_elog(NOTICE, "%d\tPROTECT\t1\t%s\t%d", balance, fn, ln);
	return protect(s);
}

void
pg_unprotect(int n, char *fn, int ln)
{
	balance=balance-n;
	plc_elog(NOTICE, "%d\tUNPROTECT\t%d\t%s\t%d", balance, n, fn, ln);
	unprotect(n);
}
#endif /* DEBUGPROTECT */
