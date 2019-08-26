/*------------------------------------------------------------------------------
 *
 *
 * Portions Copyright (c) 2016, Pivotal.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 *
 *------------------------------------------------------------------------------
 */

#include "misc.h"
#include "utils/memutils.h"
#include "utils/palloc.h"

// TODO: rename PLy_malloc to a more meaningful name
void *top_palloc(size_t bytes) {
	/* We need our allocations to be long-lived, so use TopMemoryContext */
	return MemoryContextAlloc(TopMemoryContext, bytes);
}

char *plc_top_strdup(const char *str) {
	int len = strlen(str);
	char *out = top_palloc(len + 1);
	memcpy(out, str, len);
	out[len] = '\0';
	return out;
}
