/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2018-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#ifndef PLC_PYQUOTE_H
#define PLC_PYQUOTE_H

#include <Python.h>

PyObject * PLy_quote_literal(PyObject *self, PyObject *args);

PyObject * PLy_quote_nullable(PyObject *self, PyObject *args);

PyObject * PLy_quote_ident(PyObject *self, PyObject *args);

#endif /* PLC_PYQUOTE_H */
