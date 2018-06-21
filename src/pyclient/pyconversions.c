/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016, Pivotal.
 *
 *------------------------------------------------------------------------------
 */

#include "pyconversions.h"
#include "pycall.h"
#include "pyerror.h"
#include "common/messages/messages.h"
#include "common/comm_utils.h"

#include <Python.h>

static PyObject *PLyUnicode_Bytes(PyObject *unicode);

static PyObject *plc_pyobject_from_int1(char *input, plcPyType *type);

static PyObject *plc_pyobject_from_int2(char *input, plcPyType *type);

static PyObject *plc_pyobject_from_int4(char *input, plcPyType *type);

static PyObject *plc_pyobject_from_int8(char *input, plcPyType *type);

static PyObject *plc_pyobject_from_float4(char *input, plcPyType *type);

static PyObject *plc_pyobject_from_float8(char *input, plcPyType *type);

static PyObject *plc_pyobject_from_text(char *input, plcPyType *type);

static PyObject *plc_pyobject_from_text_ptr(char *input, plcPyType *type);

static PyObject *plc_pyobject_from_array_dim(plcArray *arr, plcPyType *type,
                                             int *idx, int *ipos, char **pos, int vallen, int dim);

static PyObject *plc_pyobject_from_array(char *input, plcPyType *type);

static PyObject *plc_pyobject_from_udt(char *input, plcPyType *type);

static PyObject *plc_pyobject_from_udt_ptr(char *input, plcPyType *type);

static PyObject *plc_pyobject_from_bytea(char *input, plcPyType *type);

static PyObject *plc_pyobject_from_bytea_ptr(char *input, plcPyType *type);

static int plc_pyobject_as_int1(PyObject *input, char **output, plcPyType *type);

static int plc_pyobject_as_int2(PyObject *input, char **output, plcPyType *type);

static int plc_pyobject_as_int4(PyObject *input, char **output, plcPyType *type);

static int plc_pyobject_as_int8(PyObject *input, char **output, plcPyType *type);

static int plc_pyobject_as_float4(PyObject *input, char **output, plcPyType *type);

static int plc_pyobject_as_float8(PyObject *input, char **output, plcPyType *type);

static int plc_pyobject_as_text(PyObject *input, char **output, plcPyType *type);

static int plc_pyobject_as_array(PyObject *input, char **output, plcPyType *type);

static int plc_pyobject_as_udt(PyObject *input, char **output, plcPyType *type);

static int plc_pyobject_as_bytea(PyObject *input, char **output, plcPyType *type);

static void plc_pyobject_iter_free(plcIterator *iter);

static rawdata *plc_pyobject_as_array_next(plcIterator *iter);

static plcPyInputFunc Ply_get_input_function(plcDatatype dt, bool isArrayElement);

plcPyOutputFunc Ply_get_output_function(plcDatatype dt);

static void plc_parse_type(plcPyType *pytype, plcType *type, char *argName, bool isArrayElement);

static PyObject *plc_pyobject_from_int1(char *input, plcPyType *type UNUSED) {
	return PyInt_FromLong((long) *input);
}

static PyObject *plc_pyobject_from_int2(char *input, plcPyType *type UNUSED) {
	return PyInt_FromLong((long) *((short *) input));
}

static PyObject *plc_pyobject_from_int4(char *input, plcPyType *type UNUSED) {
	return PyInt_FromLong((long) *((int *) input));
}

static PyObject *plc_pyobject_from_int8(char *input, plcPyType *type UNUSED) {
	return PyLong_FromLongLong((long long) *((long long *) input));
}

static PyObject *plc_pyobject_from_float4(char *input, plcPyType *type UNUSED) {
	return PyFloat_FromDouble((double) *((float *) input));
}

static PyObject *plc_pyobject_from_float8(char *input, plcPyType *type UNUSED) {
	return PyFloat_FromDouble(*((double *) input));
}

static PyObject *plc_pyobject_from_text(char *input, plcPyType *type UNUSED) {
	return PyString_FromString(input);
}

static PyObject *plc_pyobject_from_text_ptr(char *input, plcPyType *type UNUSED) {
	return PyString_FromString(*((char **) input));
}

static PyObject *plc_pyobject_from_array_dim(plcArray *arr,
                                             plcPyType *type,
                                             int *idx,
                                             int *ipos,
                                             char **pos,
                                             int vallen,
                                             int dim) {
	PyObject *res = NULL;
	if (dim == arr->meta->ndims) {
		if (arr->nulls[*ipos] != 0) {
			res = Py_None;
			Py_INCREF(Py_None);
		} else {
			res = type->conv.inputfunc(*pos, type);
		}
		*ipos += 1;
		*pos = *pos + vallen;
	} else {
		res = PyList_New(arr->meta->dims[dim]);
		for (idx[dim] = 0; idx[dim] < arr->meta->dims[dim]; idx[dim]++) {
			PyObject *obj;
			obj = plc_pyobject_from_array_dim(arr, type, idx, ipos, pos, vallen, dim + 1);
			if (obj == NULL) {
				Py_DECREF(res);
				return NULL;
			}
			PyList_SetItem(res, idx[dim], obj);
		}
	}
	return res;
}

static PyObject *plc_pyobject_from_array(char *input, plcPyType *type) {
	plcArray *arr = (plcArray *) input;
	PyObject *res = NULL;

	if (arr->meta->ndims == 0) {
		res = PyList_New(0);
	} else {
		int *idx;
		int ipos;
		char *pos;
		int vallen = 0;

		idx = malloc(sizeof(int) * arr->meta->ndims);
		memset(idx, 0, sizeof(int) * arr->meta->ndims);
		ipos = 0;
		pos = arr->data;
		vallen = plc_get_type_length(arr->meta->type);
		res = plc_pyobject_from_array_dim(arr, &type->subTypes[0], idx, &ipos, &pos, vallen, 0);
		free(idx);
	}

	return res;
}

static PyObject *plc_pyobject_from_udt(char *input, plcPyType *type) {
	plcUDT *udt;
	int i;
	PyObject *res = NULL;
	PyObject *obj = NULL;

	udt = (plcUDT *) input;
	res = PyDict_New();

	for (i = 0; i < type->nSubTypes; i++) {
		if (type->subTypes[i].typeName != NULL) {
			if (udt->data[i].isnull) {
				PyDict_SetItemString(res, type->subTypes[i].typeName, Py_None);
			} else {
				obj = type->subTypes[i].conv.inputfunc(udt->data[i].value,
				                                       &type->subTypes[i]);
				PyDict_SetItemString(res, type->subTypes[i].typeName, obj);
				Py_XDECREF(obj);
			}
		} else {
			raise_execution_error("Field %d of the input UDT is unnamed and cannot be converted to Python dict key", i);
			res = NULL;
			break;
		}
	}

	return res;
}

static PyObject *plc_pyobject_from_udt_ptr(char *input, plcPyType *type) {
	return plc_pyobject_from_udt(*((char **) input), type);
}

static PyObject *plc_pyobject_from_bytea(char *input, plcPyType *type UNUSED) {
	return PyBytes_FromStringAndSize(input + 4, *((int *) input));
}

static PyObject *plc_pyobject_from_bytea_ptr(char *input, plcPyType *type) {
	return plc_pyobject_from_bytea(*((char **) input), type);
}

static int plc_pyobject_as_int1(PyObject *input, char **output, plcPyType *type UNUSED) {
	int res = 0;
	char *out = (char *) malloc(1);
	*output = out;
	if (PyInt_Check(input))
		*out = (char) PyInt_AsLong(input);
	else if (PyLong_Check(input))
		*out = (char) PyLong_AsLongLong(input);
	else if (PyFloat_Check(input))
		*out = (char) PyFloat_AsDouble(input);
	else {
		raise_execution_error("Exception occurred transforming result object to int1");
		res = -1;
	}
	return res;
}

static int plc_pyobject_as_int2(PyObject *input, char **output, plcPyType *type UNUSED) {
	int res = 0;
	char *out = (char *) malloc(2);
	*output = out;
	if (PyInt_Check(input))
		*((short *) out) = (short) PyInt_AsLong(input);
	else if (PyLong_Check(input))
		*((short *) out) = (short) PyLong_AsLongLong(input);
	else if (PyFloat_Check(input))
		*((short *) out) = (short) PyFloat_AsDouble(input);
	else {
		raise_execution_error("Exception occurred transforming result object to int2");
		res = -1;
	}
	return res;
}

static int plc_pyobject_as_int4(PyObject *input, char **output, plcPyType *type UNUSED) {
	int res = 0;
	char *out = (char *) malloc(4);
	*output = out;
	if (PyInt_Check(input))
		*((int *) out) = (int) PyInt_AsLong(input);
	else if (PyLong_Check(input))
		*((int *) out) = (int) PyLong_AsLongLong(input);
	else if (PyFloat_Check(input))
		*((int *) out) = (int) PyFloat_AsDouble(input);
	else {
		raise_execution_error("Exception occurred transforming result object to int4");
		res = -1;
	}
	return res;
}

static int plc_pyobject_as_int8(PyObject *input, char **output, plcPyType *type UNUSED) {
	int res = 0;
	char *out = (char *) malloc(8);
	*output = out;
	if (PyLong_Check(input))
		*((long long *) out) = (long long) PyLong_AsLongLong(input);
	else if (PyInt_Check(input))
		*((long long *) out) = (long long) PyInt_AsLong(input);
	else if (PyFloat_Check(input))
		*((long long *) out) = (long long) PyFloat_AsDouble(input);
	else {
		raise_execution_error("Exception occurred transforming result object to int8");
		res = -1;
	}
	return res;
}

static int plc_pyobject_as_float4(PyObject *input, char **output, plcPyType *type UNUSED) {
	int res = 0;
	char *out = (char *) malloc(4);
	*output = out;
	if (PyFloat_Check(input))
		*((float *) out) = (float) PyFloat_AsDouble(input);
	else if (PyLong_Check(input))
		*((float *) out) = (float) PyLong_AsLongLong(input);
	else if (PyInt_Check(input))
		*((float *) out) = (float) PyInt_AsLong(input);
	else {
		raise_execution_error("Exception occurred transforming result object to float4");
		res = -1;
	}
	return res;
}

static int plc_pyobject_as_float8(PyObject *input, char **output, plcPyType *type UNUSED) {
	int res = 0;
	char *out = (char *) malloc(8);
	*output = out;
	if (PyFloat_Check(input))
		*((double *) out) = (double) PyFloat_AsDouble(input);
	else if (PyLong_Check(input))
		*((double *) out) = (double) PyLong_AsLongLong(input);
	else if (PyInt_Check(input))
		*((double *) out) = (double) PyInt_AsLong(input);
	else {
		raise_execution_error("Exception occurred transforming result object to float8");
		res = -1;
	}
	return res;
}

static int plc_pyobject_as_text(PyObject *input, char **output, plcPyType *type UNUSED) {

	PyObject *plrv_bo;
	int res = 0;

	if (PyUnicode_Check(input))
		plrv_bo = PLyUnicode_Bytes(input);
	else if (PyFloat_Check(input)) {
		/* use repr() for floats, str() is lossy */
#if PY_MAJOR_VERSION >= 3
		PyObject *s = PyObject_Repr(input);

		plrv_bo = PLyUnicode_Bytes(s);
		Py_XDECREF(s);
#else
		plrv_bo = PyObject_Repr(input);
#endif
	} else {
#if PY_MAJOR_VERSION >= 3
		PyObject *s = PyObject_Str(plrv);

		plrv_bo = PLyUnicode_Bytes(s);
		Py_XDECREF(s);
#else
		plrv_bo = PyObject_Str(input);
#endif
	}
	if (!plrv_bo) {
		*output = NULL;
				raise_execution_error("Exception occurred transforming result object to text");
				res = -1;
	} else {
		*output = pstrdup(PyBytes_AsString(plrv_bo));
		Py_XDECREF(plrv_bo);
	}

	return res;
}

static void plc_pyobject_iter_free(plcIterator *iter) {
	plcArrayMeta *meta;
	plcPyArrMeta *pymeta;
	meta = (plcArrayMeta *) iter->meta;
	pymeta = (plcPyArrMeta *) iter->payload;
	pfree(meta->dims);
	pfree(pymeta->dims);
	pfree(iter->meta);
	pfree(iter->payload);
	pfree(iter->position);
	return;
}

static rawdata *plc_pyobject_as_array_next(plcIterator *iter) {
	plcPyArrMeta *meta;
	plcPyArrPointer *ptrs;
	rawdata *res;
	PyObject *obj;
	int ptr;

	meta = (plcPyArrMeta *) iter->payload;
	ptrs = (plcPyArrPointer *) iter->position;
	res = (rawdata *) pmalloc(sizeof(rawdata));

	ptr = meta->ndims - 1;
	obj = PySequence_GetItem(ptrs[ptr].obj, ptrs[ptr].pos);
	if (obj == NULL || obj == Py_None) {
		res->isnull = 1;
		res->value = NULL;
	} else {
		res->isnull = 0;
		meta->outputfunc(obj, &res->value, meta->type);
	}
	Py_XDECREF(obj);

	while (ptr >= 0) {
		ptrs[ptr].pos += 1;
		/* If we finished up iterating over this dimension */
		if (ptrs[ptr].pos == meta->dims[ptr]) {
			Py_DECREF(ptrs[ptr].obj);
			ptrs[ptr].obj = NULL;
			ptrs[ptr].pos = 0;
			ptr -= 1;
		}
			/* If we found the "next" dimension to iterate over */
		else if (ptrs[ptr].pos < meta->dims[ptr]) {
			ptr += 1;
			while (ptr < meta->ndims) {
				ptrs[ptr].obj = PySequence_GetItem(ptrs[ptr - 1].obj, ptrs[ptr - 1].pos);
				ptrs[ptr].pos = 0;
				ptr += 1;
			}
			break;
		}
	}

	return res;
}

static int plc_pyobject_as_array(PyObject *input, char **output, plcPyType *type) {
	plcPyArrMeta *meta;
	plcArrayMeta *arrmeta;
	PyObject *obj;
	plcIterator *iter;
	size_t dims[PLC_MAX_ARRAY_DIMS];
	PyObject *stack[PLC_MAX_ARRAY_DIMS];
	int ndims = 0;
	int res = 0;
	int i = 0;
	plcPyArrPointer *ptrs;

	/* We allow only lists to be returned as arrays */
	if (PySequence_Check(input) && !PyString_Check(input)) {
		obj = input;
		Py_INCREF(obj);
		/* We want to iterate through all iterable objects except by strings */
		while (obj != NULL && PySequence_Check(obj) && !PyString_Check(obj)) {
			int len = PySequence_Length(obj);
			if (len < 0) {
				*output = NULL;
				return -1;
			}
			dims[ndims] = len;
			stack[ndims] = obj;
			ndims += 1;
			if (dims[ndims - 1] > 0)
				obj = PySequence_GetItem(obj, 0);
			else
				break;
		}
		Py_XDECREF(obj);

		/* Allocate the iterator */
		iter = (plcIterator *) pmalloc(sizeof(plcIterator));

		/* Initialize metas */
		arrmeta = (plcArrayMeta *) pmalloc(sizeof(plcArrayMeta));
		arrmeta->ndims = ndims;
		arrmeta->dims = (int *) pmalloc(ndims * sizeof(int));
		arrmeta->size = (ndims == 0) ? 0 : 1;
		arrmeta->type = type->subTypes[0].type;

		meta = (plcPyArrMeta *) pmalloc(sizeof(plcPyArrMeta));
		meta->ndims = ndims;
		meta->dims = (size_t *) pmalloc(ndims * sizeof(size_t));
		meta->outputfunc = Ply_get_output_function(type->subTypes[0].type);
		meta->type = &type->subTypes[0];

		for (i = 0; i < ndims; i++) {
			meta->dims[i] = dims[i];
			arrmeta->dims[i] = (int) dims[i];
			arrmeta->size *= (int) dims[i];
		}

		iter->meta = arrmeta;
		iter->payload = (char *) meta;

		/* Initializing initial position */
		ptrs = (plcPyArrPointer *) pmalloc(ndims * sizeof(plcPyArrPointer));
		for (i = 0; i < ndims; i++) {
			ptrs[i].pos = 0;
			ptrs[i].obj = stack[i];
		}
		iter->position = (char *) ptrs;

		/* Initializing "data" */
		iter->data = (char *) input;

		/* Initializing "next" and "cleanup" functions */
		iter->next = plc_pyobject_as_array_next;
		iter->cleanup = plc_pyobject_iter_free;

		*output = (char *) iter;
	} else {
		raise_execution_error("Cannot convert non-sequence object to array");
		*output = NULL;
		res = -1;
	}

	return res;
}

static int plc_pyobject_as_udt(PyObject *input, char **output, plcPyType *type) {
	int res = 0;

	*output = NULL;
	if (!PyDict_Check(input)) {
		raise_execution_error("Only 'dict' object can be converted to UDT \"%s\"", type->typeName);
		res = -1;
	} else {
		int i = 0;
		plcUDT *udt;

		udt = pmalloc(sizeof(plcUDT));
		udt->data = pmalloc(type->nSubTypes * sizeof(rawdata));
		for (i = 0; i < type->nSubTypes && res == 0; i++) {
			PyObject *value = NULL;
			value = PyDict_GetItemString(input, type->subTypes[i].typeName);
			if (value == NULL) {
				udt->data[i].isnull = true;
				udt->data[i].value = NULL;
				raise_execution_error("Cannot find key '%s' in result dictionary for converting "
					                      "it into UDT", type->subTypes[i].typeName);
				res = -1;
			} else if (value == Py_None) {
				udt->data[i].isnull = true;
				udt->data[i].value = NULL;
			} else {
				udt->data[i].isnull = false;
				res = type->subTypes[i].conv.outputfunc(value, &udt->data[i].value, &type->subTypes[i]);
			}
		}

		*output = (char *) udt;
	}

	return res;
}

static int plc_pyobject_as_bytea(PyObject *input, char **output, plcPyType *type UNUSED) {
	PyObject *volatile plrv_so = NULL;
	int len;
	char *res;

	if (input == Py_None) {
		raise_execution_error("None object cannot be transformed to bytea");
		return -1;
	}

#if PY_MAJOR_VERSION >= 3
	if (PyUnicode_Check(input)) {
		Py_ssize_t sz = 0;
		char *str;

		str = PyUnicode_AsUTF8AndSize(input, &sz);
		if (str == NULL) {
			raise_execution_error("Failed to get byte representation of unicode string");
			return -1;
		}

		res = pmalloc(sz + 4);
		*((int*)res) = (int)sz;
		memcpy(res + 4, str, sz);
		*output = res;
		return 0;
	}
#endif

	plrv_so = PyObject_Bytes(input);
	if (!plrv_so) {
		raise_execution_error("Could not create bytes representation of Python object");
		return -1;
	}

	len = PyBytes_Size(plrv_so);
	res = pmalloc(len + 4);
	*((int *) res) = len;
	memcpy(res + 4, PyBytes_AsString(plrv_so), len);
	Py_DECREF(plrv_so);
	*output = res;

	return 0;
}

static plcPyInputFunc Ply_get_input_function(plcDatatype dt, bool isArrayElement) {
	plcPyInputFunc res = NULL;
	switch (dt) {
		case PLC_DATA_INT1:
			res = plc_pyobject_from_int1;
			break;
		case PLC_DATA_INT2:
			res = plc_pyobject_from_int2;
			break;
		case PLC_DATA_INT4:
			res = plc_pyobject_from_int4;
			break;
		case PLC_DATA_INT8:
			res = plc_pyobject_from_int8;
			break;
		case PLC_DATA_FLOAT4:
			res = plc_pyobject_from_float4;
			break;
		case PLC_DATA_FLOAT8:
			res = plc_pyobject_from_float8;
			break;
		case PLC_DATA_TEXT:
			if (isArrayElement) {
				res = plc_pyobject_from_text_ptr;
			} else {
				res = plc_pyobject_from_text;
			}
			break;
		case PLC_DATA_BYTEA:
			if (isArrayElement) {
				res = plc_pyobject_from_bytea_ptr;
			} else {
				res = plc_pyobject_from_bytea;
			}
			break;
		case PLC_DATA_ARRAY:
			res = plc_pyobject_from_array;
			break;
		case PLC_DATA_UDT:
			if (isArrayElement) {
				res = plc_pyobject_from_udt_ptr;
			} else {
				res = plc_pyobject_from_udt;
			}
			break;
		default:
			raise_execution_error("Type %s [%d] cannot be passed Ply_get_input_function function",
			                      plc_get_type_name(dt), (int) dt);
			break;
	}
	return res;
}

plcPyOutputFunc Ply_get_output_function(plcDatatype dt) {
	plcPyOutputFunc res = NULL;
	switch (dt) {
		case PLC_DATA_INT1:
			res = plc_pyobject_as_int1;
			break;
		case PLC_DATA_INT2:
			res = plc_pyobject_as_int2;
			break;
		case PLC_DATA_INT4:
			res = plc_pyobject_as_int4;
			break;
		case PLC_DATA_INT8:
			res = plc_pyobject_as_int8;
			break;
		case PLC_DATA_FLOAT4:
			res = plc_pyobject_as_float4;
			break;
		case PLC_DATA_FLOAT8:
			res = plc_pyobject_as_float8;
			break;
		case PLC_DATA_TEXT:
			res = plc_pyobject_as_text;
			break;
		case PLC_DATA_BYTEA:
			res = plc_pyobject_as_bytea;
			break;
		case PLC_DATA_ARRAY:
			res = plc_pyobject_as_array;
			break;
		case PLC_DATA_UDT:
			res = plc_pyobject_as_udt;
			break;
		default:
			raise_execution_error("Type %s [%d] cannot be passed Ply_get_output_function function",
			                      plc_get_type_name(dt), (int) dt);
			break;
	}
	return res;
}

static void plc_parse_type(plcPyType *pytype, plcType *type, char *argName, bool isArrayElement) {
	int i = 0;

	pytype->typeName = (type->typeName == NULL) ? NULL : strdup(type->typeName);
	pytype->argName = (argName == NULL) ? NULL : strdup(argName);
	pytype->type = type->type;
	pytype->nSubTypes = type->nSubTypes;
	pytype->conv.inputfunc = Ply_get_input_function(pytype->type, isArrayElement);
	pytype->conv.outputfunc = Ply_get_output_function(pytype->type);
	if (pytype->nSubTypes > 0) {
		bool isArray = (type->type == PLC_DATA_ARRAY) ? true : false;
		pytype->subTypes = (plcPyType *) malloc(pytype->nSubTypes * sizeof(plcPyType));
		for (i = 0; i < type->nSubTypes; i++) {
			plc_parse_type(&pytype->subTypes[i], &type->subTypes[i], NULL, isArray);
		}
	} else {
		pytype->subTypes = NULL;
	}
}

plcPyFunction *plc_py_init_function(plcMsgCallreq *call) {
	plcPyFunction *res;
	int i;

	res = (plcPyFunction *) malloc(sizeof(plcPyFunction));
	res->call = call;
	res->proc.src = strdup(call->proc.src);
	res->proc.name = strdup(call->proc.name);
	res->nargs = call->nargs;
	res->retset = call->retset;
	res->args = (plcPyType *) malloc(res->nargs * sizeof(plcPyType));
	res->objectid = call->objectid;
	res->pySD = PyDict_New();

	for (i = 0; i < res->nargs; i++) {
		plc_parse_type(&res->args[i], &call->args[i].type, call->args[i].name, false);
	}

	plc_parse_type(&res->res, &call->retType, "result", false);

	return res;
}

plcPyResult *plc_init_result_conversions(plcMsgResult *res) {
	plcPyResult *pyres = NULL;
	uint32 i;

	pyres = (plcPyResult *) malloc(sizeof(plcPyResult));
	pyres->res = res;
	pyres->args = (plcPyType *) malloc(res->cols * sizeof(plcPyType));

	for (i = 0; i < res->cols; i++) {
		plc_parse_type(&pyres->args[i], &res->types[i], NULL, false);
	}

	return pyres;
}

static void plc_py_free_type(plcPyType *type) {
	int i = 0;
	if (type->typeName != NULL) {
		free(type->typeName);
	}
	if (type->argName != NULL) {
		free(type->argName);
	}
	for (i = 0; i < type->nSubTypes; i++)
		plc_py_free_type(&type->subTypes[i]);
	if (type->nSubTypes > 0)
		free(type->subTypes);
	return;
}

void plc_py_free_function(plcPyFunction *func) {
	int i = 0;
	for (i = 0; i < func->nargs; i++)
		plc_py_free_type(&func->args[i]);
	plc_py_free_type(&func->res);
	Py_DECREF(func->pySD);
	free(func->args);
	free(func->proc.src);
	free(func->proc.name);
	free(func);
}

void plc_free_result_conversions(plcPyResult *res) {
	uint32 i;

	for (i = 0; i < res->res->cols; i++) {
		plc_py_free_type(&res->args[i]);
	}
	free(res->args);
	free(res);
}

void plc_py_copy_type(plcType *type, plcPyType *pytype) {
	type->type = pytype->type;
	type->nSubTypes = pytype->nSubTypes;
	type->typeName = (pytype->typeName == NULL) ? NULL : strdup(pytype->typeName);
	if (type->nSubTypes > 0) {
		int i = 0;
		type->subTypes = (plcType *) pmalloc(type->nSubTypes * sizeof(plcType));
		for (i = 0; i < type->nSubTypes; i++)
			plc_py_copy_type(&type->subTypes[i], &pytype->subTypes[i]);
	} else {
		type->subTypes = NULL;
	}
}



/*
 * Convert a Python unicode object to a Python string/bytes object in
 * PostgreSQL server encoding.	Reference ownership is passed to the
 * caller.
 */
static PyObject *
PLyUnicode_Bytes(PyObject *unicode)
{
	PyObject   *rv;
	rv = PyUnicode_AsEncodedString(unicode, serverenc, "strict");
	if (rv == NULL)
		plc_elog(ERROR, "could not convert Python Unicode object to PostgreSQL server encoding");
	return rv;
}
