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

static PyTypeObject PLy_SubtransactionType = {
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

PyObject *plpy_execute(PyObject *self UNUSED, PyObject *args);

static plcMsgResult *receive_from_backend();

static plcMsgResult *receive_from_backend() {
    plcMessage *resp = NULL;
    int         res = 0;
    plcConn    *conn = plcconn_global;

    res = plcontainer_channel_receive(conn, &resp);
    if (res < 0) {
        raise_execution_error("Error receiving data from the backend, %d", res);
        return NULL;
    }

    switch (resp->msgtype) {
        case MT_CALLREQ:
            handle_call((plcMsgCallreq*)resp, conn);
            free_callreq((plcMsgCallreq*)resp, false, false);
            return receive_from_backend();
        case MT_RESULT:
            break;
        default:
            raise_execution_error("Client cannot process message type %c", resp->msgtype);
            return NULL;
    }
    return (plcMsgResult*)resp;
}

/* plpy methods */
PyObject *PLy_spi_execute(PyObject *self UNUSED, PyObject *args) {
    int           i, j;
    plcMsgSQL    *msg;
    plcMsgResult *resp;
    PyObject     *pyresult,
                 *pydict,
                 *pyval;
    plcPyResult  *result;
    plcConn      *conn = plcconn_global;
	char       *query;
	long        limit = 0;

    /* If the execution was terminated we don't need to proceed with SPI */
    if (plc_is_execution_terminated != 0) {
        return NULL;
    }

	if (!PyArg_ParseTuple(args, "s|l", &query, &limit)) {
		raise_execution_error("Argument error for plpy module 'execute()'");
		return NULL;
	}

    msg            = malloc(sizeof(plcMsgSQL));
    msg->msgtype   = MT_SQL;
    msg->sqltype   = SQL_TYPE_STATEMENT;
	msg->limit     = limit;
    msg->statement = query;

    plcontainer_channel_send(conn, (plcMessage*)msg);

    /* we don't need it anymore */
    free(msg);

    resp = receive_from_backend();
    if (resp == NULL) {
        raise_execution_error("Error receiving data from backend");
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
