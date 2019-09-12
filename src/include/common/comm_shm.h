/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2019-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */


#ifndef _CO_SHM_H
#define _CO_SHM_H

#include "storage/dsm.h"
#include "storage/shm_mq.h"
#include "storage/spin.h"

#define CO_SHM_KEY  "plcoordinator_shm"

/**
 * state: uninitialized, ready, exiting
 * protocol: CO_PROTO_TCP, CO_PROTO_UNIX, CO_PROTO_UNIXPACKET
 * address: TCP(host:port), UNIX(/path/to/file.socket)
 */
typedef enum CoordinatorState {
    CO_STATE_UNINITIALIZED = 1,
    CO_STATE_READY,
    CO_STATE_EXITING,
	CO_STATE_BUFFER_INITIALIZED,
	CO_STATE_BUFFER_ATTACHED,
} CoordinatorState;

typedef enum CoordinatorProtocol {
    CO_PROTO_TCP = 1,
    CO_PROTO_UNIX,
    CO_PROTO_PROTO_UNIXPACKET,
} CoordinatorProtocol;

typedef struct CoordinatorStruct
{
    volatile CoordinatorState state;
    CoordinatorProtocol protocol;
    char address[504];
} CoordinatorStruct;

typedef struct requester_info_entry
{
    int id; // pid of QE
    int sock; // socket file descriptor between QE and coordinator
} requester_info_entry;

typedef enum QeRequestType
{
	CREATE_SERVER_DEBUG = 1,
	DESTROY_SERVER_DEBUG = 2,
	DESTROY_SERVER_DOCKER = 3,
	UNKNOWN_REQUEST = -99,
} QeRequestType;

typedef enum ContainerType
{
	STAND_ALONE = 1, /* Stand alone process */
	DOCKER = 2, /* Docker container */
} ContainerType;

typedef struct QeServerInfoKey
{
	pid_t pid;   /* QE PID */
	int conn; /* gp_session_id */
	int segid; /* segment id */
} QeServerInfoKey;

typedef struct QeServerInfoEntry
{
	QeServerInfoKey key; /* The key of hash table */
	ContainerType type; /* Type of container */
	char containerId[16];
	/* TODO: to extend for monitor */
} QeServerInfoEntry;

/* Message queue */

typedef struct QeRequest
{
	pid_t pid; /* QE PID */
	int conn; /* gp_session_id */
	QeRequestType requestType; /* type of request from QE */
	char containerId[16]; /* container id (pid of stand alone, for debug mode only)  */
} QeRequest;

typedef struct ShmqBufferStatus
{
	slock_t mutex;
	CoordinatorState state;
} ShmqBufferStatus;


#define	PLC_COORDINATOR_MAGIC_NUMBER 0x666fff88
#define SHMQ_BUFFER_BLOCK_NUMBER 10000

/* For debug use only */
extern bool plcontainer_debug_mode;
extern char *plcontainer_debug_server_path;

#define DEBUG_UDS_PREFIX "/tmp/plconatiner.debug"

#endif /* _CO_SHM_H */
