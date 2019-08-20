/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <execinfo.h>
#include <unistd.h>

#include "server_misc.h"

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

void *palloc(size_t size) {
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
