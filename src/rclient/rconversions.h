/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */
#ifndef PLC_RCONVERSIONS_H
#define PLC_RCONVERSIONS_H

#include <R.h>
#include <Rembedded.h>
#include <Rinternals.h>
#include <Rdefines.h>

#include "common/messages/messages.h"
#include "common/comm_utils.h"

#define PLC_MAX_ARRAY_DIMS 2

typedef struct plcRType plcRType;

typedef SEXP (*plcRInputFunc)(char *, plcRType *);

typedef int (*plcROutputFunc)(SEXP, char **, plcRType *);

typedef struct plcRArrPointer {
	size_t pos;
	SEXP obj;
} plcRArrPointer;

typedef struct plcRArrMeta {
	int ndims;
	size_t *dims;
	plcRType *type;
	plcROutputFunc outputfunc;
} plcRArrMeta;

typedef struct plcRTypeConv {
	plcRInputFunc inputfunc;
	plcROutputFunc outputfunc;
} plcRTypeConv;

typedef struct plcRResult {
	plcMsgResult *res;
	plcRTypeConv *inconv;
} plcRResult;

struct plcRType {
	plcDatatype type;
	char *argName;
	char *typeName;
	int nSubTypes;
	plcRType *subTypes;
	plcRTypeConv conv;
};

typedef struct plcRFunction {
	plcProcSrc proc;
	plcMsgCallreq *call;
	SEXP RProc;
	int nargs;
	int retset;
	unsigned int objectid;
	plcRType *args;
	plcRType res;
} plcRFunction;

plcRFunction *plc_R_init_function(plcMsgCallreq *call);

void plc_r_copy_type(plcType *type, plcRType *pytype);

plcRResult *plc_init_result_conversions(plcMsgResult *res);

void plc_r_free_function(plcRFunction *func);

void plc_free_result_conversions(plcRResult *res);

SEXP get_r_vector(plcDatatype type_id, int numels);

rawdata *plc_r_vector_element_rawdata(SEXP vector, int idx, plcRType *type);

int plc_r_matrix_as_setof(SEXP input, int start, int dim1, char **output, plcRType *type);

plcROutputFunc plc_get_output_function(plcDatatype dt);

//#define DEBUGPROTECT

#ifdef DEBUGPROTECT
#undef PROTECT
extern SEXP pg_protect(SEXP s, char *fn, int ln);
#define PROTECT(s)      pg_protect(s, __FILE__, __LINE__)

#undef UNPROTECT
extern void pg_unprotect(int n, char *fn, int ln);
#define UNPROTECT(n)    pg_unprotect(n, __FILE__, __LINE__)
#endif /* DEBUGPROTECT */


#endif /* PLC_RCONVERSIONS_H */
