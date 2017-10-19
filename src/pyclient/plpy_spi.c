/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */
#include "plpy_spi.h"

#include "common/comm_channel.h"
#include "common/comm_utils.h"
#include "pycall.h"
#include "pyerror.h"
#include "pyconversions.h"

#include <Python.h>

typedef struct PLySubtransactionObject
{
	PyObject_HEAD
	bool		started;
	bool		exited;
} PLySubtransactionObject;


static PyObject *PLy_subtransaction_new(void);
static void PLy_subtransaction_dealloc(PyObject *);

static PyObject *PLy_subtransaction_enter(PyObject *, PyObject *);
static PyObject *PLy_subtransaction_exit(PyObject *, PyObject *);


static char PLy_subtransaction_doc[] = {
	"PostgreSQL subtransaction context manager"
};


static PyMethodDef PLy_subtransaction_methods[] = {
	{"__enter__", PLy_subtransaction_enter, METH_VARARGS, NULL},
	{"__exit__", PLy_subtransaction_exit, METH_VARARGS, NULL},
	/* user-friendly names for Python <2.6 */
	{"enter", PLy_subtransaction_enter, METH_VARARGS, NULL},
	{"exit", PLy_subtransaction_exit, METH_VARARGS, NULL},
	{NULL, NULL, 0, NULL}
};

PyTypeObject PLy_SubtransactionType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"PLySubtransaction",		/* tp_name */
	sizeof(PLySubtransactionObject),	/* tp_size */
	0,							/* tp_itemsize */

	/*
	 * methods
	 */
	PLy_subtransaction_dealloc, /* tp_dealloc */
	0,							/* tp_print */
	0,							/* tp_getattr */
	0,							/* tp_setattr */
	0,							/* tp_compare */
	0,							/* tp_repr */
	0,							/* tp_as_number */
	0,							/* tp_as_sequence */
	0,							/* tp_as_mapping */
	0,							/* tp_hash */
	0,							/* tp_call */
	0,							/* tp_str */
	0,							/* tp_getattro */
	0,							/* tp_setattro */
	0,							/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,	/* tp_flags */
	PLy_subtransaction_doc,		/* tp_doc */
	0,							/* tp_traverse */
	0,							/* tp_clear */
	0,							/* tp_richcompare */
	0,							/* tp_weaklistoffset */
	0,							/* tp_iter */
	0,							/* tp_iternext */
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
PyObject *PLy_spi_execute(PyObject *self, PyObject *pyquery);
PyObject *PLy_spi_prepare(PyObject *self, PyObject *args);

/* Python objects */
typedef struct PLyPlanObject
{
	PyObject_HEAD
	void        *pplan; /* Store the pointer to plan on the QE side. */
	plcDatatype *argtypes;
	int          nargs;
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
	{NULL, NULL, 0, NULL}
};

PyTypeObject PLy_PlanType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"PLyPlan",					/* tp_name */
	sizeof(PLyPlanObject),		/* tp_size */
	0,							/* tp_itemsize */

	/*
	 * methods
	 */
	PLy_plan_dealloc,			/* tp_dealloc */
	0,							/* tp_print */
	0,							/* tp_getattr */
	0,							/* tp_setattr */
	0,							/* tp_compare */
	0,							/* tp_repr */
	0,							/* tp_as_number */
	0,							/* tp_as_sequence */
	0,							/* tp_as_mapping */
	0,							/* tp_hash */
	0,							/* tp_call */
	0,							/* tp_str */
	0,							/* tp_getattro */
	0,							/* tp_setattro */
	0,							/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,	/* tp_flags */
	PLy_plan_doc,				/* tp_doc */
	0,							/* tp_traverse */
	0,							/* tp_clear */
	0,							/* tp_richcompare */
	0,							/* tp_weaklistoffset */
	0,							/* tp_iter */
	0,							/* tp_iternext */
	PLy_plan_methods,			/* tp_tpmethods */
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
PLy_plan_new(void)
{
	PLyPlanObject *ob;

	if ((ob = PyObject_New(PLyPlanObject, &PLy_PlanType)) == NULL)
		return NULL;

	ob->pplan = NULL;
	ob->nargs = 0;
	ob->argtypes = NULL;

	return (PyObject *) ob;
}

/* SPI_freeplan(ob->pplan) */
static int PLy_freeplan(PLyPlanObject *ob)
{
    int            res;
    plcMsgSQL      msg;
    plcMessage    *resp;
    plcConn       *conn = plcconn_global;

    /* If the execution was terminated we don't need to proceed with SPI */
    if (plc_is_execution_terminated != 0) {
        return -1;
    }

    msg.msgtype   = MT_SQL;
    msg.sqltype   = SQL_TYPE_UNPREPARE;
	msg.pplan     = ob->pplan;

    plcontainer_channel_send(conn, (plcMessage*) &msg);
	/* No need to free for msg after tx. */

    res = plcontainer_channel_receive(conn, &resp, MT_RAW_BIT);
    if (res < 0) {
        raise_execution_error("Error receiving data from the frontend, %d", res);
        return res;
    }

	int32 *prv;
	prv = (int32 *)(((plcMsgRaw *)resp)->data);
	res = *prv;

    free_rawmsg((plcMsgRaw *) resp);
    return 0;
}

static void
PLy_plan_dealloc(PyObject *arg)
{
	PLyPlanObject *ob = (PLyPlanObject *) arg;

	PLy_freeplan(ob);
	if (ob->argtypes)
		pfree(ob->argtypes);

	arg->ob_type->tp_free(arg);
}

/* Py_INCREF(Py_True) will lead to "strict-aliasing" warning, so workaround. */
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
static PyObject *
PLy_plan_status(PyObject *self __attribute__((unused)), PyObject *args)
{
	if (PyArg_ParseTuple(args, ""))
	{
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

static plcMsgResult *receive_from_frontend() {
    plcMessage *resp = NULL;
    int         res = 0;
    plcConn    *conn = plcconn_global;

    res = plcontainer_channel_receive(conn, &resp, MT_CALLREQ_BIT|MT_RESULT_BIT);
    if (res < 0) {
        raise_execution_error("Error receiving data from the frontend, %d", res);
        return NULL;
    }

    switch (resp->msgtype) {
        case MT_CALLREQ:
            handle_call((plcMsgCallreq*)resp, conn);
            free_callreq((plcMsgCallreq*)resp, false, false);
            return receive_from_frontend();
        case MT_RESULT:
            break;
        default:
            raise_execution_error("Client cannot process message type %c.\n"
								  "Should never reach here.", resp->msgtype);
            return NULL;
    }
    return (plcMsgResult*)resp;
}

/* execute(query="select * from foo", limit=5)
 * execute(plan=plan, values=(foo, bar), limit=5)
 */
PyObject *
PLy_spi_execute(PyObject *self UNUSED, PyObject *args)
{
	char	   *query;
	PyObject   *plan;
	PyObject   *list = NULL;
	long		limit = 0;

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

	raise_execution_error("plpy.execute expected a query or a plan");
	return NULL;
}

static PyObject *
PLy_spi_execute_plan(PyObject *ob, PyObject *list, long limit) {
    int           i, j, nargs;
    plcMsgSQL     msg;
    plcMsgResult *resp;
    PyObject     *pyresult,
                 *pydict,
                 *pyval;
    plcPyResult  *result;
    plcConn      *conn = plcconn_global;
	plcArgument  *args;
	PLyPlanObject *py_plan;

	if (list != NULL)
	{
		if (!PySequence_Check(list) || PyString_Check(list) || PyUnicode_Check(list))
		{
			raise_execution_error("plpy.execute takes a sequence as its second argument");
			return NULL;
		}
		nargs = PySequence_Length(list);
	}
	else
		nargs = 0;

	py_plan = (PLyPlanObject *) ob;

	if (py_plan->nargs != nargs) {
		raise_execution_error("plpy.execute takes bad argument number: %d vs expected %d", nargs, py_plan->nargs);
		return NULL;
	}

	if (nargs > 0)
		args = pmalloc(sizeof(plcArgument) * nargs);
	else
		args = NULL;
	for (j = 0; j < nargs; j++)
	{
		PyObject    *elem;
		args[j].type.type = py_plan->argtypes[j];
		args[j].name = NULL; /* We do not need name */
		args[j].type.nSubTypes = 0;
		args[j].type.typeName = NULL;
		args[j].data.value = NULL;

		elem = PySequence_GetItem(list, j);
		if (elem != Py_None)
		{
			args[j].data.isnull = 0;
			if (Ply_get_output_function(py_plan->argtypes[j])(elem, &args[j].data.value, NULL) < 0) {
				/* Free allocated memory. */
				free_arguments(args, j + 1, false, false);
				raise_execution_error("Failed to convert data in pexecute");
				return NULL;
			}
		} else {
			/* FIXME: Wrong ? */
			args[j].data.isnull = 1;
		}
	}

    msg.msgtype   = MT_SQL;
    msg.sqltype   = SQL_TYPE_PEXECUTE;
	msg.pplan     = py_plan->pplan;
	msg.limit     = limit;
	msg.nargs     = nargs;
	msg.args      = args;

    plcontainer_channel_send(conn, (plcMessage*) &msg);
	free_arguments(args, nargs, false, false);

    resp = receive_from_frontend();
    if (resp == NULL) {
        raise_execution_error("Error receiving data from frontend");
        return NULL;
    }

    result = plc_init_result_conversions(resp);

    /* convert the result set into list of dictionaries */
    pyresult = PyList_New(result->res->rows);
    if (pyresult == NULL) {
        raise_execution_error("Cannot allocate new list object in Python");
        free_result(resp, false);
        plc_free_result_conversions(result);
        return NULL;
    }

    for (j = 0; j < result->res->cols; j++) {
        if (result->args[j].conv.inputfunc == NULL) {
            raise_execution_error("Type %d is not yet supported by Python container",
                                  (int)result->args[j].type);
            free_result(resp, false);
            plc_free_result_conversions(result);
            return NULL;
        }
    }

    for (i = 0; i < result->res->rows; i++) {
        pydict = PyDict_New();

        for (j = 0; j < result->res->cols; j++) {
            pyval = result->args[j].conv.inputfunc(result->res->data[i][j].value,
                                                   &result->args[j]);

            if (PyDict_SetItemString(pydict, result->res->names[j], pyval) != 0) {
                raise_execution_error("Error setting result dictionary element",
                                      (int)result->res->types[j].type);
                free_result(resp, false);
                plc_free_result_conversions(result);
                return NULL;
            }
        }

        if (PyList_SetItem(pyresult, i, pydict) != 0) {
            raise_execution_error("Error setting result list element",
                                  (int)result->res->types[j].type);
            free_result(resp, false);
            plc_free_result_conversions(result);
            return NULL;
        }
    }

    free_result(resp, false);
    plc_free_result_conversions(result);

    return pyresult;
}

PyObject *
PLy_subtransaction(PyObject *self UNUSED, PyObject *unused UNUSED)
{
	return PLy_subtransaction_new();
}

/* Allocate and initialize a PLySubtransactionObject */
static PyObject *
PLy_subtransaction_new(void)
{
	PLySubtransactionObject *ob;

	ob = PyObject_New(PLySubtransactionObject, &PLy_SubtransactionType);

	if (ob == NULL)
		return NULL;

	ob->started = false;
	ob->exited = false;

	return (PyObject *) ob;
}

/* Python requires a dealloc function to be defined */
static void
PLy_subtransaction_dealloc(PyObject *subxact UNUSED)
{
}


/*
 * TODO: send the message and execute PLy_subtransaction_enter on QE
 */
static PyObject *
PLy_subtransaction_enter(PyObject *self UNUSED, PyObject *unused UNUSED)
{
	return self;
}

/*
 * TODO: send the message and execute PLy_subtransaction_exit on QE
 */
static PyObject *
PLy_subtransaction_exit(PyObject *self UNUSED, PyObject *args UNUSED)
{
	return Py_None;
}

static PyObject *
PLy_spi_execute_query(char *query, long limit) {
    int           i, j;
    plcMsgSQL     msg;
    plcMsgResult *resp;
    PyObject     *pyresult,
                 *pydict,
                 *pyval;
    plcPyResult  *result;
    plcConn      *conn = plcconn_global;

    msg.msgtype   = MT_SQL;
    msg.sqltype   = SQL_TYPE_STATEMENT;
	msg.limit     = limit;
    msg.statement = query;

    plcontainer_channel_send(conn, (plcMessage*) &msg);

    resp = receive_from_frontend();
    if (resp == NULL) {
        raise_execution_error("Error receiving data from frontend");
        return NULL;
    }

    result = plc_init_result_conversions(resp);

    /* convert the result set into list of dictionaries */
    pyresult = PyList_New(result->res->rows);
    if (pyresult == NULL) {
        raise_execution_error("Cannot allocate new list object in Python");
        free_result(resp, false);
        plc_free_result_conversions(result);
        return NULL;
    }

    for (j = 0; j < result->res->cols; j++) {
        if (result->args[j].conv.inputfunc == NULL) {
            raise_execution_error("Type %d is not yet supported by Python container",
                                  (int)result->args[j].type);
            free_result(resp, false);
            plc_free_result_conversions(result);
            return NULL;
        }
    }

    for (i = 0; i < result->res->rows; i++) {
        pydict = PyDict_New();

        for (j = 0; j < result->res->cols; j++) {
            pyval = result->args[j].conv.inputfunc(result->res->data[i][j].value,
                                                   &result->args[j]);

            if (PyDict_SetItemString(pydict, result->res->names[j], pyval) != 0) {
                raise_execution_error("Error setting result dictionary element",
                                      (int)result->res->types[j].type);
                free_result(resp, false);
                plc_free_result_conversions(result);
                return NULL;
            }
        }

        if (PyList_SetItem(pyresult, i, pydict) != 0) {
            raise_execution_error("Error setting result list element",
                                  (int)result->res->types[j].type);
            free_result(resp, false);
            plc_free_result_conversions(result);
            return NULL;
        }
    }

    free_result(resp, false);
    plc_free_result_conversions(result);

    return pyresult;
}

PyObject *PLy_spi_prepare(PyObject *self UNUSED, PyObject *args) {
    int            i;
    plcMsgSQL      msg;
    plcMessage    *resp;
    plcConn       *conn = plcconn_global;
	char          *query;
    int            nargs, res;
	PLyPlanObject *py_plan;
	PyObject      *list = NULL;
	PyObject      *optr = NULL;

    /* If the execution was terminated we don't need to proceed with SPI */
    if (plc_is_execution_terminated != 0) {
        return NULL;
    }

	if (!PyArg_ParseTuple(args, "s|O", &query, &list))
		return NULL;

	if (list && (!PySequence_Check(list)))
	{
		raise_execution_error("second argument of plpy.prepare must be a sequence");
		return NULL;
	}

	nargs = list ? PySequence_Length(list) : 0;

    msg.msgtype   = MT_SQL;
    msg.sqltype   = SQL_TYPE_PREPARE;
	msg.nargs     = nargs;
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
			char	   *sptr;

			optr = PySequence_GetItem(list, i);
			if (PyString_Check(optr))
				sptr = PyString_AsString(optr);
#if 0
			/* FIXME: add PLyUnicode_AsString() */
			else if (PyUnicode_Check(optr))
				sptr = PLyUnicode_AsString(optr);
#endif
			else
			{
				free_arguments(msg.args, i, false, false);
				raise_execution_error("plpy.prepare: type name at ordinal position %d is not a string", i);
				return NULL;
			}
			fill_prepare_argument(&msg.args[i], sptr);
	}

	plcontainer_channel_send(conn, (plcMessage*) &msg);
	free_arguments(msg.args, msg.nargs, false, false);

    res = plcontainer_channel_receive(conn, &resp, MT_RAW_BIT);
    if (res < 0) {
        raise_execution_error("Error receiving data from the frontend, %d", res);
        return NULL;
    }

	char *start;
	int offset, tx_len;
	int is_plan_valid;

	offset = 0;
	start = ((plcMsgRaw *)resp)->data;
	tx_len = ((plcMsgRaw *)resp)->size;

	if ((py_plan = (PLyPlanObject *) PLy_plan_new()) == NULL) {
		raise_execution_error("Fail to create a plan object");
		free_rawmsg((plcMsgRaw *) resp);
		return NULL;
	}
	is_plan_valid = (*((int32 *) (start + offset))); offset += sizeof(int32);
	if (!is_plan_valid) {
		raise_execution_error("plpy.prepare failed. See backend for details.");
		free_rawmsg((plcMsgRaw *) resp);
		return NULL;
	}
	py_plan->pplan = (void *) (*((int64 *) (start + offset))); offset += sizeof(int64);
	py_plan->nargs = *((int32 *) (start + offset)); offset += sizeof(int32);
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
