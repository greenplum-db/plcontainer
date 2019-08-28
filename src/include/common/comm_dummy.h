#ifndef _COMMON_DUMMY_H
#define _COMMON_DUMMY_H

#include "common/comm_connectivity.h"

#ifdef PLC_SERVER
#include <stdbool.h>
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

#else /* PLC_SERVER */
#include "postgres.h"
#include "miscadmin.h"
#include "utils/elog.h"
#include "utils/palloc.h"
#endif

extern char *pstrdup(const char *str);
extern void pfree(void *ptr);
extern void *palloc(size_t size);

extern void plc_elog(int log_level, const char *format, ...);

void deinit_pplan_slots(plcContext *ctx);
void init_pplan_slots(plcContext *ctx);

#endif
