/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "plpy_spi.h"

#include "common/comm_channel.h"
#include "common/comm_utils.h"
#include "pycall.h"
#include "pyerror.h"
#include "pyconversions.h"

#define dgettext(d, x) (x)

typedef struct PLySubtransactionObject {
	PyObject_HEAD
	bool started;
	bool exited;
} PLySubtransactionObject;


static PyObject *PLy_subtransaction_new(void);

static void PLy_subtransaction_dealloc(PyObject *);

static PyObject *PLy_subtransaction_enter(PyObject *, PyObject *);

static PyObject *PLy_subtransaction_exit(PyObject *, PyObject *);


static char PLy_subtransaction_doc[] = {
	"PostgreSQL subtransaction context manager"
};


/* call PyErr_SetString with a vprint interface and translation support */
static void
PLy_exception_set(PyObject *, const char *, ...)
__attribute__((format(printf, 2, 3)));

static PyObject *PLy_exc_spi_error = NULL;

static PyMethodDef PLy_subtransaction_methods[] = {
	{"__enter__", PLy_subtransaction_enter, METH_VARARGS, NULL},
	{"__exit__",  PLy_subtransaction_exit,  METH_VARARGS, NULL},
	/* user-friendly names for Python <2.6 */
	{"enter",     PLy_subtransaction_enter, METH_VARARGS, NULL},
	{"exit",      PLy_subtransaction_exit,  METH_VARARGS, NULL},
	{NULL, NULL, 0,                                       NULL}
};

PyTypeObject PLy_SubtransactionType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"PLySubtransaction",        /* tp_name */
	sizeof(PLySubtransactionObject),    /* tp_size */
	0,                            /* tp_itemsize */

	/*
	 * methods
	 */
	PLy_subtransaction_dealloc, /* tp_dealloc */
	0,                            /* tp_print */
	0,                            /* tp_getattr */
	0,                            /* tp_setattr */
	0,                            /* tp_compare */
	0,                            /* tp_repr */
	0,                            /* tp_as_number */
	0,                            /* tp_as_sequence */
	0,                            /* tp_as_mapping */
	0,                            /* tp_hash */
	0,                            /* tp_call */
	0,                            /* tp_str */
	0,                            /* tp_getattro */
	0,                            /* tp_setattro */
	0,                            /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,    /* tp_flags */
	PLy_subtransaction_doc,        /* tp_doc */
	0,                            /* tp_traverse */
	0,                            /* tp_clear */
	0,                            /* tp_richcompare */
	0,                            /* tp_weaklistoffset */
	0,                            /* tp_iter */
	0,                            /* tp_iternext */
	PLy_subtransaction_methods, /* tp_tpmethods */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
};

static PyObject *PLy_spi_execute_query(char *query, long limit);

static PyObject *PLy_spi_execute_plan(PyObject *, PyObject *, long);

static PyObject *PLy_spi_execute_fetch_result(plcMsgResult *resp);

PyObject *PLy_spi_execute(PyObject *self, PyObject *pyquery);

PyObject *PLy_spi_prepare(PyObject *self, PyObject *args);

typedef struct PLyResultObject
{
	PyObject_HEAD
	/* HeapTuple *tuples; */
	PyObject   *nrows;			/* number of rows returned by query */
	PyObject   *rows;			/* data rows, or None if no data returned */
	PyObject   *status;			/* query status, SPI_OK_*, or SPI_ERR_* */
} PLyResultObject;

static char PLy_result_doc[] = {
	"Results of a Greenplum query"
};

static PyObject *PLy_result_new(void);
static void PLy_result_dealloc(PyObject *);
static PyObject *PLy_result_nrows(PyObject *, PyObject *);
static PyObject *PLy_result_status(PyObject *, PyObject *);
static Py_ssize_t PLy_result_length(PyObject *);
static PyObject *PLy_result_item(PyObject *, Py_ssize_t);
static PyObject *PLy_result_slice(PyObject *, Py_ssize_t, Py_ssize_t);
static int	PLy_result_ass_item(PyObject *, Py_ssize_t, PyObject *);
static int	PLy_result_ass_slice(PyObject *, Py_ssize_t, Py_ssize_t, PyObject *);

static PySequenceMethods PLy_result_as_sequence = {
	PLy_result_length,			/* sq_length */
	NULL,						/* sq_concat */
	NULL,						/* sq_repeat */
	PLy_result_item,			/* sq_item */
	PLy_result_slice,			/* sq_slice */
	PLy_result_ass_item,		/* sq_ass_item */
	PLy_result_ass_slice,		/* sq_ass_slice */
};

static PyMethodDef PLy_result_methods[] = {
	{"nrows", PLy_result_nrows, METH_VARARGS, NULL},
	{"status", PLy_result_status, METH_VARARGS, NULL},
	{NULL, NULL, 0, NULL}
};

PyTypeObject PLy_ResultType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"PLyResult",				/* tp_name */
	sizeof(PLyResultObject),	/* tp_size */
	0,							/* tp_itemsize */

	/*
	 * methods
	 */
	PLy_result_dealloc,			/* tp_dealloc */
	0,							/* tp_print */
	0,							/* tp_getattr */
	0,							/* tp_setattr */
	0,							/* tp_compare */
	0,							/* tp_repr */
	0,							/* tp_as_number */
	&PLy_result_as_sequence,	/* tp_as_sequence */
	0,							/* tp_as_mapping */
	0,							/* tp_hash */
	0,							/* tp_call */
	0,							/* tp_str */
	0,							/* tp_getattro */
	0,							/* tp_setattro */
	0,							/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,	/* tp_flags */
	PLy_result_doc,				/* tp_doc */
	0,							/* tp_traverse */
	0,							/* tp_clear */
	0,							/* tp_richcompare */
	0,							/* tp_weaklistoffset */
	0,							/* tp_iter */
	0,							/* tp_iternext */
	PLy_result_methods,			/* tp_tpmethods */
};

/* result object methods */

static PyObject *
PLy_result_new(void)
{
	PLyResultObject *ob;

	if ((ob = PyObject_New(PLyResultObject, &PLy_ResultType)) == NULL)
		return NULL;

	/* ob->tuples = NULL; */

	Py_INCREF(Py_None);
	ob->status = Py_None;
	ob->nrows = PyInt_FromLong(-1);
	ob->rows = PyList_New(0);

	return (PyObject *) ob;
}

static void
PLy_result_dealloc(PyObject *arg)
{
	PLyResultObject *ob = (PLyResultObject *) arg;

	Py_XDECREF(ob->nrows);
	Py_XDECREF(ob->rows);
	Py_XDECREF(ob->status);

	arg->ob_type->tp_free(arg);
}

static PyObject *
PLy_result_nrows(PyObject *self, PyObject *args UNUSED)
{
	PLyResultObject *ob = (PLyResultObject *) self;

	Py_INCREF(ob->nrows);
	return ob->nrows;
}

static PyObject *
PLy_result_status(PyObject *self, PyObject *args UNUSED)
{
	PLyResultObject *ob = (PLyResultObject *) self;

	Py_INCREF(ob->status);
	return ob->status;
}

static Py_ssize_t
PLy_result_length(PyObject *arg)
{
	PLyResultObject *ob = (PLyResultObject *) arg;

	return PyList_Size(ob->rows);
}

static PyObject *
PLy_result_item(PyObject *arg, Py_ssize_t idx)
{
	PyObject   *rv;
	PLyResultObject *ob = (PLyResultObject *) arg;

	rv = PyList_GetItem(ob->rows, idx);
	if (rv != NULL)
		Py_INCREF(rv);
	return rv;
}

static int
PLy_result_ass_item(PyObject *arg, Py_ssize_t idx, PyObject *item)
{
	int			rv;
	PLyResultObject *ob = (PLyResultObject *) arg;

	Py_INCREF(item);
	rv = PyList_SetItem(ob->rows, idx, item);
	return rv;
}

static PyObject *
PLy_result_slice(PyObject *arg, Py_ssize_t lidx, Py_ssize_t hidx)
{
	PLyResultObject *ob = (PLyResultObject *) arg;

	return PyList_GetSlice(ob->rows, lidx, hidx);
}

static int
PLy_result_ass_slice(PyObject *arg, Py_ssize_t lidx, Py_ssize_t hidx, PyObject *slice)
{
	int			rv;
	PLyResultObject *ob = (PLyResultObject *) arg;

	rv = PyList_SetSlice(ob->rows, lidx, hidx, slice);
	return rv;
}

/* Python objects */
typedef struct PLyPlanObject {
	PyObject_HEAD
	void *pplan; /* Store the pointer to plan on the QE side. */
	plcDatatype *argtypes;
	int32 nargs;
} PLyPlanObject;

#define is_PLyPlanObject(x) ((x)->ob_type == &PLy_PlanType)

/* PLyPlanObject, PLyResultObject and SPI interface */
static PyObject *PLy_plan_new(void);

static void PLy_plan_dealloc(PyObject *);

static PyObject *PLy_plan_status(PyObject *, PyObject *);

/* some globals for the python module */
static char PLy_plan_doc[] = {
	"Store a PostgreSQL plan"
};

static PyMethodDef PLy_plan_methods[] = {
	{"status", PLy_plan_status, METH_VARARGS, NULL},
	{NULL, NULL, 0,                           NULL}
};

PyTypeObject PLy_PlanType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"PLyPlan",                    /* tp_name */
	sizeof(PLyPlanObject),        /* tp_size */
	0,                            /* tp_itemsize */

	/*
	 * methods
	 */
	PLy_plan_dealloc,            /* tp_dealloc */
	0,                            /* tp_print */
	0,                            /* tp_getattr */
	0,                            /* tp_setattr */
	0,                            /* tp_compare */
	0,                            /* tp_repr */
	0,                            /* tp_as_number */
	0,                            /* tp_as_sequence */
	0,                            /* tp_as_mapping */
	0,                            /* tp_hash */
	0,                            /* tp_call */
	0,                            /* tp_str */
	0,                            /* tp_getattro */
	0,                            /* tp_setattro */
	0,                            /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,    /* tp_flags */
	PLy_plan_doc,                /* tp_doc */
	0,                            /* tp_traverse */
	0,                            /* tp_clear */
	0,                            /* tp_richcompare */
	0,                            /* tp_weaklistoffset */
	0,                            /* tp_iter */
	0,                            /* tp_iternext */
	PLy_plan_methods,            /* tp_tpmethods */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
};

/* plan object methods */
static PyObject *
PLy_plan_new(void) {
	PLyPlanObject *ob;

	if ((ob = PyObject_New(PLyPlanObject, &PLy_PlanType)) == NULL)
		return NULL;

	ob->pplan = NULL;
	ob->nargs = 0;
	ob->argtypes = NULL;

	return (PyObject *) ob;
}

/* SPI_freeplan(ob->pplan) */
static int PLy_freeplan(PLyPlanObject *ob) {
	int res;
	plcMsgSQL msg;
	plcMessage *resp;
	plcConn *conn = plcconn_global;

	/* If the execution was terminated we don't need to proceed with SPI */
	if (plc_is_execution_terminated != 0) {
		return -1;
	}

	msg.msgtype = MT_SQL;
	msg.sqltype = SQL_TYPE_UNPREPARE;
	msg.pplan = ob->pplan;

	plcontainer_channel_send(conn, (plcMessage *) &msg);
	/* No need to free for msg after tx. */

	res = plcontainer_channel_receive(conn, &resp, MT_RAW_BIT);
	if (res < 0) {
		raise_execution_error("Error receiving data from the frontend, %d", res);
		return res;
	}

	int32 *prv;
	prv = (int32 *) (((plcMsgRaw *) resp)->data);
	res = *prv;

	free_rawmsg((plcMsgRaw *) resp);
	return 0;
}

static void
PLy_plan_dealloc(PyObject *arg) {
	PLyPlanObject *ob = (PLyPlanObject *) arg;

	PLy_freeplan(ob);
	if (ob->argtypes)
		pfree(ob->argtypes);

	arg->ob_type->tp_free(arg);
}

/* Py_INCREF(Py_True) will lead to "strict-aliasing" warning, so workaround. */
#pragma GCC diagnostic ignored "-Wstrict-aliasing"

static PyObject *
PLy_plan_status(PyObject *self __attribute__((unused)), PyObject *args) {
	if (PyArg_ParseTuple(args, "")) {
		Py_INCREF(Py_True);
		return Py_True;
		/* return PyInt_FromLong(self->status); */
	}
	raise_execution_error("plan.status takes no arguments");
	return NULL;
}

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma GCC diagnostic pop /* It is supported since gcc 4.6. */
#endif

static plcMessage *receive_from_frontend() {
	plcMessage *resp = NULL;
	int res = 0;
	plcConn *conn = plcconn_global;

	res = plcontainer_channel_receive(conn, &resp, MT_CALLREQ_BIT | MT_RESULT_BIT | MT_SUBTRAN_RESULT_BIT);
	if (res < 0) {
		raise_execution_error("Error receiving data from the frontend, %d", res);
		return NULL;
	}

	switch (resp->msgtype) {
		case MT_CALLREQ:
			handle_call((plcMsgCallreq *) resp, conn);
			free_callreq((plcMsgCallreq *) resp, false, false);
			return receive_from_frontend();
		case MT_RESULT:
			break;
		case MT_SUBTRAN_RESULT:
			break;
		default:
			raise_execution_error("Client cannot process message type %c.\n"
				                      "Should never reach here.", resp->msgtype);
			return NULL;
	}
	return (plcMessage *) resp;
}

/* execute(query="select * from foo", limit=5)
 * execute(plan=plan, values=(foo, bar), limit=5)
 */
PyObject *
PLy_spi_execute(PyObject *self UNUSED, PyObject *args) {
	char *query;
	PyObject *plan;
	PyObject *list = NULL;
	long limit = 0;

	/* If the execution was terminated we don't need to proceed with SPI */
	if (plc_is_execution_terminated != 0) {
		return NULL;
	}

	if (PyArg_ParseTuple(args, "s|l", &query, &limit))
		return PLy_spi_execute_query(query, limit);

	PyErr_Clear();

	if (PyArg_ParseTuple(args, "O|Ol", &plan, &list, &limit) &&
	    is_PLyPlanObject(plan))
		return PLy_spi_execute_plan(plan, list, limit);

	PLy_exception_set(PLy_exc_spi_error, "plpy.execute expected a query or a plan");
	return NULL;
}

static PyObject *
PLy_spi_execute_fetch_result(plcMsgResult *resp)
{
	plcPyResult *obj;
	PLyResultObject *result;
	PyObject *pydict, *pyval;
	uint32 i, j;

	result = (PLyResultObject *) PLy_result_new();
	Py_DECREF(result->status);
	result->status = PyInt_FromLong(1);

	if (resp->cols == 0)
	{
		plc_elog(DEBUG1, "the rows is %d", resp->rows);
		Py_DECREF(result->nrows);
		result->nrows = PyInt_FromLong((long) resp->rows);
		free_result(resp, false);
		return (PyObject *) result;
	}

	obj = plc_init_result_conversions(resp);

	Py_DECREF(result->nrows);
	result->nrows = PyInt_FromLong(obj->res->rows);

	for (j = 0; j < obj->res->cols; j++) {
		if (obj->args[j].conv.inputfunc == NULL) {
			PLy_exception_set(PyExc_TypeError,
					"Type %d is not yet supported by Python container",
					(int) obj->args[j].type);
			Py_DECREF(result);
			result = NULL;
			goto ret;
		}
	}
	for (i = 0; i < obj->res->rows; i++) {
		if (i == 0) {
			Py_DECREF(result->rows);
			result->rows = PyList_New(obj->res->rows);
		}
		pydict = PyDict_New();

		if (pydict == NULL) {
			raise_execution_error("Can not allocate a dict for python spi");
			Py_DECREF(result);
			result = NULL;
			goto ret;
		}

		for (j = 0; j < obj->res->cols; j++) {
			if (obj->res->data[i][j].isnull) {
				/* FIXME: handle the error case. */
				PyDict_SetItemString(pydict, obj->res->names[j], Py_None);
			} else {
				pyval = obj->args[j].conv.inputfunc(obj->res->data[i][j].value,
						&obj->args[j]);

				if (PyDict_SetItemString(pydict, obj->res->names[j], pyval)
						!= 0) {
					raise_execution_error(
							"Error setting result dictionary element",
							(int) obj->res->types[j].type);
					Py_XDECREF(pyval);
					Py_DECREF(pydict);
					Py_DECREF(result);
					result = NULL;
					goto ret;
				}
				Py_XDECREF(pyval);
			}
		}
		PyList_SetItem(result->rows, i, pydict);
	}
	ret:
	// Call plc_free_result_conversions before resp is freed because we use resp.cols in plc_free_result_conversions.
	plc_free_result_conversions(obj);
	free_result(resp, false);

	return (PyObject *) result;
}

static PyObject *
PLy_spi_execute_plan(PyObject *ob, PyObject *list, long limit) {
	uint32 j;
	int32 nargs;
	plcMsgSQL msg;
	plcMsgResult *resp;
	plcConn *conn = plcconn_global;
	plcArgument *args;
	PLyPlanObject *py_plan;

	if (list != NULL) {
		if (!PySequence_Check(list) || PyString_Check(list) || PyUnicode_Check(list)) {
			PLy_exception_set(PyExc_TypeError, "plpy.execute takes a sequence as its second argument");
			return NULL;
		}
		nargs = PySequence_Length(list);
	} else
		nargs = 0;

	py_plan = (PLyPlanObject *) ob;

	if (py_plan->nargs != nargs) {
		PLy_exception_set(PyExc_TypeError, "plpy.execute takes bad argument number: %d vs expected %d",
			nargs, py_plan->nargs);
		return NULL;
	}

	if (nargs > 0)
		args = pmalloc(sizeof(plcArgument) * nargs);
	else
		args = NULL;
	for (j = 0; j < (uint32) nargs; j++) {
		PyObject *elem;
		args[j].type.type = py_plan->argtypes[j];
		args[j].name = NULL; /* We do not need name */
		args[j].type.nSubTypes = 0;
		args[j].type.typeName = NULL;
		args[j].data.value = NULL;

		elem = PySequence_GetItem(list, j);
		if (elem != Py_None) {
			args[j].data.isnull = 0;
			if (Ply_get_output_function(py_plan->argtypes[j])(elem, &args[j].data.value, NULL) < 0) {
				/* Free allocated memory. */
				free_arguments(args, j + 1, false, false);
				Py_DECREF(elem);
				PLy_exception_set(PyExc_TypeError, "Failed to convert data in pexecute");
				return NULL;
			}
		} else {
			/* FIXME: Wrong ? */
			args[j].data.isnull = 1;
		}
	}

	msg.msgtype = MT_SQL;
	msg.sqltype = SQL_TYPE_PEXECUTE;
	msg.pplan = py_plan->pplan;
	msg.limit = limit;
	msg.nargs = nargs;
	msg.args = args;

	plcontainer_channel_send(conn, (plcMessage *) &msg);
	free_arguments(args, nargs, false, false);

	resp = (plcMsgResult *) receive_from_frontend();
	if (resp == NULL) {
		PLy_exception_set(PLy_exc_spi_error, "Error receiving data from frontend");
		return NULL;
	}

	return PLy_spi_execute_fetch_result(resp);
}

PyObject *
PLy_subtransaction(PyObject *self UNUSED, PyObject *unused UNUSED) {
	return PLy_subtransaction_new();
}

/* Allocate and initialize a PLySubtransactionObject */
static PyObject *
PLy_subtransaction_new(void) {
	PLySubtransactionObject *ob;

	ob = PyObject_New(PLySubtransactionObject, &PLy_SubtransactionType);

	if (ob == NULL)
		return NULL;

	ob->started = false;
	ob->exited = false;

	return (PyObject *) ob;
}

/* Python requires a dealloc function to be defined
 */
static void
PLy_subtransaction_dealloc(PyObject *subxact UNUSED) {
}


/*
 * TODO: send the message and execute PLy_subtransaction_enter on QE
 *
 * subxact.__enter__() or subxact.enter()
 *
 * Start an explicit subtransaction.  SPI calls within an explicit
 * subtransaction will not start another one, so you can atomically
 * execute many SPI calls and still get a controllable exception if
 * one of them fails.
 */
static PyObject *
PLy_subtransaction_enter(PyObject *self, PyObject *unused UNUSED) {

	plc_elog(DEBUG1, "Subtransaction enter");
	plcConn *conn = plcconn_global;
	PLySubtransactionObject *subxact = (PLySubtransactionObject *) self;

	if (subxact->started) {
		PLy_exception_set(PyExc_ValueError, "this subtransaction has already been entered");
		return NULL;
	}

	if (subxact->exited) {
		PLy_exception_set(PyExc_ValueError, "this subtransaction has already been exited");
		return NULL;
	}

	subxact->started = true;

	plcMsgSubtransaction msg;
	plcMsgSubtransactionResult *resp;


	msg.msgtype = MT_SUBTRANSACTION;
	msg.action = 'n'; /*set operation to enter 'n' */
	msg.type = 'n';    /* for enter, type is useless */

	plcontainer_channel_send(conn, (plcMessage *) &msg);
	resp = (plcMsgSubtransactionResult *) receive_from_frontend();
	if (resp == NULL) {
		raise_execution_error("Error receiving data from frontend");
		return NULL;
	}
	switch (resp->result) {
		case SUCCESS:
			break;
		case NO_SUBTRANSACTION_ERROR:
		case RELEASE_SUBTRANSACTION_ERROR:
			raise_execution_error("Error receiving subtransaction exit error message");
			break;
		case CREATE_SUBTRANSACTION_ERROR:
			raise_execution_error("Error when beginning subtransaction on QE side");
			break;
		default:
			raise_execution_error("Error receiving unknown subtransaction error message");
			break;
	}

	Py_INCREF(self);
	return self;

}

/*
 * TODO: send the message and execute PLy_subtransaction_exit on QE
 *
 * subxact.__exit__(exc_type, exc, tb) or subxact.exit(exc_type, exc, tb)
 *
 * Exit an explicit subtransaction. exc_type is an exception type, exc
 * is the exception object, tb is the traceback.  If exc_type is None,
 * commit the subtransactiony, if not abort it.
 *
 * The method signature is chosen to allow subtransaction objects to
 * be used as context managers as described in
 * <http://www.python.org/dev/peps/pep-0343/>.
 *
 */
static PyObject *
PLy_subtransaction_exit(PyObject *self, PyObject *args) {
	plc_elog(DEBUG1, "Subtransaction exit");
	plcConn *conn = plcconn_global;
	PyObject *type;
	PyObject *value;
	PyObject *traceback;
	PLySubtransactionObject *subxact = (PLySubtransactionObject *) self;

	if (!PyArg_ParseTuple(args, "OOO", &type, &value, &traceback))
		return NULL;

	if (!subxact->started) {
		PLy_exception_set(PyExc_ValueError, "this subtransaction has not been entered");
		return NULL;
	}

	if (subxact->exited) {
		PLy_exception_set(PyExc_ValueError, "this subtransaction has already been exited");
		return NULL;
	}

	subxact->exited = true;


	plcMsgSubtransaction msg;
	plcMsgSubtransactionResult *resp;


	msg.msgtype = MT_SUBTRANSACTION;
	msg.action = 'x'; /*set operation to enter 'n' */
	if (type != Py_None) {
		msg.type = 'n';
	} else {
		msg.type = 'e';
	}
	plcontainer_channel_send(conn, (plcMessage *) &msg);
	resp = (plcMsgSubtransactionResult *) receive_from_frontend();
	if (resp == NULL) {
		raise_execution_error("Error receiving data from frontend");
		return NULL;
	}
	switch (resp->result) {
		case SUCCESS:
			break;
		case NO_SUBTRANSACTION_ERROR:
			raise_execution_error("Error there is no opened subtransaction");
			break;
		case RELEASE_SUBTRANSACTION_ERROR:
			raise_execution_error("Error when releasing subtransaction on QE side");
			break;
		case CREATE_SUBTRANSACTION_ERROR:
			raise_execution_error("Error receiving subtransaction enter error message");
			break;
		default:
			raise_execution_error("Error receiving unknown subtransaction error message");
			break;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
PLy_spi_execute_query(char *query, long limit) {
	plcMsgSQL msg;
	plcMsgResult *resp;
	plcConn *conn = plcconn_global;

	msg.msgtype = MT_SQL;
	msg.sqltype = SQL_TYPE_STATEMENT;
	msg.limit = limit;
	msg.statement = query;

	plcontainer_channel_send(conn, (plcMessage *) &msg);

	resp = (plcMsgResult *) receive_from_frontend();
	if (resp == NULL) {
		raise_execution_error("Error receiving data from frontend");
		return NULL;
	}

	return PLy_spi_execute_fetch_result(resp);
}

PyObject *PLy_spi_prepare(PyObject *self UNUSED, PyObject *args) {
	int i;
	plcMsgSQL msg;
	plcMessage *resp;
	plcConn *conn = plcconn_global;
	char *query;
	int nargs, res;
	PLyPlanObject *py_plan;
	PyObject *list = NULL;
	PyObject *optr = NULL;

	/* If the execution was terminated we don't need to proceed with SPI */
	if (plc_is_execution_terminated != 0) {
		return NULL;
	}

	if (!PyArg_ParseTuple(args, "s|O", &query, &list))
		return NULL;

	if (list && (!PySequence_Check(list))) {
		PLy_exception_set(PyExc_TypeError,
								  "second argument of plpy.prepare must be a sequence");
		return NULL;
	}

	nargs = list ? PySequence_Length(list) : 0;

	msg.msgtype = MT_SQL;
	msg.sqltype = SQL_TYPE_PREPARE;
	msg.nargs = nargs;
	msg.statement = query;
	if (msg.nargs > 0)
		msg.args = pmalloc(msg.nargs * sizeof(plcArgument));
	else if (msg.nargs < 0) {
		raise_execution_error("plpy.prepare: arg number (%d) < 0. Impossible!"
			                      " Probably there is a code bug.", msg.nargs);
		return NULL;
	} else
		msg.args = NULL;
	for (i = 0; i < msg.nargs; i++) {
		char *sptr;

		optr = PySequence_GetItem(list, i);
		if (PyString_Check(optr))
			sptr = (char*) PyString_AsString(optr);
#if 0
			/* FIXME: add PLyUnicode_AsString() */
			else if (PyUnicode_Check(optr))
				sptr = PLyUnicode_AsString(optr);
#endif
		else {
			free_arguments(msg.args, i, false, false);
			raise_execution_error("plpy.prepare: type name at ordinal position %d is not a string", i);
			return NULL;
		}
		fill_prepare_argument(&msg.args[i], sptr, PLC_DATA_TEXT);
	}

	plcontainer_channel_send(conn, (plcMessage *) &msg);
	free_arguments(msg.args, msg.nargs, false, false);

	res = plcontainer_channel_receive(conn, &resp, MT_RAW_BIT);
	if (res < 0) {
		PLy_exception_set(PLy_exc_spi_error, "Error receiving data from the frontend, %d", res);
		return NULL;
	}

	char *start;
	int offset, tx_len;
	int is_plan_valid;

	offset = 0;
	start = ((plcMsgRaw *) resp)->data;
	tx_len = ((plcMsgRaw *) resp)->size;

	if ((py_plan = (PLyPlanObject *) PLy_plan_new()) == NULL) {
		raise_execution_error("Fail to create a plan object");
		free_rawmsg((plcMsgRaw *) resp);
		return NULL;
	}
	is_plan_valid = (*((int32 *) (start + offset)));
	offset += sizeof(int32);
	if (!is_plan_valid) {
		raise_execution_error("plpy.prepare failed. See backend for details.");
		free_rawmsg((plcMsgRaw *) resp);
		return NULL;
	}
	py_plan->pplan = (void *) (*((int64 *) (start + offset)));
	offset += sizeof(int64);
	py_plan->nargs = *((int32 *) (start + offset));
	offset += sizeof(int32);
	if (py_plan->nargs != nargs) {
		raise_execution_error("plpy.prepare: bad argument number: %d "
			                      "(returned) vs %d (expected).", py_plan->nargs, nargs);
		free_rawmsg((plcMsgRaw *) resp);
		return NULL;
	}

	if (nargs > 0) {
		if (offset + (signed int) sizeof(plcDatatype) * nargs != tx_len) {
			raise_execution_error("Client format error for spi prepare. "
				                      "Calculated length (%d) vs transferred length (%d)",
			                      offset + sizeof(plcDatatype) * nargs, tx_len);
			free_rawmsg((plcMsgRaw *) resp);
			return NULL;
		}

		py_plan->argtypes = malloc(sizeof(plcDatatype) * nargs);
		if (py_plan->argtypes == NULL) {
			raise_execution_error("Could not allocate %d bytes for argtypes"
				                      " in py_plan", sizeof(plcDatatype) * nargs);
			free_rawmsg((plcMsgRaw *) resp);
			return NULL;
		}
		memcpy(py_plan->argtypes, start + offset, sizeof(plcDatatype) * nargs);
	}

	free_rawmsg((plcMsgRaw *) resp);
	return (PyObject *) py_plan;
}

/*
 * Call PyErr_SetString with a vprint interface and translation support
 */
static void
PLy_exception_set(PyObject *exc, const char *fmt, ...) {
	char buf[1024];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), dgettext(TEXTDOMAIN, fmt), ap);
	va_end(ap);

	plc_elog(DEBUG1, "Python caught an exception: %s", buf);

	PyErr_SetString(exc, buf);
}

