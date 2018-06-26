/*------------------------------------------------------------------------------
 *
 *
 * Portions Copyright (c) 2016, Pivotal.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 *
 *------------------------------------------------------------------------------
 */
#include "comm_utils.h"

#ifndef PLC_CLIENT

#include "utils/memutils.h"
#include "utils/palloc.h"

void *PLy_malloc(size_t bytes) {
	/* We need our allocations to be long-lived, so use TopMemoryContext */
	return MemoryContextAlloc(TopMemoryContext, bytes);
}

char *
PLy_strdup(const char *str)
{
	char	   *result;
	size_t		len;

	len = strlen(str) + 1;
	result = PLy_malloc(len);
	memcpy(result, str, len);

	return result;
}

char *plc_top_strdup(char *str) {
	int len = strlen(str);
	char *out = PLy_malloc(len + 1);
	memcpy(out, str, len);
	out[len] = '\0';
	return out;
}

#else

/*
 * This function is copied from is_log_level_output in elog.c
 */
int
is_write_log(int elevel, int log_min_level)
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

void *pmalloc(size_t size) {
	void *addr = malloc(size);
	if (addr == NULL)
		plc_elog(ERROR, "Fail to allocate %ld bytes", (unsigned long) size);
	return addr;
}

typedef void (*signal_handler)(int);

static void set_signal_handler(int signo, int sigflags, signal_handler func) {
	int ret;
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = func;
	sa.sa_flags = sigflags;
	sigemptyset(&sa.sa_mask);

	ret = sigaction(signo, &sa, NULL);
	if (ret < 0) {
			plc_elog(ERROR, "sigaction(%d with flag 0x%x) failed: %s", signo,
			        sigflags, strerror(errno));
		return;
	}

	return;
}

static void sigsegv_handler() {
	void *stack[64];
	int size;

	size = backtrace(stack, 100);
		plc_elog(LOG, "signal SIGSEGV was captured in pl/container. Stack:");
	fflush(stdout);

	/* Do not call backtrace_symbols() since it calls malloc(3) which is not
	 * async signal safe.
	 */
	backtrace_symbols_fd(stack, size, STDERR_FILENO);
	fflush(stderr);

	raise(SIGSEGV);
}

void set_signal_handlers() {
	set_signal_handler(SIGSEGV, SA_RESETHAND, sigsegv_handler);
}

int sanity_check_client()
{
	if (sizeof(int8) != 1) {
		plc_elog(ERROR, "length of int8 (%ld) is not 1", sizeof(int8));
		return -1;
	}

	if (sizeof(int16) != 2) {
		plc_elog(ERROR, "length of int16 (%ld) is not 2", sizeof(int16));
		return -1;
	}

	if (sizeof(int32) != 4) {
		plc_elog(ERROR, "length of int32 (%ld) is not 4", sizeof(int32));
		return -1;
	}

	if (sizeof(uint32) != 4) {
		plc_elog(ERROR, "length of uint32 (%ld) is not 4", sizeof(uint32));
		return -1;
	}

	if (sizeof(int64) != 8) {
		plc_elog(ERROR, "length of int64 (%ld) is not 8", sizeof(int64));
		return -1;
	}

	if (sizeof(float4) != 4) {
		plc_elog(ERROR, "length of float4 (%ld) is not 4", sizeof(float4));
		return -1;
	}

	if (sizeof(float8) != 8) {
		plc_elog(ERROR, "length of float8 (%ld) is not 8", sizeof(float8));
		return -1;
	}

	return 0;
}

#endif /* PLC_CLIENT */
