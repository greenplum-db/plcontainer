/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2019-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */
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
