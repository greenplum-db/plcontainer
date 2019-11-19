#include <stdarg.h>
#include "postgres.h"
#include "lib/stringinfo.h"
#include "utils/memutils.h"
#include "common/comm_dummy.h"
#include "common/comm_connectivity.h"

extern void plc_elog(int log_level, const char *format, ...) pg_attribute_printf(2,3);

void plc_elog(int log_level, const char *format, ...)
{
	StringInfoData buf;
	initStringInfo(&buf);
	appendStringInfo(&buf, "plcontainer log: ");
	for(;;) {
		va_list args;
		int needed;
		va_start(args, format);
		needed = appendStringInfoVA(&buf, format, args);
		va_end(args);
		if (needed == 0)
			break;
		enlargeStringInfo(&buf, needed);
	}

	if (log_level == ERROR && global_context) {
		plcContextLogging(LOG, global_context);
	}

	elog(log_level, "%s", buf.data);
	pfree(buf.data);
}

void *txn_palloc(size_t size) {
	/* Allocat memory to live in the transcation level */
	if (TopTransactionContext != NULL)
		return MemoryContextAlloc(TopTransactionContext, size);
	else
		return MemoryContextAlloc(CurrentMemoryContext, size);
}
