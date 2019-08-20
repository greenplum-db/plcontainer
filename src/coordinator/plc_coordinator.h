/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2019-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#ifndef _CO_COORDINATOR_H
#define _CO_COORDINATOR_H
/**
 * state: uninitialized, ready, exiting
 * protocol: CO_PROTO_TCP, CO_PROTO_UNIX, CO_PROTO_UNIXPACKET
 * address: TCP(host:port), UNIX(/path/to/file.socket)
 */
typedef enum {
    CO_STATE_UNINITIALIZED = 1,
    CO_STATE_READY,
    CO_STATE_EXITING,
} CoordinatorState;

typedef enum {
    CO_PROTO_TCP = 1,
    CO_PROTO_UNIX,
    CO_PROTO_PROTO_UNIXPACKET,
} coordinator_protocol;

typedef struct
{
    volatile CoordinatorState state;
    coordinator_protocol protocol;
    char address[504];
} CoordinatorStruct;
#define CO_SHM_KEY  "plcoordinator_shm"

typedef struct
{
    int id; // pid of QE
    int sock; // socket file descriptor between QE and coordinator
} requester_info_entry;

#endif /* _CO_COORDINATOR_H */
