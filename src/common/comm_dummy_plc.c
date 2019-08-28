#include <stdarg.h>
#include "postgres.h"
#include "lib/stringinfo.h"
#include "common/comm_dummy.h"

extern void plc_elog(int log_level, const char *format, ...) pg_attribute_printf(2,3);

void plc_elog(int log_level, const char *format, ...)
{
	StringInfoData buf;
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
	elog(log_level, "%s", buf.data);
	pfree(buf.data);
}
