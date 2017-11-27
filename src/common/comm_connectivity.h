/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */
#ifndef PLC_COMM_CONNECTIVITY_H
#define PLC_COMM_CONNECTIVITY_H

#include <stddef.h>

#include "comm_utils.h"

#define PLC_BUFFER_SIZE 8192
#define PLC_BUFFER_MIN_FREE 200
#define PLC_INPUT_BUFFER 0
#define PLC_OUTPUT_BUFFER 1

typedef struct plcBuffer {
    char *data;
    int   pStart;
    int   pEnd;
    int   bufSize;
} plcBuffer;

#ifndef COMM_STANDALONE
#define MAX_PPLAN 32 /* Max number of pplan saved in one connection. */
struct pplan_slots {
	int64 pplan;
	int next;
};
#endif

typedef struct plcConn {
	int sock;
	int rx_timeout_sec;
	plcBuffer* buffer[2];
#ifndef COMM_STANDALONE
	char *uds_fn; /* File for unix domain socket connection only. */
	int container_slot;
	int head_free_pplan_slot;  /* free list of spi pplan slot */
	struct pplan_slots pplans[MAX_PPLAN]; /* for spi plannning */
#endif
} plcConn;

#define UDS_SHARED_FILE "unix.domain.socket.shared.file"
#define IPC_CLIENT_DIR "/tmp/plcontainer"
#define IPC_GPDB_BASE_DIR "/tmp/plcontainer"
#define MAX_SHARED_FILE_SZ strlen(UDS_SHARED_FILE)

#ifndef COMM_STANDALONE
plcConn * plcConnect_inet(int port);
plcConn * plcConnect_ipc(char *uds_fn);
void plcDisconnect(plcConn *conn);
#endif
plcConn * plcConnInit(int sock);

int plcBufferAppend (plcConn *conn, char *prt, size_t len);
int plcBufferRead (plcConn *conn, char *resBuffer, size_t len);
int plcBufferReceive (plcConn *conn, size_t nBytes);
int plcBufferFlush (plcConn *conn);

#endif /* PLC_COMM_CONNECTIVITY_H */
