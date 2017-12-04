/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016, Pivotal.
 *
 *------------------------------------------------------------------------------
 */

#include <Python.h>

#include "pycache.h"
#include "pyconversions.h"
#include "common/comm_utils.h"

static plcPyFunction **plcPyFuncCache = NULL;

static void plc_py_function_cache_up(int index);

/* Move up the cache item */
static void plc_py_function_cache_up(int index) {
	plcPyFunction *tmp;
	int i;
	if (index > 0) {
		tmp = plcPyFuncCache[index];
		for (i = index; i > 0; i--) {
			plcPyFuncCache[i] = plcPyFuncCache[i - 1];
		}
		plcPyFuncCache[0] = tmp;
	}
}

plcPyFunction *plc_py_function_cache_get(unsigned int objectid) {
	int i;
	plcPyFunction *resFunc = NULL;
	/* Initialize all the elements with nulls initially */
	if (plcPyFuncCache == NULL) {
		plcPyFuncCache = malloc(PLC_PY_FUNCTION_CACHE_SIZE * sizeof(plcPyFunction));
		for (i = 0; i < PLC_PY_FUNCTION_CACHE_SIZE; i++) {
			plcPyFuncCache[i] = NULL;
		}
	}
	for (i = 0; i < PLC_PY_FUNCTION_CACHE_SIZE; i++) {
		if (plcPyFuncCache[i] != NULL &&
		    plcPyFuncCache[i]->objectid == objectid) {
			resFunc = plcPyFuncCache[i];
			plc_py_function_cache_up(i);
			break;
		}
	}
	return resFunc;
}

void plc_py_function_cache_put(plcPyFunction *func) {
	int i;
	plcPyFunction *oldFunc;
	oldFunc = plc_py_function_cache_get(func->objectid);
	/* If the function is not cached already */
	if (oldFunc == NULL) {
		/* If the last element exists we need to free its memory */
		if (plcPyFuncCache[PLC_PY_FUNCTION_CACHE_SIZE - 1] != NULL) {
			plc_py_free_function(plcPyFuncCache[PLC_PY_FUNCTION_CACHE_SIZE - 1]);
		}
		/* Move our LRU cache right */
		for (i = PLC_PY_FUNCTION_CACHE_SIZE - 1; i > 0; i--) {
			plcPyFuncCache[i] = plcPyFuncCache[i - 1];
		}
	} else {
		plc_py_free_function(oldFunc);
	}
	plcPyFuncCache[0] = func;
}
