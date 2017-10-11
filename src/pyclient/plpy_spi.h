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

typedef struct PlyPlan {
	plcDatatype *arg_datatype;
	void        *plan;
	int          nargs;
} plyPlan;

PyObject *PLy_spi_execute(PyObject *self, PyObject *pyquery);
PyObject *PLy_spi_prepare(PyObject *self, PyObject *args);

#endif /* PLC_PYSPI_H */
