/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#include "pycall.h"
#include "pylogging.h"
#include "common/messages/messages.h"
#include "common/comm_channel.h"
#include "common/comm_utils.h"

#include <Python.h>

static PyObject *PLy_output(volatile int, PyObject *, PyObject *);

PyObject *PLy_debug(PyObject *self, PyObject *args) {
	return PLy_output(DEBUG2, self, args);
}

PyObject *PLy_log(PyObject *self, PyObject *args) {
	return PLy_output(LOG, self, args);
}

PyObject *PLy_info(PyObject *self, PyObject *args) {
	return PLy_output(INFO, self, args);
}

PyObject *PLy_notice(PyObject *self, PyObject *args) {
	return PLy_output(NOTICE, self, args);
}

PyObject *PLy_warning(PyObject *self, PyObject *args) {
	return PLy_output(WARNING, self, args);
}

PyObject *PLy_error(PyObject *self, PyObject *args) {
	return PLy_output(ERROR, self, args);
}

PyObject *PLy_fatal(PyObject *self, PyObject *args) {
	return PLy_output(FATAL, self, args);
}

static PyObject *PLy_output(volatile int level, PyObject *self UNUSED, PyObject *args) {
	PyObject *volatile so;
	char *volatile sv;
	int freesv = 0;
	plcConn *conn = plcconn_global;
	plcMsgLog *msg;

	if (plc_is_execution_terminated == 0) {
		if (PyTuple_Size(args) == 1) {
			/*
			 * Treat single argument specially to avoid undesirable ('tuple',)
			 * decoration.
			 */
			PyObject *o;
			PyArg_UnpackTuple(args, "plpy.elog", 1, 1, &o);
			so = PyObject_Str(o);
		} else {
			so = PyObject_Str(args);
		}

		if (so == NULL || ((sv = PyString_AsString(so)) == NULL)) {
			level = ERROR;
			sv = strdup("could not parse error message in plpy.elog");
			freesv = 1;
		}

		if (level >= ERROR)
			plc_is_execution_terminated = 1;

		msg = pmalloc(sizeof(plcMsgLog));
		msg->msgtype = MT_LOG;
		msg->level = level;
		msg->message = sv;

		plcontainer_channel_send(conn, (plcMessage *) msg);

		/*
		 * Note: If sv came from PyString_AsString(), it points into storage
		 * owned by so.  So free so after using sv.
		 */
		Py_XDECREF(so);
		free(msg);
		if (freesv == 1)
			free(sv);
	}

	/*
	 * return a legal object so the interpreter will continue on its merry way
	 */
	Py_INCREF(Py_None);
	return Py_None;
}
