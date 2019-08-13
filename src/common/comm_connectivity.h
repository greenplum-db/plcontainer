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
	int pStart;
	int pEnd;
	int bufSize;
} plcBuffer;

#define MAX_PPLAN 32 /* Max number of pplan saved in one connection. */
struct pplan_slots {
	int64 pplan;
	int next;
};

typedef struct plcConn {
	int sock;
	int rx_timeout_sec;
	plcBuffer buffer[2];
// #ifndef PLC_CLIENT
	// char *uds_fn; /* File for unix domain socket connection only. */
	// int container_slot;
	// int head_free_pplan_slot;  /* free list of spi pplan slot */
	// struct pplan_slots pplans[MAX_PPLAN]; /* for spi plannning */
// #endif
} plcConn;

typedef struct plcContext
{
	plcConn conn;
	char *service_address; /* File for unix domain socket connection only. */
	// int container_slot;
	int head_free_pplan_slot;  /* free list of spi pplan slot */
	struct pplan_slots pplans[MAX_PPLAN]; /* for spi plannning */
} plcContext;


#define UDS_SHARED_FILE "unix.domain.socket.shared.file"
#define IPC_CLIENT_DIR "/tmp/plcontainer"
#define IPC_GPDB_BASE_DIR "/tmp/plcontainer"
#define MAX_SHARED_FILE_SZ strlen(UDS_SHARED_FILE)

// #ifndef PLC_CLIENT
// 
// plcConn initialized with invalid socket fd(connecting to container)
// socket connecting to the coordinator is one-time fd.
// plcConn *plcConnect_inet(int port);
// 
// plcConn *plcConnect_ipc(char *uds_fn);
// 
// void plcDisconnect(plcConn *conn);
// 
// #endif
// return socket file descriptor, or -1 if failed
// network : unix
int plcListenServer(const char *network, const char *service_address);
int plcDialToServer(const char *network, const char *server_address);
// separator initialization of plcConn/plcContext with socket fd
void plcContextInit(plcContext *ctx);
void plcConnInit(plcConn *conn);
void plcDisconnect(plcConn *conn);
void plcFreeContext(plcContext *ctx);
void plcReleaseContext(plcContext *ctx);

// plcConn *plcConnInit(int sock);

int plcBufferAppend(plcConn *conn, char *prt, size_t len);

int plcBufferRead(plcConn *conn, char *resBuffer, size_t len);

int plcBufferReceive(plcConn *conn, size_t nBytes);

int plcBufferFlush(plcConn *conn);

// init plcBuffer to retain a default buffer.
int plcBufferInit(plcBuffer *buffer);

// free the internal buffer in plcBuffer, not plcBuffer itself
// i.e. to make the buffer empty
void plcBufferRelease(plcBuffer *buffer);
// return the available data size.
static inline int plcBufferAvailableSize(const plcBuffer *buffer)
{
	return buffer->pEnd - buffer->pStart;
}

int ListenUnix(const char *network, const char *address);
int ListenTCP(const char *network, const char *address);

#endif /* PLC_COMM_CONNECTIVITY_H */
