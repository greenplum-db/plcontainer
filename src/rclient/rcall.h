/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */
#ifndef PLC_RCALL_H
#define PLC_RCALL_H

#include "common/messages/messages.h"
#include "common/comm_connectivity.h"

#define UNUSED __attribute__ (( unused ))

// Global connection object
extern plcConn *plcconn_global;

// Global execution termination flag
extern int plc_is_execution_terminated;

// Processing of the Greenplum function call
void handle_call(plcMsgCallreq *req, plcConn *conn);

// Initialization of R module
int r_init(void);

void raise_execution_error(const char *format, ...);

void plc_raise_delayed_error(plcConn *conn);

#endif /* PLC_RCALL_H */
