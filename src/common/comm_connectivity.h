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

typedef struct plcConn {
	int container_slot;
    int sock;
    plcBuffer* buffer[2];
} plcConn;

#define UDS_SHARED_FILE "unix.domain.socket.shared.file"
#define IPC_CLIENT_DIR "/tmp/plcontainer"
#define IPC_GPDB_BASE_DIR "/tmp/plcontainer"
#define MAX_SHARED_FILE_SZ strlen(UDS_SHARED_FILE)

#ifndef COMM_STANDALONE
void plc_init_ipc(void);
void plc_deinit_ipc(void);
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
