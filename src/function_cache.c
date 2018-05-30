/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016, Pivotal.
 *
 *------------------------------------------------------------------------------
 */

#ifdef PLC_PG
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include "function_cache.h"
#ifdef PLC_PG
#pragma GCC diagnostic pop
#endif

#include "message_fns.h"

static plcProcInfo *plcFunctionCache[PLC_FUNCTION_CACHE_SIZE];
static int plcFunctionCacheInitialized = 0;

/* Move up the cache item */
void function_cache_up(int index) {
	plcProcInfo *tmp;
	int i;
	if (index > 0) {
		tmp = plcFunctionCache[index];
		for (i = index; i > 0; i--) {
			plcFunctionCache[i] = plcFunctionCache[i - 1];
		}
		plcFunctionCache[0] = tmp;
	}
}

plcProcInfo *function_cache_get(Oid funcOid) {
	int i;
	plcProcInfo *resFunc = NULL;
	/* Initialize all the elements with nulls initially */
	if (!plcFunctionCacheInitialized) {
		for (i = 0; i < PLC_FUNCTION_CACHE_SIZE; i++) {
			plcFunctionCache[i] = NULL;
		}
		plcFunctionCacheInitialized = 1;
	}
	for (i = 0; i < PLC_FUNCTION_CACHE_SIZE; i++) {
		if (plcFunctionCache[i] != NULL &&
		    plcFunctionCache[i]->funcOid == funcOid) {
			resFunc = plcFunctionCache[i];
			function_cache_up(i);
			break;
		}
	}
	return resFunc;
}

void function_cache_put(plcProcInfo *func) {
	int i;
	plcProcInfo *oldFunc;
	oldFunc = function_cache_get(func->funcOid);
	/* If the function is not cached already */
	if (oldFunc == NULL) {
		/* If the last element exists we need to free its memory */
		if (plcFunctionCache[PLC_FUNCTION_CACHE_SIZE - 1] != NULL) {
			free_proc_info(plcFunctionCache[PLC_FUNCTION_CACHE_SIZE - 1]);
		}
		/* Move our LRU cache right */
		for (i = PLC_FUNCTION_CACHE_SIZE - 1; i > 0; i--) {
			plcFunctionCache[i] = plcFunctionCache[i - 1];
		}
		plcFunctionCache[0] = func;
	} else {
		free_proc_info(oldFunc);
		plcFunctionCache[0] = func;
	}
}
