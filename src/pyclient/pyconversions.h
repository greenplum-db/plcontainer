/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016, Pivotal.
 *
 *------------------------------------------------------------------------------
 */

#ifndef PLC_PYCONVERSIONS_H
#define PLC_PYCONVERSIONS_H

#include <Python.h>
#include "common/messages/messages.h"

#define PLC_MAX_ARRAY_DIMS 10

typedef struct plcPyType plcPyType;

typedef PyObject *(*plcPyInputFunc)(char *, plcPyType *);

typedef int (*plcPyOutputFunc)(PyObject *, char **, plcPyType *);

/* Working with arrays in Python */

typedef struct plcPyArrPointer {
	size_t pos;
	PyObject *obj;
} plcPyArrPointer;

typedef struct plcPyArrMeta {
	int ndims;
	size_t *dims;
	plcPyType *type;
	plcPyOutputFunc outputfunc;
} plcPyArrMeta;

/* Working with types in Python */

typedef struct plcPyTypeConv {
	plcPyInputFunc inputfunc;
	plcPyOutputFunc outputfunc;
} plcPyTypeConv;

struct plcPyType {
	plcDatatype type;
	char *argName;
	char *typeName;
	int nSubTypes;
	plcPyType *subTypes;
	plcPyTypeConv conv;
};

typedef struct plcPyResult {
	plcMsgResult *res;
	plcPyType *args;
} plcPyResult;

typedef struct plcPyFunction {
	plcProcSrc proc;
	plcMsgCallreq *call;
	PyObject *pyProc;
	int nargs;
	plcPyType *args;
	plcPyType res;
	int retset;
	unsigned int objectid;
	PyObject *pyfunc;
	PyObject *pySD;
} plcPyFunction;

void plc_py_copy_type(plcType *type, plcPyType *pytype);

plcPyFunction *plc_py_init_function(plcMsgCallreq *call);

plcPyResult *plc_init_result_conversions(plcMsgResult *res);

void plc_py_free_function(plcPyFunction *func);

void plc_free_result_conversions(plcPyResult *res);

plcPyOutputFunc Ply_get_output_function(plcDatatype dt);

#endif /* PLC_PYCONVERSIONS_H */
