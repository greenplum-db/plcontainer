/*
Copyright 1994 The PL-J Project. All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE PL-J PROJECT ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
THE PL-J PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
   OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
   OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those of the authors and should not be
interpreted as representing official policies, either expressed or implied, of the PL-J Project.
*/

/**
 * file:            comm_logging.h.
 * author:            PostgreSQL developement group.
 * author:            Laszlo Hornyak
 */

/*
 * Portions Copyright © 2016-Present Pivotal Software, Inc.
 */

#ifndef PLC_COMM_UTILS_H
#define PLC_COMM_UTILS_H

/*
  PLC_CLIENT should be defined for standalone interpreters
  running inside containers, since they don't have access to postgres
  symbols. If it was defined, plc_elog will print the logs to stdout or
  in case of an error to stderr. pmalloc, pfree & pstrdup will use the
  std library.
*/
#ifdef PLC_CLIENT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <execinfo.h>

#include "comm_log.h"

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

/* Postgres-specific types from GPDB c.h header */
typedef signed char int8;        /* == 8 bits */
typedef signed short int16;      /* == 16 bits */
typedef signed int int32;        /* == 32 bits */
typedef unsigned int uint32;     /* == 32 bits */
typedef long long int int64;     /* == 64 bits */
#define INT64_FORMAT "%lld"
typedef float float4;
typedef double float8;
typedef char bool;
#define true    ((bool) 1)
#define false   ((bool) 0)
/* End of extraction from c.h */

extern int is_write_log(int elevel, int log_min_level);

#define plc_elog(lvl, fmt, ...)                                          \
        do {                                                            \
            FILE *out = stdout;                                         \
            if (lvl >= ERROR) {                                         \
                out = stderr;                                           \
            }                                                           \
            if (is_write_log(lvl, client_log_level)) {              \
              fprintf(out, "plcontainer log: %s, ", clientLanguage);    \
              fprintf(out, "%s, %s, %d, ", dbUsername, dbName, dbQePid);\
              fprintf(out, #lvl ": ");                                  \
              fprintf(out, fmt, ##__VA_ARGS__);                         \
              fprintf(out, "\n");                                       \
              fflush(out);                                              \
            }                                                           \
            if (lvl >= ERROR) {                                         \
                exit(1);                                                \
            }                                                           \
        } while (0)

void *pmalloc(size_t size);

#define PLy_malloc pmalloc
#define pfree free
#define pstrdup strdup
#define plc_top_strdup strdup

void set_signal_handlers(void);

int sanity_check_client(void);

#else /* PLC_CLIENT */

#include "postgres.h"

#ifdef PLC_PG
#define write_log printf
#endif

/* QE process uses elog, while cleanup process should use write_log*/
#define backend_log(elevel, ...)  \
	do { \
		if (getppid() == PostmasterPid) { \
			plc_elog(elevel, __VA_ARGS__); \
		} else { \
			write_log(__VA_ARGS__); \
		} \
	} while(0)

#define plc_elog(lvl, fmt, ...) elog(lvl, "plcontainer: " fmt, ##__VA_ARGS__);
#define pmalloc palloc

/* pfree & pstrdup are already defined by postgres */
void *PLy_malloc(size_t bytes);

char *plc_top_strdup(char *str);

char *PLy_strdup(const char *);

#endif /* PLC_CLIENT */

#endif /* PLC_COMM_UTILS_H */
