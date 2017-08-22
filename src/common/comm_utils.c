/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016, Pivotal.
 *
 *------------------------------------------------------------------------------
 */
#include "comm_utils.h"

#ifndef COMM_STANDALONE

    #include "utils/memutils.h"
    #include "utils/palloc.h"

    void *plc_top_alloc(size_t bytes) {
    	/* We need our allocations to be long-lived, so use TopMemoryContext */
    	return MemoryContextAlloc(TopMemoryContext, bytes);
    }
    
    char *plc_top_strdup(char *str) {
        int   len = strlen(str);
        char *out = plc_top_alloc(len + 1);
        memcpy(out, str, len);
        out[len] = '\0';
        return out;
    }

#else

	void *pmalloc(size_t size) {
		void *addr = malloc(size);
		if (addr == NULL)
			lprintf(ERROR, "Fail to allocate %ld bytes", (unsigned long) size);
		return addr;
	}

#endif /* COMM_STANDALONE */
