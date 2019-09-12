/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */
#ifndef PLC_COMM_SERVER_H
#define PLC_COMM_SERVER_H

#include "common/comm_connectivity.h"

/*
  SERVER should be defined for standalone interpreters
  running inside containers, since they don't have access to postgres
  symbols. If it was defined, plc_elog will print the logs to stdout or
  in case of an error to stderr. palloc, pfree & pstrdup will use the
  std library.
*/

// Enabling debug would create infinite loop of client receiving connections
//#define _DEBUG_SERVER

#define SERVER_PORT 8080

// Timeout in seconds for server to wait for client connection
#define TIMEOUT_SEC 20

void set_signal_handlers(void);

int sanity_check_client(void);

int start_listener(const char* stand_alone_uds);

void connection_wait(int sock);

plcConn *connection_init(int sock);

plcConn *start_server(const char *stand_alone_uds);

// Global log level for server
extern int server_log_level;

#endif /* PLC_COMM_SERVER_H */
