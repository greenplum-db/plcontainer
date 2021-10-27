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

extern PyTypeObject PLy_PlanType;
extern PyTypeObject PLy_SubtransactionType;
extern PyTypeObject PLy_ResultType;

PyObject *PLy_spi_execute(PyObject *self, PyObject *pyquery);

PyObject *PLy_spi_prepare(PyObject *self, PyObject *args);

PyObject *PLy_subtransaction(PyObject *, PyObject *);

#endif /* PLC_PYSPI_H */
