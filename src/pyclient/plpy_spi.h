/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016, Pivotal.
 *
 *------------------------------------------------------------------------------
 */
#ifndef PLC_PYSPI_H
#define PLC_PYSPI_H

#include <Python.h>

PyObject *PLy_spi_execute(PyObject *self, PyObject *pyquery);

PyObject *PLy_subtransaction(PyObject *, PyObject *);

#endif /* PLC_PYSPI_H */
