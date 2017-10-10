/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */
#include "common/comm_channel.h"
#include "common/comm_utils.h"
#include "pycall.h"
#include "pyerror.h"
#include "pyconversions.h"

#include <Python.h>

PyObject *PLy_spi_prepare(PyObject *, PyObject *);
PyObject *PLy_spi_execute(PyObject *, PyObject *);
static PyObject *PLy_spi_execute_query(char *query, long limit);
static PyObject *PLy_spi_execute_plan(PyObject *, PyObject *, long);

/* FIXME: Use a python object instead. */
typedef struct Ply_plan {
	void        *pplan; /* Store the pointer to plan on the QE side. */
	plcDatatype *argtypes;
	int          nargs;
} Ply_plan;

static plcMsgResult *receive_from_frontend();

static plcMsgResult *receive_from_frontend() {
    plcMessage *resp = NULL;
    int         res = 0;
    plcConn    *conn = plcconn_global;

    res = plcontainer_channel_receive(conn, &resp);
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
            raise_execution_error("Client cannot process message type %c", resp->msgtype);
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
	void       *plan;
	PyObject   *list = NULL;
	long		limit = 0;

    /* If the execution was terminated we don't need to proceed with SPI */
    if (plc_is_execution_terminated != 0) {
        return NULL;
    }

	if (PyArg_ParseTuple(args, "s|l", &query, &limit))
		return PLy_spi_execute_query(query, limit);

	PyErr_Clear();

	if (PyArg_ParseTuple(args, "L|Ol", &plan, &list, &limit))
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
	Ply_plan     *py_plan;

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

	py_plan = (Ply_plan *) ob;

	if (py_plan->nargs != nargs) {
		raise_execution_error("plpy.execute takes bad argument number: %d vs expected %d", nargs, py_plan->nargs);
		return NULL;
	}

	args = malloc(sizeof(plcArgument) * nargs);
	for (j = 0; j < nargs; j++)
	{
		PyObject    *elem;
		args[j].name = NULL;
		args[j].type.type = py_plan->argtypes[j]; /* FIXME */

		elem = PySequence_GetItem(list, j);
		if (elem != Py_None)
		{
			args[j].data.isnull = 0;
			/* Fill plcPyType? Or Datatype? */
			Ply_get_output_function(py_plan->argtypes[j])(elem, &args[j].data.value, NULL);
			/* Double check type. false? */
		} else {
			args[j].data.isnull = 1;
			args[j].data.value = NULL;
		}
	}

    msg.msgtype   = MT_SQL;
    msg.sqltype   = SQL_TYPE_PEXECUTE;
	msg.pplan     = py_plan->pplan;
	msg.limit     = limit;
	msg.nargs     = nargs;
	msg.args      = args;

    plcontainer_channel_send(conn, (plcMessage*) &msg);
	/* FIXME: Release more */
	free(args);

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
    PyObject      *pyresult;
    plcConn       *conn = plcconn_global;
	char          *query;
    int            nargs, res;
	struct Ply_plan *py_plan;
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
	msg.args      = malloc(msg.nargs * sizeof(plcArgument));
	for (i = 0; i < msg.nargs; i++) {
			char	   *sptr;

			optr = PySequence_GetItem(list, i);
			if (PyString_Check(optr))
				sptr = PyString_AsString(optr);
				/* FIXME
			else if (PyUnicode_Check(optr))
				sptr = PLyUnicode_AsString(optr);
				*/
			else
			{
				raise_execution_error("plpy.prepare: type name at ordinal position %d is not a string", i);
				return NULL;
			}
			fill_prepare_argument(&msg.args[i], sptr);
	}

    plcontainer_channel_send(conn, (plcMessage*) &msg);
	/* FIXME: free msg */

    res = plcontainer_channel_receive(conn, &resp);
    if (res < 0) {
        raise_execution_error("Error receiving data from the frontend, %d", res);
        return NULL;
    }

	if (resp->msgtype == MT_RAW) {
		char *start;
		int offset, tx_len;

		offset = 0;
		start = ((plcMsgRaw *)resp)->data;
		tx_len = ((plcMsgRaw *)resp)->size;

		py_plan = malloc(sizeof(Ply_plan));
		py_plan->pplan = (void *) (*((long long *) (start + offset))); offset += sizeof(int64);
		py_plan->nargs = *((int *) (start + offset)); offset += sizeof(int32);
		if (py_plan->nargs != nargs) {
			raise_execution_error("plpy.prepare: bad argument number: %d "
				"(returned) vs %d (expected).", py_plan->nargs, nargs);
			return NULL;
		}

		if (nargs > 0) {
			if (offset + (signed int) sizeof(plcDatatype) * nargs != tx_len) {
				raise_execution_error("Client format error for spi prepare. "
					"calculated length (%d) vs transferred length (%d)",
					offset + sizeof(plcDatatype) * nargs, tx_len);
				return NULL;
			}

			py_plan->argtypes = malloc(sizeof(plcDatatype) * nargs);
			if (py_plan->argtypes == NULL) {
				raise_execution_error("Could not allocate %d bytes for argtypes"
					" in py_plan", sizeof(plcDatatype) * nargs);
				return NULL;
			}
			memcpy(py_plan->argtypes, start + offset, sizeof(plcDatatype) * nargs);
		}
		/* FIXME: error handling and free */
	} else {
		raise_execution_error("Client expects message type %c", resp->msgtype);
		return NULL;
	}

    free_rawmsg((plcMsgRaw *) resp);
	pyresult = PyLong_FromLongLong((long long) py_plan);

    return pyresult;
}
