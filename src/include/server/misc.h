/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */
#ifndef PLC_SERVER_MISC_H
#define PLC_SERVER_MISC_H

#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <execinfo.h>
/* Compatibility with R that defines WARNING and ERROR by itself */
#undef WARNING
#undef ERROR

/* Error level codes from GPDB utils/elog.h header */
#define DEBUG5     10
#define DEBUG4     11
#define DEBUG3     12
#define DEBUG2     13
#define DEBUG1     14
#define LOG        15
#define COMMERROR  16
#define INFO       17
#define NOTICE     18
#define WARNING    19
#define ERROR      20
#define FATAL      21
#define PANIC      22
/* End of extraction from utils/elog.h */

// Global log level for server
int server_log_level;

extern int is_write_log(int elevel, int log_min_level);

#define plc_elog(lvl, fmt, ...)                                         \
        do {                                                            \
            FILE *out = stdout;                                         \
            if (lvl >= ERROR) {                                         \
                out = stderr;                                           \
            }                                                           \
            if (is_write_log(lvl, server_log_level)) {                  \
              fprintf(out, "plcontainer log: ");                        \
              fprintf(out, #lvl ": ");                                  \
              fprintf(out, fmt, ##__VA_ARGS__);                         \
              fprintf(out, "\n");                                       \
              fflush(out);                                              \
            }                                                           \
            if (lvl >= ERROR) {                                         \
                exit(1);                                                \
            }                                                           \
        } while (0)

void *palloc(size_t size);

#define PLy_malloc palloc
#define pfree free
#define pstrdup strdup
#define plc_top_strdup strdup

#endif /* PLC_SERVER_MISC_H */

