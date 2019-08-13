/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */
#ifndef PLC_COMM_SERVER_H
#define PLC_COMM_SERVER_H

#include "comm_connectivity.h"

// Enabling debug would create infinite loop of client receiving connections
//#define _DEBUG_SERVER

#define SERVER_PORT 8080

// Timeout in seconds for server to wait for client connection
#define TIMEOUT_SEC 20


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

void *palloc(size_t size);

#define PLy_malloc palloc
#define pfree free
#define pstrdup strdup
#define plc_top_strdup strdup

void set_signal_handlers(void);

int sanity_check_client(void);

int start_listener(void);

void connection_wait(int sock);

plcConn *connection_init(int sock);

void receive_loop(void (*handle_call)(plcMsgCallreq *, plcConn *), plcConn *conn);

#endif /* PLC_COMM_SERVER_H */
