/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#include "common/comm_dummy.h"

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
