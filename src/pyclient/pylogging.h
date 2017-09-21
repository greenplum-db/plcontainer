/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016, Pivotal.
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#ifndef PLC_PYLOGGING_H
#define PLC_PYLOGGING_H

#include <Python.h>

PyObject *PLy_debug(PyObject *self, PyObject *args);
PyObject *PLy_log(PyObject *self, PyObject *args);
PyObject *PLy_info(PyObject *self, PyObject *args);
PyObject *PLy_notice(PyObject *self, PyObject *args);
PyObject *PLy_warning(PyObject *self, PyObject *args);
PyObject *PLy_error(PyObject *self, PyObject *args);
PyObject *PLy_fatal(PyObject *self, PyObject *args);

#endif /* PLC_PYLOGGING_H */
