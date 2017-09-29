/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#include <stdlib.h>
#include <string.h>

#include "common/comm_channel.h"
#include "common/comm_utils.h"
#include "common/comm_connectivity.h"
#include "pycall.h"
#include "pyerror.h"
#include "pyconversions.h"
#include "pylogging.h"
#include "plpy_spi.h"
#include "pycache.h"

#include <Python.h>

plcConn* plcconn_global = NULL;

static char *create_python_func(plcMsgCallreq *req);
static PyObject *arguments_to_pytuple(plcPyFunction *pyfunc);
static int process_call_results(plcConn *conn, PyObject *retval, plcPyFunction *pyfunc);
static int fill_rawdata(rawdata *res, PyObject *retval, plcPyFunction *pyfunc);

static PyObject *PyMainModule = NULL;
static PyMethodDef moddef[] = {
    /*
     * logging methods
     */
    {"debug",   PLy_debug,   METH_VARARGS, NULL},
    {"log",     PLy_log,     METH_VARARGS, NULL},
    {"info",    PLy_info,    METH_VARARGS, NULL},
    {"notice",  PLy_notice,  METH_VARARGS, NULL},
    {"warning", PLy_warning, METH_VARARGS, NULL},
    {"error",   PLy_error,   METH_VARARGS, NULL},
    {"fatal",   PLy_fatal,   METH_VARARGS, NULL},

	/*
	 * create a stored plan
	 */
	{"prepare", PLy_spi_prepare, METH_VARARGS, NULL},

	/*
	 * execute a plan or query
	 */
	{"execute", PLy_spi_execute, METH_VARARGS, NULL},

    /*
     * create the subtransaction context manager
     */
    {"subtransaction", PLy_subtransaction, METH_NOARGS, NULL},

    {NULL, NULL, 0, NULL}
};

#if PY_MAJOR_VERSION >= 3
    static PyModuleDef plc_plpy_module = {
        PyModuleDef_HEAD_INIT,     /* m_base */
        "plpy",                    /* m_name */
        NULL,                      /* m_doc */
        -1,                        /* m_size */
        moddef,                    /* m_methods */
        NULL,                      /* m_reload */
        NULL,                      /* m_traverse */
        NULL,                      /* m_clear */
        NULL                       /* m_free */
    };
#endif

int python_init() {
    PyObject *plpymod = NULL;
    PyObject *dict = NULL;
    PyObject *gd = NULL;

    plc_Py_SetProgramName("PythonContainer");
    Py_Initialize();

	/*
	 * initialize plpy module
	 */
	if (PyType_Ready(&PLy_PlanType) < 0)
		lprintf(ERROR, "could not initialize PLy_PlanType");
	if (PyType_Ready(&PLy_SubtransactionType) < 0)
		lprintf(ERROR, "could not initialize PLy_SubtransactionType");

    /* create the plpy module */
    #if PY_MAJOR_VERSION >= 3
        plpymod = PyModule_Create(&plc_plpy_module);
    #else
        plpymod = Py_InitModule("plpy", moddef);
    #endif

    /* Initialize the main module */
    PyMainModule = PyImport_ImportModule("__main__");

    /* Add plpy module to it */
    PyModule_AddObject(PyMainModule, "plpy", plpymod);

    /* Get module dictionary of objects */
    dict = PyModule_GetDict(PyMainModule);
    if (dict == NULL) {
        raise_execution_error("Cannot get '__main__' module contents in Python");
        return -1;
    }

    gd = PyDict_New();
    if (gd == NULL) {
        raise_execution_error("Cannot allocate dictionary object for GD");
        return -1;
    }

    if (PyDict_SetItemString(dict, "GD", gd) < 0) {
        raise_execution_error("Cannot set GD dictionary to main module");
        return -1;
    }
    Py_DECREF(gd);

    return 0;
}

void handle_call(plcMsgCallreq *req, plcConn *conn) {
    PyObject      *retval = NULL;
    PyObject      *dict = NULL;
    PyObject      *args = NULL;
    plcPyFunction *pyfunc = NULL;

    /*
     * Keep our connection for future calls from Python back to us.
     */
    plcconn_global   = conn;
    plc_sending_data = 0;
    plc_is_execution_terminated = 0;

    dict = PyModule_GetDict(PyMainModule); // Returns borrowed reference
    if (dict == NULL) {
        raise_execution_error("Cannot get '__main__' module contents in Python");
        return;
    }

    pyfunc = plc_py_function_cache_get(req->objectid);

    if (pyfunc == NULL || req->hasChanged) {
        char     *func;
        PyObject *val;

        /* Parse request to get funcion structure */
        pyfunc = plc_py_init_function(req);

        /* Modify function code for compiling it into Python object */
        func = create_python_func(req);
        if (func == NULL) {
            return;
        }

        /* The function will be in the dictionary because it was wrapped with "def proc_name:... " */
        val = PyRun_String(func, Py_single_input, dict, dict); // Returns new reference
        if (val == NULL) {
            raise_execution_error("Cannot compile function in Python");
            return;
        }
        Py_DECREF(val);
        free(func);

        /* get the function from the global dictionary, returns borrowed reference */
        val = PyDict_GetItemString(dict, req->proc.name);
        if (!PyCallable_Check(val)) {
            raise_execution_error("Object produced by function is not callable");
            return;
        }

        pyfunc->pyfunc = val;

        plc_py_function_cache_put(pyfunc);
    } else {
        pyfunc->call = req;
    }

    if (PyDict_SetItemString(dict, "SD", pyfunc->pySD) < 0) {
        raise_execution_error("Cannot set SD dictionary to main module");
        return;
    }

    args = arguments_to_pytuple(pyfunc);
    if (args == NULL) {
        raise_execution_error("Cannot convert input arguments to Python tuple");
        return;
    }

    /* call the function */
    plc_is_execution_terminated = 0;
    retval = PyObject_Call(pyfunc->pyfunc, args, NULL); // returns new reference
    if (retval == NULL || PyErr_Occurred()) {
        Py_XDECREF(args);
        raise_execution_error("Exception occurred in Python during function execution");
        return;
    }

    if (plc_is_execution_terminated == 0) {
        process_call_results(conn, retval, pyfunc);
    }

    pyfunc->call = NULL;
    Py_XDECREF(args);
    Py_XDECREF(retval);

    return;
}

static char *create_python_func(plcMsgCallreq *req) {
    int         i, plen;
    const char *sp;
    char *      mrc, *mp;
    size_t      mlen, namelen;
    const char *src, *name;

    name = req->proc.name;
    src  = req->proc.src;

    /*
     * room for function source, the def statement, and the function call.
     *
     * note: we need to allocate strlen(src) * 2 since we replace
     * newlines with newline followed by tab (i.e. "\n\t")
     */
    namelen = strlen(name);
    /* source */
    mlen = sizeof("def ") + namelen + sizeof("():\n\t") + (strlen(src) * 2);
    /* function delimiter*/
    mlen += sizeof("\n\n");
    /* room for n commas and the n+1 argument names */
    mlen += req->nargs;
    mlen += sizeof("args");
    for (i = 0; i < req->nargs; i++) {
        if (req->args[i].name != NULL) {
            mlen += strlen(req->args[i].name);
        }
    }
    mlen += 1; /* null byte */

    mrc  = malloc(mlen);
    plen = snprintf(mrc, mlen, "def %s(args", name);
    if (plen < 0 || ((size_t)plen) > mlen) {
        raise_execution_error("Function name is too long and not fitting the predefined buffer size");
        free(mrc);
        return NULL;
    }

    sp = src;
    mp = mrc + plen;

    for (i = 0; i < req->nargs; i++) {
        if (req->args[i].name != NULL) {
            mp += snprintf(mp, mlen - (mp - mrc), ",%s", req->args[i].name);
        }
    }

    mp += snprintf(mp, mlen - (mp - mrc), "):\n\t");
    /* replace newlines with newline+tab */
    while (*sp != '\0') {
        if (*sp == '\r' && *(sp + 1) == '\n')
            sp++;

        if (*sp == '\n' || *sp == '\r') {
            *mp++ = '\n';
            *mp++ = '\t';
            sp++;
        } else
            *mp++ = *sp++;
    }
    /* finish the function definition with 2 newlines */
    *mp++ = '\n';
    *mp++ = '\n';
    *mp++ = '\0';

    if (mp > mrc + mlen) {
        raise_execution_error("Function body is too long and not fitting the predefined buffer size");
        free(mrc);
        return NULL;
    }

    return mrc;
}

static PyObject *arguments_to_pytuple(plcPyFunction *pyfunc) {
    PyObject *args;
    PyObject *arglist;
    int i;
    int notnull = 0;
    int pos;

    /* Amount of elements that have names and should make it to the input tuple */
    for (i = 0; i < pyfunc->nargs; i++) {
        if (pyfunc->args[i].argName != NULL) {
            notnull += 1;
        }
    }

    /* Creating a tuple that would be input to the function and list of arguments */
    args = PyTuple_New(notnull + 1);
    arglist = PyList_New(pyfunc->nargs);

    /* First element of the argument list is the full list of arguments */
    PyTuple_SetItem(args, 0, arglist); // steals the reference to arglist
    pos = 1;
    for (i = 0; i < pyfunc->nargs; i++) {
        PyObject *arg = NULL;

        /* Get the argument from the callreq structure */
        if (pyfunc->call->args[i].data.isnull) {
            Py_INCREF(Py_None);
            arg = Py_None;
        } else {
            if (pyfunc->args[i].conv.inputfunc == NULL) {
                raise_execution_error("Parameter '%s' (#%d) type %d is not supported",
                                      pyfunc->args[i].argName,
                                      i,
                                      pyfunc->args[i].type);
                return NULL;
            }
            arg = pyfunc->args[i].conv.inputfunc(pyfunc->call->args[i].data.value,
                                                 &pyfunc->args[i]);
        }

        /* Argument cannot be NULL unless some error has happened as Py_None != NULL */
        if (arg == NULL) {
            raise_execution_error("Converting parameter '%s' (#%d) to Python type failed",
                                  pyfunc->args[i].argName, i);
            return NULL;
        }

        /* Only named arguments are passed to the function input tuple */
        if (pyfunc->args[i].argName != NULL) {
            /* As the object reference will be stolen by setitem we need to incref */
            Py_INCREF(arg);
            if (PyTuple_SetItem(args, pos, arg) != 0) { // steals the reference to arg
                raise_execution_error("Appending Python list element %d for argument '%s' has failed",
                                      i, pyfunc->args[i].argName);
                return NULL;
            }
            pos += 1;
        }

        /* All the arguments, including unnamed, are passed to the arguments array */
        if (PyList_SetItem(arglist, i, arg) != 0) { // steals the reference to arg
            raise_execution_error("Appending Python list element %d for argument '%s' has failed",
                                  i, pyfunc->args[i].argName);
            return NULL;
        }
    }
    return args;
}

static int process_call_results(plcConn *conn, PyObject *retval, plcPyFunction *pyfunc) {
    plcMsgResult *res;
    int           retcode = 0;

    /* allocate a result */
    res           = malloc(sizeof(plcMsgResult));
    res->msgtype  = MT_RESULT;
    res->names    = malloc(1 * sizeof(char*));
    res->names[0] = (pyfunc->res.argName == NULL) ? NULL : strdup(pyfunc->res.argName);
    res->types    = malloc(1 * sizeof(plcType));
    res->data     = NULL;
    res->exception_callback = plc_error_callback;
    plc_py_copy_type(&res->types[0], &pyfunc->res);

    /* Now we support only functions returning single column */
    res->cols = 1;

    if (pyfunc->retset) {
        int       i      = 0;
        int       len    = 0;
        PyObject *retobj = retval;
        PyObject *obj    = NULL;

        if (PySequence_Check(retval)) {
            len = PySequence_Length(retval);
        } else {
            PyObject *iter = NULL;

            iter = PyObject_GetIter(retval);
            if (iter == NULL) {
                raise_execution_error("Cannot get iterator out of the returned object");
                free_result(res, true);
                return -1;
            }

            retobj = PyList_New(0);
            obj = PyIter_Next(iter);
            while (obj != NULL && retcode == 0) {
                len += 1;
                retcode = PyList_Append(retobj, obj);
                Py_DECREF(obj);
                obj = PyIter_Next(iter);
            }
            Py_XDECREF(obj);
            Py_DECREF(iter);

            if (retcode < 0) {
                raise_execution_error("Error receiving result data from Python iterator");
                free_result(res, true);
                return -1;
            }
        }

        res->rows = len;
        res->data = malloc(res->rows * sizeof(rawdata*));
        memset(res->data, 0, res->rows * sizeof(rawdata*));
        for(i = 0; i < len && retcode == 0; i++) {
            res->data[i] = malloc(res->cols * sizeof(rawdata));
            obj = PySequence_GetItem(retobj, i);
            retcode = fill_rawdata(&res->data[i][0], obj, pyfunc);
            Py_XDECREF(obj);
        }
    } else {
        res->rows = 1;
        res->data    = malloc(res->rows * sizeof(rawdata*));
        res->data[0] = malloc(res->cols * sizeof(rawdata));
        retcode = fill_rawdata(&res->data[0][0], retval, pyfunc);
    }

    /* If the output operation succeeded we send the result back */
    if (retcode == 0) {
        /* We manually state that we are sending the data to avoid message interleaving */
        plc_sending_data = 1;
        plcontainer_channel_send(conn, (plcMessage*)res);
        plc_sending_data = 0;
    }

    free_result(res, true);

    /* After the message is sent we can safely send exceptions */
    plc_raise_delayed_error();

    return retcode;
}

static int fill_rawdata(rawdata *res, PyObject *retval, plcPyFunction *pyfunc) {
    res->value  = NULL;
    if (retval == Py_None) {
        res->isnull = 1;
    } else {
        int ret = 0;
        res->isnull = 0;
        if (pyfunc->res.conv.outputfunc == NULL) {
            raise_execution_error("Type %d is not yet supported by Python container",
                                  (int)pyfunc->res.type);
            return -1;
        }
        ret = pyfunc->res.conv.outputfunc(retval, &res->value, &pyfunc->res);
        if (ret != 0) {
            raise_execution_error("Exception raised converting function output to type %s [%d]",
                                  plc_get_type_name(pyfunc->res.type), (int)pyfunc->res.type);
            return -1;
        }
    }
    return 0;
}
