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

#ifndef PLC_COMM_UTILS_H
#define PLC_COMM_UTILS_H

/*
  COMM_STANDALONE should be defined for standalone interpreters
  running inside containers, since they don't have access to postgres
  symbols. If it was defined, lprintf will print the logs to stdout or
  in case of an error to stderr. pmalloc, pfree & pstrdup will use the
  std library.
*/
#ifdef COMM_STANDALONE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <execinfo.h>

    /* Compatibility with R that defines WARNING and ERROR by itself */
    #undef WARNING
    #undef ERROR

    /* Error level codes from GPDB utils/elog.h header */
    #define DEBUG2     13
    #define DEBUG1     14
    #define LOG        15
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
    typedef long long int int64;     /* == 64 bits */
    typedef float float4;
    typedef double float8;
    typedef char bool;
    #define true    ((bool) 1)
    #define false   ((bool) 0)
    /* End of extraction from c.h */

    #define lprintf(lvl, fmt, ...)            \
        do {                                  \
            FILE *out = stdout;               \
            if (lvl >= ERROR) {               \
                out = stderr;                 \
            }                                 \
            fprintf(out, #lvl ": ");          \
            fprintf(out, fmt, ##__VA_ARGS__); \
            fprintf(out, "\n");               \
            if (lvl >= ERROR) {               \
				fflush(out);                  \
                exit(1);                      \
            }                                 \
        } while (0)
	void *pmalloc(size_t size);
    #define plc_top_alloc pmalloc
    #define pfree free
    #define pstrdup strdup
    #define plc_top_strdup strdup

	void set_signal_handlers(void);

#else /* COMM_STANDALONE */

#include "postgres.h"

    #define lprintf elog
    #define pmalloc palloc
    /* pfree & pstrdup are already defined by postgres */
    void *plc_top_alloc(size_t bytes);
    char *plc_top_strdup(char *str);

#endif /* COMM_STANDALONE */

#endif /* PLC_COMM_UTILS_H */
