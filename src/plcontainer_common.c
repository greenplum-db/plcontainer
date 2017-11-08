/**
 * SQL message handler implementation.
 *
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#include "postgres.h"

#include "plcontainer_common.h"

/* Some utility functions
 * They are consistent with upstream plpython.c
 * where the functions are called PLy_free and PLy_malloc.
 * */
void *
plcontainer_malloc(size_t bytes)
{
	/* We need our allocations to be long-lived, so use TopMemoryContext */
	return MemoryContextAlloc(TopMemoryContext, bytes);
}
void
plcontainer_free(void *ptr)
{
	if(ptr != NULL)
		pfree(ptr);
	ptr = NULL;
}
