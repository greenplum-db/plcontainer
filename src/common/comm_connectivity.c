/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common/comm_connectivity.h"
#include "common/comm_dummy.h"

plcContext *global_context = NULL;

void plcContextBeginStage(plcContext *ctx, const char *stage_name, const char *message_format, ...)
	__attribute__ ((format (printf, 3, 4)));

void plcContextEndStage(plcContext *ctx, const char *stage_name, plcContextStageStatus status, const char *message_format, ...)
	__attribute__ ((format (printf, 4, 5)));

void plcContextInit(plcContext *ctx)
{
	ctx->service_address = NULL;
	ctx->container_id = NULL;
	ctx->current_stage_num = 0;
	ctx->max_stage_num = MAX_PLC_CONTEXT_STAGE_NUM;
	ctx->is_new_ctx = true;
	global_context = ctx;
}

/*
 * This function only release buffers and reset plan array.
 * NOTE: socket fd keeps open and service address is still valid.
 * We INTEND to reuse connection to the container.
 */
void plcReleaseContext(plcContext *ctx)
{
	ctx->current_stage_num = 0;
	ctx->is_new_ctx = false;
	global_context = ctx;
}

/*
 *  This function is used for re-init buffer and plan slot
 *  when a new query coming and connection is resued.
 */

void plcContextReset(plcContext *ctx)
{
	ctx->current_stage_num = 0;
	ctx->is_new_ctx = false;
	global_context = ctx;
}

/*
 * clearup the container connection context
 */
void plcFreeContext(plcContext *ctx)
{
	plcReleaseContext(ctx);
	pfree(ctx->service_address);
	pfree(ctx->container_id);
	pfree(ctx);
	global_context = NULL;
}

void plcContextBeginStage(plcContext *ctx, const char *stage_name, const char *message_format, ...)
{
    if (ctx->current_stage_num >= ctx->max_stage_num) {
        plc_elog(ERROR, "plcContext has too many stages.");
    }

    plcContextStage *stage = &ctx->stages[ctx->current_stage_num];
    strncpy(stage->name, stage_name, MAX_PLC_CONTEXT_STAGE_NAME_SIZE-1);
    gettimeofday(&stage->begin_time, NULL);
   
    stage->message[0] = '\0'; 
    if (message_format) {
        va_list args;
        va_start(args, message_format);
        vsnprintf(stage->message, MAX_PLC_CONTEXT_STAGE_MESSAGE_SIZE, message_format, args);
        va_end(args); 
    }

}

void plcContextEndStage(plcContext *ctx, const char *stage_name, plcContextStageStatus status, const char *message_format, ...) {
    plcContextStage *stage = &ctx->stages[ctx->current_stage_num];
    if (strcmp(stage->name, stage_name) != 0) {
        plc_elog(ERROR, "plcContext finish stage error, current stage %s != %s", stage->name, stage_name);
    }

    gettimeofday(&stage->end_time, NULL);
    stage->cost_ms = stage->end_time.tv_sec * 1000 + stage->end_time.tv_usec / 1000 
                    - stage->begin_time.tv_sec * 1000 - stage->begin_time.tv_usec / 1000;

    stage->status = status;

     if (message_format) {
        va_list args;
        va_start(args, message_format);
        vsnprintf(stage->message+strlen(stage->message), MAX_PLC_CONTEXT_STAGE_MESSAGE_SIZE-strlen(stage->message), message_format, args);
        va_end(args); 
    }
    ctx->current_stage_num++;
}

void plcContextLogging(int log_level, plcContext *ctx) {
    char logbuf[MAX_PLC_CONTEXT_STAGE_MESSAGE_SIZE * MAX_PLC_CONTEXT_STAGE_NUM] = "PLContainer Trace Logging:\n";
    for (int i=0;i<ctx->current_stage_num;i++) {
        plcContextStage *stage = &ctx->stages[i];
        snprintf(logbuf+strlen(logbuf), MAX_PLC_CONTEXT_STAGE_MESSAGE_SIZE*MAX_PLC_CONTEXT_STAGE_NUM-strlen(logbuf), 
                "\nSTAGE_%d:%s STATUS:%d COST:%dms MESSAGE:%s", i, stage->name, stage->status, stage->cost_ms, stage->message); 
    }
    plc_elog(log_level, "%s", logbuf);
}
