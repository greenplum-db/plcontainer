/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */
#include "rconversions.h"
#include "rcall.h"
#include "common/comm_channel.h"

static SEXP plc_r_object_from_int1(char *input, plcRType *type);

static SEXP plc_r_object_from_int2(char *input, plcRType *type);

static SEXP plc_r_object_from_int4(char *input, plcRType *type);

static SEXP plc_r_object_from_int8(char *input, plcRType *type);

static SEXP plc_r_object_from_float4(char *input, plcRType *type);

static SEXP plc_r_object_from_float8(char *input, plcRType *type);

static SEXP plc_r_object_from_text(char *input, plcRType *type);

static SEXP plc_r_object_from_text_ptr(char *input, plcRType *type);

static SEXP plc_r_object_from_array(char *input, plcRType *type);

static SEXP plc_r_object_from_udt(char *input, plcRType *type);

static SEXP plc_r_object_from_udt_ptr(char *input, plcRType *type);

static SEXP plc_r_object_from_bytea(char *input, plcRType *type);

static int plc_r_object_as_int1(SEXP input, char **output, plcRType *type);

static int plc_r_object_as_int2(SEXP input, char **output, plcRType *type);

static int plc_r_object_as_int4(SEXP input, char **output, plcRType *type);

static int plc_r_object_as_int8(SEXP input, char **output, plcRType *type);

static int plc_r_object_as_float4(SEXP input, char **output, plcRType *type);

static int plc_r_object_as_float8(SEXP input, char **output, plcRType *type);

static int plc_r_object_as_text(SEXP input, char **output, plcRType *type);

static int plc_r_object_as_array(SEXP input, char **output, plcRType *type);

static int plc_r_object_as_udt(SEXP input, char **output, plcRType *type);

static int plc_r_object_as_bytea(SEXP input, char **output, plcRType *type);

static void plc_r_object_iter_free(plcIterator *iter);

static rawdata *plc_r_object_as_array_next(plcIterator *iter);

static plcRInputFunc plc_get_input_function(plcDatatype dt, bool isArrayElement);

static void plc_parse_type(plcRType *Rtype, plcType *type, char *argName, bool isArrayElement);

static char *last_R_error_msg = NULL;

/*
 *
 * NOTE all input functions will return a protected
 * value
 *
 */
static SEXP plc_r_object_from_int1(char *input, plcRType *type UNUSED) {
	SEXP arg;
	PROTECT(arg = ScalarLogical((int) *input));
	return arg;
}

static SEXP plc_r_object_from_int2(char *input, plcRType *type UNUSED) {
	SEXP arg;
	PROTECT(arg = ScalarInteger(*((short *) input)));
	return arg;
}

static SEXP plc_r_object_from_int4(char *input, plcRType *type UNUSED) {
	SEXP arg;
	PROTECT(arg = ScalarInteger(*((int *) input)));
	return arg;
}

static SEXP plc_r_object_from_int8(char *input, plcRType *type UNUSED) {
	SEXP arg;
	PROTECT(arg = ScalarReal((double) *((int64 *) input)));
	return arg;
}

static SEXP plc_r_object_from_float4(char *input, plcRType *type UNUSED) {
	SEXP arg;
	PROTECT(arg = ScalarReal((double) *((float *) input)));
	return arg;
}

static SEXP plc_r_object_from_float8(char *input, plcRType *type UNUSED) {
	SEXP arg;
	PROTECT(arg = ScalarReal(*((double *) input)));
	return arg;
}

static SEXP plc_r_object_from_text(char *input, plcRType *type UNUSED) {
	SEXP arg;
	PROTECT(arg = mkString(input));
	return arg;
}

static SEXP plc_r_object_from_text_ptr(char *input, plcRType *type UNUSED) {
	SEXP arg;
	PROTECT(arg = mkString(*((char **) input)));
	return arg;
}

static SEXP plc_r_object_from_array(char *input, plcRType *type) {
	plcArray *arr = (plcArray *) input;
	SEXP res = R_NilValue;

	if (arr->meta->ndims == 0) {
		PROTECT(res = get_r_vector(type->type, 0));
	} else {
		char *pos;
		int vallen = 0;
		int arr_length = 1;
		int i;
		plcRInputFunc infunc;
		plcRType *elmtype;

		/* calculate the length of the array */
		for (i = 0; i < arr->meta->ndims; i++) {
			arr_length *= arr->meta->dims[i];
		}

		/* allocate a vector */
		elmtype = &type->subTypes[0];
		PROTECT(res = get_r_vector(elmtype->type, arr_length));
		vallen = plc_get_type_length(elmtype->type);
		infunc = plc_get_input_function(elmtype->type, true);

		pos = arr->data;
		for (i = 0; i < arr_length; i++) {
			SEXP obj = NULL;

			if (arr->nulls[i] == 0) {
				/*
				 * call the input function for the element in the array
				 */
				obj = infunc(pos, elmtype);
			}
			switch (arr->meta->type) {
				/* 2 and 4 byte integer pgsql datatype => use R INTEGER */
				case PLC_DATA_INT2:
				case PLC_DATA_INT4:
					if (arr->nulls[i] != 0) {
						INTEGER_DATA(res)[i] = NA_INTEGER;
					} else {
						INTEGER_DATA(res)[i] = asInteger(obj);
					}
					break;

					/*
					 * Other numeric types => use R REAL
					 * Note pgsql int8 is mapped to R REAL
					 * because R INTEGER is only 4 byte
					 */
				case PLC_DATA_INT8:
					if (arr->nulls[i] != 0) {
						NUMERIC_DATA(res)[i] = NA_REAL;
					} else {
						NUMERIC_DATA(res)[i] = (float8) asReal(obj);
					}
					break;

				case PLC_DATA_FLOAT4:
					if (arr->nulls[i] != 0) {
						NUMERIC_DATA(res)[i] = NA_REAL;
					} else {
						NUMERIC_DATA(res)[i] = (float4) asReal(obj);
					}
					break;

				case PLC_DATA_FLOAT8:
					if (arr->nulls[i] != 0) {
						NUMERIC_DATA(res)[i] = NA_REAL;
					} else {
						NUMERIC_DATA(res)[i] = (float8) asReal(obj);
					}
					break;
				case PLC_DATA_INT1:
					if (arr->nulls[i] != 0) {
						LOGICAL_DATA(res)[i] = NA_LOGICAL;
					} else {
						LOGICAL_DATA(res)[i] = asLogical(obj);
					}
					break;
				case PLC_DATA_UDT:
					if (arr->nulls[i] != 0) {
						SET_VECTOR_ELT(res, i, R_NilValue);
					} else {
						SET_VECTOR_ELT(res, i, obj);
					}
					break;
				case PLC_DATA_INVALID:
				case PLC_DATA_ARRAY:
				case PLC_DATA_BYTEA:
					raise_execution_error("Arrays cannot handle elements of type %s [%d]",
					                      plc_get_type_name(arr->meta->type),
					                      arr->meta->type);
					break;
				case PLC_DATA_TEXT:
				default:
					/* Everything else is defaulted to string */
					if (arr->nulls[i] != 0) {
						SET_STRING_ELT(res, i, NA_STRING);
					} else {
						obj = STRING_ELT(obj, 0);
						SET_STRING_ELT(res, i, obj);
					}
			}
			/* move position to next element in the source array */
			pos += vallen;

			/* if it isn't a null we have protected it above */
			if (arr->nulls[i] == 0) {
				UNPROTECT(1);
			}
		}
		if (arr->meta->ndims > 0) {
			SEXP matrix_dims;

			/* attach dimensions */
			PROTECT(matrix_dims = allocVector(INTSXP, arr->meta->ndims));
			for (i = 0; i < arr->meta->ndims; i++) {
				INTEGER_DATA(matrix_dims)[i] = arr->meta->dims[i];
			}

			setAttrib(res, R_DimSymbol, matrix_dims);
			UNPROTECT(1);
		}
	}

	return res;
}

static SEXP plc_r_object_from_udt(char *input, plcRType *type) {
	plcUDT *udt;
	int i;
	SEXP res = R_NilValue;
	SEXP element = R_NilValue;
	SEXP row_names = R_NilValue;
	SEXP names = R_NilValue;

	udt = (plcUDT *) input;

	PROTECT(res = NEW_LIST(type->nSubTypes));
	PROTECT(names = NEW_CHARACTER(type->nSubTypes));

	for (i = 0; i < type->nSubTypes; i++) {
		if (type->subTypes[i].typeName != NULL) {
			if (udt->data[i].isnull) {
				PROTECT(element = R_NilValue);
			} else {
				element = type->subTypes[i].conv.inputfunc(udt->data[i].value,
				                                           &type->subTypes[i]);
			}

			SET_STRING_ELT(names, i, Rf_mkChar(type->subTypes[i].typeName));

			SET_VECTOR_ELT(res, i, element);
			UNPROTECT(1);

		} else {
			res = R_NilValue;
			break;
		}
	}
	/* attach the column names */
	setAttrib(res, R_NamesSymbol, names);
	UNPROTECT(1); //names

	/* attach row names - basically just the row number, zero based */
	PROTECT(row_names = allocVector(STRSXP, 1));
	SET_STRING_ELT(row_names, 0, Rf_mkChar("1"));

	setAttrib(res, R_RowNamesSymbol, row_names);
	UNPROTECT(1); // row_names

	/* finally, tell R we are a data.frame */
	setAttrib(res, R_ClassSymbol, mkString("data.frame"));

	/* we return res PROTECTED as per all of the other input functions */
	return res;
}

static SEXP plc_r_object_from_udt_ptr(char *input, plcRType *type) {
	return plc_r_object_from_udt(*((char **) input), type);
}

static SEXP plc_r_object_from_bytea(char *input, plcRType *type UNUSED) {
	SEXP result;
	SEXP s, t, obj;
	int status;
	int bsize;

	bsize = *((int *) input);
	PROTECT(obj = get_r_vector(PLC_DATA_BYTEA, bsize));
	memcpy((char *) RAW(obj), input + 4, bsize);

	/*
	 * Need to construct a call to
	 * unserialize(rval)
	 */
	PROTECT(t = s = allocList(2));
	SET_TYPEOF(s, LANGSXP);
	SETCAR(t, install("unserialize"));
	t = CDR(t);
	SETCAR(t, obj);

	PROTECT(result = R_tryEval(s, R_GlobalEnv, &status));
	if (status != 0) {
		if (last_R_error_msg) {

			raise_execution_error("R interpreter expression evaluation error: %s", last_R_error_msg);
		} else {
			raise_execution_error("R interpreter expression evaluation error: "
				                      "R expression evaluation error caught in \"unserialize\".");
		}
	}

	UNPROTECT(2);

	return result;
}

static int plc_r_object_as_int1(SEXP input, char **output, plcRType *type UNUSED) {
	int res = 0;
	char *out = (char *) malloc(1);
	*output = out;
	switch (TYPEOF(input)) {
		case LGLSXP:
			*((int8 *) out) = (int8) asLogical(input);
			break;
		case INTSXP:
			*((int8 *) out) = (int8) asInteger(input) == 0 ? 0 : 1;
			break;
		case REALSXP:
			*((int8 *) out) = (int8) asReal(input) == 0 ? 0 : 1;
			break;
		default:
			res = -1;
	}
	return res;
}

static int plc_r_object_as_int2(SEXP input, char **output, plcRType *type UNUSED) {
	int res = 0;
	char *out = (char *) malloc(2);
	*output = out;

	switch (TYPEOF(input)) {
		case LGLSXP:
			*((short *) out) = (short) asLogical(input);
			break;
		case INTSXP:
			*((short *) out) = (short) asInteger(input);
			break;
		case REALSXP:
			*((short *) out) = (short) asReal(input);
			break;
		default:
			res = -1;
	}

	return res;
}

static int plc_r_object_as_int4(SEXP input, char **output, plcRType *type UNUSED) {
	int res = 0;
	char *out = (char *) malloc(4);
	*output = out;

	switch (TYPEOF(input)) {
		case LGLSXP:
			*((int *) out) = (int) asLogical(input);
			break;
		case INTSXP:
			*((int *) out) = (int) asInteger(input);
			break;
		case REALSXP:
			*((int *) out) = (int) asReal(input);
			break;
		default:
			res = -1;
	}

	return res;
}

static int plc_r_object_as_int8(SEXP input, char **output, plcRType *type UNUSED) {
	int res = 0;
	char *out = (char *) malloc(8);
	*output = out;

	switch (TYPEOF(input)) {
		case LGLSXP:
			*((int64 *) out) = (int64) asLogical(input);
			break;
		case INTSXP:
			*((int64 *) out) = (int64) asInteger(input);
			break;
		case REALSXP:
			*((int64 *) out) = (int64) asReal(input);
			break;
		default:
			res = -1;
	}
	return res;
}

static int plc_r_object_as_float4(SEXP input, char **output, plcRType *type UNUSED) {
	int res = 0;
	char *out = (char *) malloc(4);
	*output = out;

	switch (TYPEOF(input)) {
		case LGLSXP:
			*((float4 *) out) = (float4) asLogical(input);
			break;
		case INTSXP:
			*((float4 *) out) = (float4) asInteger(input);
			break;
		case REALSXP:
			*((float4 *) out) = (float4) asReal(input);
			break;
		default:
			res = -1;
	}

	return res;
}

static int plc_r_object_as_float8(SEXP input, char **output, plcRType *type UNUSED) {
	int res = 0;
	char *out = (char *) malloc(8);
	*output = out;

	switch (TYPEOF(input)) {
		case LGLSXP:
			*((float8 *) out) = (float8) asLogical(input);
			break;
		case INTSXP:
			*((float8 *) out) = (float8) asInteger(input);
			break;
		case REALSXP:
			*((float8 *) out) = (float8) asReal(input);
			break;
		default:
			res = -1;
	}
	return res;
}

static int plc_r_object_as_text(SEXP input, char **output, plcRType *type UNUSED) {
	int res = 0;

	*output = strdup(CHAR(asChar(input)));
	return res;
}


static void plc_r_object_iter_free(plcIterator *iter) {
	plcArrayMeta *meta;
	plcRArrMeta *Rmeta;
	meta = (plcArrayMeta *) iter->meta;
	Rmeta = (plcRArrMeta *) iter->payload;
	pfree(meta->dims);
	pfree(Rmeta->dims);
	pfree(iter->meta);
	pfree(iter->payload);
	pfree(iter->position);
	return;
}

rawdata *plc_r_vector_element_rawdata(SEXP vector, int idx, plcRType *rtype) {
	rawdata *res = (rawdata *) pmalloc(sizeof(rawdata));
	if ((vector == R_NilValue)
	    || ((TYPEOF(vector) == LGLSXP) && (asLogical(vector) == NA_LOGICAL))
	    || ((TYPEOF(vector) == INTSXP) && (asInteger(vector) == NA_INTEGER))
	    || ((TYPEOF(vector) == REALSXP) && (asInteger(vector) == NA_REAL))
	    || ((TYPEOF(vector) == STRSXP) && (vector == NA_STRING))) {

		res->isnull = 1;
		res->value = NULL;
	} else {
		res->isnull = 0;
		switch (rtype->type) {
			case PLC_DATA_INT1:
				if (!IS_LOGICAL(vector)) {
					raise_execution_error("Actual R type is not matching excpected returned type %s [%d]",
					                      plc_get_type_name(rtype->type), rtype->type);
					pfree(res);
					res = NULL;
					break;
				}
				res->value = pmalloc(sizeof(int));
				if (LOGICAL_DATA(vector)[idx] == NA_LOGICAL) {
					res->isnull = 1;
					*((int *) res->value) = (int) 0;
				} else {
					*((int *) res->value) = LOGICAL_DATA(vector)[idx];
				}
				break;
			case PLC_DATA_INT2:
			case PLC_DATA_INT4:
				if (!IS_INTEGER(vector)) {
					raise_execution_error("Actual R type is not matching excpected returned type %s [%d]",
					                      plc_get_type_name(rtype->type), rtype->type);
					pfree(res);
					res = NULL;
					break;
				}
				/* 2 and 4 byte integer pgsql datatype => use R INTEGER */
				res->value = pmalloc(sizeof(int));
				if (INTEGER_DATA(vector)[idx] == NA_INTEGER) {
					*((int *) res->value) = (int) 0;
					res->isnull = 1;
				} else {
					*((int *) res->value) = INTEGER_DATA(vector)[idx];
				}
				break;

				/*
				 * Other numeric types => use R REAL
				 * Note pgsql int8 is mapped to R REAL
				 * because R INTEGER is only 4 byte
				 */

			case PLC_DATA_INT8:
				res->value = pmalloc(sizeof(int64));
				if (IS_INTEGER(vector)) {
					if (INTEGER_DATA(vector)[idx] == NA_INTEGER) {
						*((int64 *) res->value) = (int64) 0;
						res->isnull = 1;
					} else {
						*((int64 *) res->value) = (int64) (INTEGER_DATA(vector)[idx]);
					}
				} else if (IS_NUMERIC(vector)) {
					if (R_IsNA(NUMERIC_DATA(vector)[idx])) {
						res->isnull = 1;
						*((int64 *) res->value) = (int64) 0;
					} else {
						*((int64 *) res->value) = (int64) (NUMERIC_DATA(vector)[idx]);
					}
				} else {
					raise_execution_error("Actual R type is not matching excpected returned type %s [%d]",
					                      plc_get_type_name(rtype->type), rtype->type);
					pfree(res);
					res = NULL;
					break;
				}
				break;

			case PLC_DATA_FLOAT4:
				if (!IS_NUMERIC(vector)) {
					raise_execution_error("Actual R type is not matching excpected returned type %s [%d]",
					                      plc_get_type_name(rtype->type), rtype->type);
					pfree(res);
					res = NULL;
					break;
				}
				res->value = pmalloc(sizeof(float4));
				if (R_IsNA(NUMERIC_DATA(vector)[idx])) {
					res->isnull = 1;
					*((float4 *) res->value) = (float4) 0;
				} else {
					*((float4 *) res->value) = (float4) (NUMERIC_DATA(vector)[idx]);
				}
				break;
			case PLC_DATA_FLOAT8:
				if (!IS_NUMERIC(vector)) {
					raise_execution_error("Actual R type is not matching excpected returned type %s [%d]",
					                      plc_get_type_name(rtype->type), rtype->type);
					pfree(res);
					res = NULL;
					break;
				}
				res->value = pmalloc(sizeof(float8));
				if (R_IsNA(NUMERIC_DATA(vector)[idx])) {
					res->isnull = 1;
					*((float8 *) res->value) = (float8) 0;
				} else {
					*((float8 *) res->value) = (float8) (NUMERIC_DATA(vector)[idx]);
				}
				break;

			case PLC_DATA_UDT:
				if (VECTOR_ELT(vector, idx) == R_NilValue) {
					res->isnull = TRUE;
					res->value = pmalloc(sizeof(int));
					*((int *) res->value) = (int) 0;
				} else {
					res->isnull = FALSE;
					plc_r_object_as_udt(VECTOR_ELT(vector, idx), &res->value, rtype);
				}
				break;

			case PLC_DATA_ARRAY:

				if ((TYPEOF(vector) != LGLSXP) && (TYPEOF(vector) != REALSXP)
				    && (TYPEOF(vector) != INTSXP) && (TYPEOF(vector) != STRSXP)) {
					// these are arrays of primitives
					if (VECTOR_ELT(vector, idx) == R_NilValue) {
						res->isnull = TRUE;
						res->value = pmalloc(sizeof(int));
						*((int *) res->value) = (int) 0;
					} else {
						res->isnull = FALSE;
						plc_r_object_as_array(VECTOR_ELT(vector, idx), &res->value, rtype);
					}
				} else {
					if (vector == R_NilValue) {
						res->isnull = TRUE;
						res->value = pmalloc(sizeof(int));
						*((int *) res->value) = (int) 0;
					} else {
						res->isnull = FALSE;
						plc_r_object_as_array(vector, &res->value, rtype);
					}
				}

				break;

			case PLC_DATA_INVALID:
			case PLC_DATA_BYTEA:
				raise_execution_error("Arrays cannot handle element type %s [%d]", plc_get_type_name(rtype->type),
				                      rtype->type);
				break;

			case PLC_DATA_TEXT:
			default:
				/* Everything else is defaulted to string */
				if (vector == NA_STRING || STRING_ELT(vector, idx) == NA_STRING) {
					res->isnull = TRUE;
					res->value = NULL;
				} else {
					res->isnull = FALSE;
					res->value = pstrdup((char *) CHAR(STRING_ELT(vector, idx)));
				}
		}

	}
	return res;
}

static rawdata *plc_r_object_as_array_next(plcIterator *iter) {
	plcRArrMeta *meta;
	plcRArrPointer *ptrs;
	rawdata *res;
	SEXP mtx;
	int ptr;
	int idx;

	meta = (plcRArrMeta *) iter->payload;
	ptrs = (plcRArrPointer *) iter->position;

	ptr = meta->ndims - 1;
	idx = ptrs[ptr].pos;
	mtx = ptrs[ptr].obj;

	res = plc_r_vector_element_rawdata(mtx, idx, meta->type);
	ptrs[ptr].pos += 1;

	return res;
}

int plc_r_matrix_as_setof(SEXP input, int start, int dim1, char **output, plcRType *type) {

	plcRArrMeta *meta;
	plcArrayMeta *arrmeta;
	plcIterator *iter;
	size_t dims[PLC_MAX_ARRAY_DIMS];
	SEXP rdims;
	int ndims = 0;
	int res = 0;
	int i = 0;
	plcRArrPointer *ptrs;

	if (input != R_NilValue && (isVector(input) || isMatrix(input))) {
		PROTECT(rdims = getAttrib(input, R_DimSymbol));

		/*
		 * we are translating this from a linear array into n rows
		 * so we lose one of the dimensions
		 */

		if (rdims != R_NilValue) {
			ndims = 1;
			dims[0] = dim1;
		} else {
			ndims = 1;
			dims[0] = dim1;
		}
		UNPROTECT(1);


		/* Allocate the iterator */
		iter = (plcIterator *) pmalloc(sizeof(plcIterator));

		/* Initialize metas */
		arrmeta = (plcArrayMeta *) pmalloc(sizeof(plcArrayMeta));
		arrmeta->ndims = ndims;
		arrmeta->dims = (int *) pmalloc(ndims * sizeof(int));
		arrmeta->size = (ndims == 0) ? 0 : 1;
		arrmeta->type = type->subTypes[0].type;

		meta = (plcRArrMeta *) pmalloc(sizeof(plcRArrMeta));
		meta->ndims = ndims;
		meta->dims = (size_t *) pmalloc(ndims * sizeof(size_t));
		meta->outputfunc = plc_get_output_function(type->subTypes[0].type);
		meta->type = &type->subTypes[0];

		for (i = 0; i < ndims; i++) {
			meta->dims[i] = dims[i];
			arrmeta->dims[i] = (int) dims[i];
			arrmeta->size *= (int) dims[i];
		}

		iter->meta = arrmeta;
		iter->payload = (char *) meta;

		/* Initializing initial position */
		ptrs = (plcRArrPointer *) pmalloc(ndims * sizeof(plcRArrPointer));
		for (i = 0; i < ndims; i++) {
			ptrs[i].pos = start;
			/* TODO this only works for one dimensional arrays */
			ptrs[i].obj = input;
		}
		iter->position = (char *) ptrs;

		/* not sure why this is necessary */
		/* Initializing "data" */
		iter->data = (char *) input;

		/* Initializing "next" and "cleanup" functions */
		iter->next = plc_r_object_as_array_next;
		iter->cleanup = plc_r_object_iter_free;

		*output = (char *) iter;


	} else {
		*output = NULL;
		return -1;
	}
	return res;
}

static int plc_r_object_as_array(SEXP input, char **output, plcRType *type) {
	plcRArrMeta *meta;
	plcArrayMeta *arrmeta;
	plcIterator *iter;
	size_t dims[PLC_MAX_ARRAY_DIMS];
	SEXP rdims;
	int ndims = 0;
	int res = 0;
	int i = 0;
	plcRArrPointer *ptrs;

	/* We allow only vector to be returned as arrays */
	if (input != R_NilValue && (isVector(input) || isMatrix(input))) {
		/* TODO this is just for vectors */

		PROTECT(rdims = getAttrib(input, R_DimSymbol));
		if (rdims != R_NilValue) {
			ndims = length(rdims);
			for (i = 0; i < ndims; i++) {
				dims[i] = INTEGER(rdims)[i];
			}
		} else {
			ndims = 1;
			dims[0] = length(input);
		}
		UNPROTECT(1);


		/* Allocate the iterator */
		iter = (plcIterator *) pmalloc(sizeof(plcIterator));

		/* Initialize metas */
		arrmeta = (plcArrayMeta *) pmalloc(sizeof(plcArrayMeta));
		arrmeta->ndims = ndims;
		arrmeta->dims = (int *) pmalloc(ndims * sizeof(int));
		arrmeta->size = (ndims == 0) ? 0 : 1;
		arrmeta->type = type->subTypes[0].type;

		meta = (plcRArrMeta *) pmalloc(sizeof(plcRArrMeta));
		meta->ndims = ndims;
		meta->dims = (size_t *) pmalloc(ndims * sizeof(size_t));
		meta->outputfunc = plc_get_output_function(type->subTypes[0].type);
		meta->type = &type->subTypes[0];

		for (i = 0; i < ndims; i++) {
			meta->dims[i] = dims[i];
			arrmeta->dims[i] = (int) dims[i];
			arrmeta->size *= (int) dims[i];
		}

		iter->meta = arrmeta;
		iter->payload = (char *) meta;

		/* Initializing initial position */
		ptrs = (plcRArrPointer *) pmalloc(ndims * sizeof(plcRArrPointer));
		for (i = 0; i < ndims; i++) {
			ptrs[i].pos = 0;
			/* TODO this only works for one dimensional arrays */
			ptrs[i].obj = input;
		}
		iter->position = (char *) ptrs;

		/* not sure why this is necessary */
		/* Initializing "data" */
		iter->data = (char *) input;

		/* Initializing "next" and "cleanup" functions */
		iter->next = plc_r_object_as_array_next;
		iter->cleanup = plc_r_object_iter_free;

		*output = (char *) iter;
	} else {
		*output = NULL;
		res = -1;
	}

	return res;
}

static int plc_r_object_as_udt(SEXP input, char **output, plcRType *type) {
	int res = 0;
	SEXP names;
	SEXP dfcol;

	if ((names = Rf_GetColNames(input)) == NILSXP) {
		raise_execution_error("Output entry for plcRType must be a named list");
		res = -1;
	} else {
		int i = 0;
		plcUDT *udt;

		udt = pmalloc(sizeof(plcUDT));
		udt->data = pmalloc(type->nSubTypes * sizeof(rawdata));
		for (i = 0; i < type->nSubTypes && res == 0; i++) {

			PROTECT(dfcol = VECTOR_ELT(input, i));
			/*
			 * R stores characters in factors for efficiency...
			 */
			if (isFactor(dfcol)) {
				/*
				 * a factor is a special type of integer
				 * but must check for NA value first
				 */
				if (INTEGER(dfcol)[i] != NA_INTEGER) {
					SEXP c = Rf_asCharacterFactor(dfcol);

					rawdata *datum = plc_r_vector_element_rawdata(c, 0, &type->subTypes[i]);
					udt->data[i].isnull = datum->isnull;
					udt->data[i].value = datum->value;
					free(datum);

				} else {
					udt->data[i].isnull = TRUE;
					udt->data[i].value = NULL;
				}

			} else {
				rawdata *datum = plc_r_vector_element_rawdata(dfcol, 0, &type->subTypes[i]);
				udt->data[i].isnull = datum->isnull;
				udt->data[i].value = datum->value;
				free(datum);
			}
			UNPROTECT(1);
		}
		*output = (char *) udt;
	}

	return res;
}

static int plc_r_object_as_bytea(SEXP input, char **output, plcRType *type UNUSED) {
	SEXP obj;
	SEXP s, t;
	int len, status;
	char *result;

	/*
	 * Need to construct a call to
	 * serialize(rval, NULL)
	 */
	PROTECT(t = s = allocList(3));
	SET_TYPEOF(s, LANGSXP);
	SETCAR(t, install("serialize"));
	t = CDR(t);
	SETCAR(t, input);
	t = CDR(t);
	SETCAR(t, R_NilValue);

	PROTECT(obj = R_tryEval(s, R_GlobalEnv, &status));
	if (status != 0) {
		if (last_R_error_msg) {
			raise_execution_error("R interpreter expression evaluation error: %s", last_R_error_msg);
		} else {
			raise_execution_error("R interpreter expression evaluation error: "
				                      "R expression evaluation error caught in \"serialize\".");
		}
		return -1;
	}

	len = LENGTH(obj);
	result = pmalloc(len + 4);
	*((int *) result) = len;
	memcpy(result + 4, (char *) RAW(obj), len);
	*output = result;

	UNPROTECT(2);

	return 0;
}

static plcRInputFunc plc_get_input_function(plcDatatype dt, bool isArrayElement) {
	plcRInputFunc res = NULL;
	switch (dt) {
		case PLC_DATA_INT1:
			res = plc_r_object_from_int1;
			break;
		case PLC_DATA_INT2:
			res = plc_r_object_from_int2;
			break;
		case PLC_DATA_INT4:
			res = plc_r_object_from_int4;
			break;
		case PLC_DATA_INT8:
			res = plc_r_object_from_int8;
			break;
		case PLC_DATA_FLOAT4:
			res = plc_r_object_from_float4;
			break;
		case PLC_DATA_FLOAT8:
			res = plc_r_object_from_float8;
			break;
		case PLC_DATA_TEXT:
			if (isArrayElement) {
				res = plc_r_object_from_text_ptr;
			} else {
				res = plc_r_object_from_text;
			}
			break;
		case PLC_DATA_BYTEA:
			if (isArrayElement) {
				raise_execution_error("BYTEA data type is not supported as array element");
			}
			res = plc_r_object_from_bytea;
			break;
		case PLC_DATA_UDT:
			if (isArrayElement) {
				res = plc_r_object_from_udt_ptr;
			} else {
				res = plc_r_object_from_udt;
			}
			break;
		case PLC_DATA_ARRAY:
			res = plc_r_object_from_array;
			break;
		default:
			raise_execution_error("Type %s [%d] cannot be passed plc_get_input_function function",
			                      plc_get_type_name(dt), (int) dt);
			break;
	}
	return res;
}

plcROutputFunc plc_get_output_function(plcDatatype dt) {
	plcROutputFunc res = NULL;
	switch (dt) {
		case PLC_DATA_INT1:
			res = plc_r_object_as_int1;
			break;
		case PLC_DATA_INT2:
			res = plc_r_object_as_int2;
			break;
		case PLC_DATA_INT4:
			res = plc_r_object_as_int4;
			break;
		case PLC_DATA_INT8:
			res = plc_r_object_as_int8;
			break;
		case PLC_DATA_FLOAT4:
			res = plc_r_object_as_float4;
			break;
		case PLC_DATA_FLOAT8:
			res = plc_r_object_as_float8;
			break;
		case PLC_DATA_TEXT:
			res = plc_r_object_as_text;
			break;
		case PLC_DATA_BYTEA:
			res = plc_r_object_as_bytea;
			break;
		case PLC_DATA_ARRAY:
			res = plc_r_object_as_array;
			break;
		case PLC_DATA_UDT:
			res = plc_r_object_as_udt;
			break;
		default:
			raise_execution_error("Type %s [%d] cannot be passed plc_get_output_function function",
			                      plc_get_type_name(dt), (int) dt);
			break;
	}
	return res;
}

static void plc_parse_type(plcRType *Rtype, plcType *type, char *argName, bool isArrayElement) {
	int i = 0;

	Rtype->typeName = (type->typeName == NULL) ? NULL : strdup(type->typeName);
	Rtype->argName = (argName == NULL) ? NULL : strdup(argName);
	Rtype->type = type->type;
	Rtype->nSubTypes = type->nSubTypes;
	Rtype->conv.inputfunc = plc_get_input_function(Rtype->type, isArrayElement);
	Rtype->conv.outputfunc = plc_get_output_function(Rtype->type);
	if (Rtype->nSubTypes > 0) {
		isArrayElement = (type->type == PLC_DATA_ARRAY) ? true : false;
		Rtype->subTypes = (plcRType *) malloc(Rtype->nSubTypes * sizeof(plcRType));
		for (i = 0; i < type->nSubTypes; i++) {
			plc_parse_type(&Rtype->subTypes[i], &type->subTypes[i], NULL, isArrayElement);
		}
	} else {
		Rtype->subTypes = NULL;
	}
}

plcRFunction *plc_R_init_function(plcMsgCallreq *call) {
	plcRFunction *res;
	int i;

	res = (plcRFunction *) malloc(sizeof(plcRFunction));
	res->call = call;
	res->proc.src = strdup(call->proc.src);
	res->proc.name = strdup(call->proc.name);
	res->nargs = call->nargs;
	res->retset = call->retset;
	res->args = (plcRType *) malloc(res->nargs * sizeof(plcRType));

	for (i = 0; i < res->nargs; i++)
		plc_parse_type(&res->args[i], &call->args[i].type, call->args[i].name, false);

	plc_parse_type(&res->res, &call->retType, "results", false);

	return res;
}

plcRResult *plc_init_result_conversions(plcMsgResult *res) {
	plcRResult *Rres = NULL;
	uint32 i;

	Rres = (plcRResult *) malloc(sizeof(plcRResult));
	Rres->res = res;
	Rres->inconv = (plcRTypeConv *) malloc(res->cols * sizeof(plcRTypeConv));

	for (i = 0; i < res->cols; i++) {
		Rres->inconv[i].inputfunc = plc_get_input_function(res->types[i].type, false);
	}

	return Rres;
}

static void plc_r_free_type(plcRType *type) {
	int i = 0;
	for (i = 0; i < type->nSubTypes; i++)
		plc_r_free_type(&type->subTypes[i]);
	if (type->nSubTypes > 0)
		free(type->subTypes);
	if (type->argName != NULL) {
		free(type->argName);
	}
	if (type->typeName != NULL) {
		free(type->typeName);
	}
	return;
}

void plc_r_free_function(plcRFunction *func) {
	int i = 0;
	for (i = 0; i < func->nargs; i++)
		plc_r_free_type(&func->args[i]);
	plc_r_free_type(&func->res);

	free(func->args);
	free(func->proc.name);
	free(func->proc.src);
	free(func);
}

void plc_free_result_conversions(plcRResult *res) {
	free(res->inconv);
	free(res);
}

void plc_r_copy_type(plcType *type, plcRType *rtype) {
	type->type = rtype->type;
	type->nSubTypes = rtype->nSubTypes;
	type->typeName = (rtype->typeName == NULL) ? NULL : strdup(rtype->typeName);
	if (type->nSubTypes > 0) {
		int i = 0;
		type->subTypes = (plcType *) pmalloc(type->nSubTypes * sizeof(plcType));
		for (i = 0; i < type->nSubTypes; i++)
			plc_r_copy_type(&type->subTypes[i], &rtype->subTypes[i]);
	} else {
		type->subTypes = NULL;
	}
}

/*
 * create an R vector of a given type and size based on pg output function oid
 */
SEXP get_r_vector(plcDatatype type_id, int numels) {
	SEXP result = R_NilValue;

	switch (type_id) {
		case PLC_DATA_INT1:
			PROTECT(result = NEW_LOGICAL(numels));
			break;
		case PLC_DATA_INT2:
		case PLC_DATA_INT4:
			PROTECT(result = NEW_INTEGER(numels));
			break;
		case PLC_DATA_INT8:
		case PLC_DATA_FLOAT4:
		case PLC_DATA_FLOAT8:
			PROTECT(result = NEW_NUMERIC(numels));
			break;
		case PLC_DATA_UDT:
			PROTECT(result = NEW_LIST(numels));
			break;
		case PLC_DATA_BYTEA:
			PROTECT(result = NEW_RAW(numels));
			break;
		case PLC_DATA_TEXT:
		default:
			PROTECT(result = NEW_CHARACTER(numels));
			break;
	}
	UNPROTECT(1);
	return result;
}
