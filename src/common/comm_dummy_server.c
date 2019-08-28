#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "common/comm_dummy.h"

int server_log_level;

void *palloc(size_t size)
{
	void *addr = malloc(size);
	if (addr == NULL)
		plc_elog(ERROR, "Fail to allocate %lu bytes", (unsigned long) size);
	return addr;
}

void pfree(void *ptr)
{
	return free(ptr);
}

char *pstrdup(const char *str)
{
	char *s = strdup(str);
	if (!s)
		plc_elog(ERROR, "Failed to strdup a string('%s')", str);
	return s;
}

static const char *log_level_to_string(int log_level)
{
	switch (log_level) {
	case ERROR: return "ERROR";
	case FATAL: return "FATAL";
	case LOG: return "LOG";
	case NOTICE: return "NOTICE";
	case WARNING: return "WARNING";
	case INFO: return "INFO";
	case DEBUG5: return "DEBUG5";
	case DEBUG4: return "DEBUG4";
	case DEBUG3: return "DEBUG3";
	case DEBUG2: return "DEBUG2";
	case DEBUG1: return "DEBUG1";
	case COMMERROR: return "COMMERROR";
	case PANIC: return "PANIC";
	default: break;
	}
	return "<unknown log level>";
}

static int is_write_log(int elevel, int log_min_level)
{
	if (elevel == LOG || elevel == COMMERROR)
	{
		if (log_min_level == LOG || log_min_level <= ERROR)
			return 1;
	}
	else if (log_min_level == LOG)
	{
		/* elevel != LOG */
		if (elevel >= FATAL)
			return 1;
	}
	/* Neither is LOG */
	else if (elevel >= log_min_level)
		return 1;

	return 0;

}
void plc_elog(int lvl, const char *format, ...)
{
	FILE *out = stdout;
	if (lvl >= ERROR) {
		out = stderr;
	}
	if (is_write_log(lvl, server_log_level)) {
		va_list ap;
		fprintf(out, "plcontainer server log: %s:", log_level_to_string(lvl));
		va_start(ap, format);
		vfprintf(out, format, ap);
		va_end(ap);
		fprintf(out, "\n");
		fflush(out);
	}
	if (lvl >= ERROR) {
		exit(1);
	}

}

void init_pplan_slots(plcContext *ctx)
{
	(void)ctx;
	plc_elog(FATAL, "This function should not be called in server side!");
}

void deinit_pplan_slots(plcContext *ctx)
{
	(void)ctx;
	plc_elog(FATAL, "This function should not be called in server side!");
}


