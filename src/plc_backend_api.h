/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#ifndef PLC_BACKEND_API_H
#define PLC_BACKEND_API_H

#include "postgres.h"

#include "plc_configuration.h"

extern char backend_error_message[256];

typedef int (*PLC_FPTR_create)(
		const runtimeConfEntry *conf,            // input the runtime config
		const backendConnectionInfo *backend,    // input the backend connection info
		const int container_slot,                // input the slot id used to generate uds name
		runtimeConnectionInfo *connection        // output the new process connection info
);

typedef int (*PLC_FPTR_start)(
		const backendConnectionInfo *backend,    // input the backend connection info
		runtimeConnectionInfo *connection        // output the new process connection info
);

typedef int (*PLC_FPTR_kill)(
		const backendConnectionInfo *backend,    // input the backend connection info
		const runtimeConnectionInfo *connection  // input the new process connection info
);

typedef int (*PLC_FPTR_inspect)(
		const plcInspectionMode type,            // the inspect method
		const backendConnectionInfo *backend,    // input the backend connection info
		const runtimeConnectionInfo *connection, // input the new process connection info
		char **element                           // the output
);

typedef int (*PLC_FPTR_wait)(
		const backendConnectionInfo *backend,    // input the backend connection info
		const runtimeConnectionInfo *connection  // input the new process connection info
);

typedef int (*PLC_FPTR_delete)(
		const backendConnectionInfo *backend,    // input the backend connection info
		const runtimeConnectionInfo *connection  // input the new process connection info
);

typedef struct PLC_FunctionEntriesData {
	PLC_FPTR_create create_backend;
	PLC_FPTR_start start_backend;
	PLC_FPTR_kill kill_backend;
	PLC_FPTR_inspect inspect_backend;
	PLC_FPTR_wait wait_backend;
	PLC_FPTR_delete delete_backend;
} PLC_FunctionEntriesData;

extern PLC_BACKEND_TYPE CurrentBackendType;

void plc_backend_prepareImplementation(PLC_BACKEND_TYPE imptype);

/* interfaces for plc backend. */

int plc_backend_create(const runtimeConfEntry *conf, const backendConnectionInfo *backend, const int container_slot, runtimeConnectionInfo *connection)  __attribute__((warn_unused_result));

int plc_backend_start(const backendConnectionInfo *backend, runtimeConnectionInfo *connection) __attribute__((warn_unused_result));

int plc_backend_kill(const backendConnectionInfo *backend, const runtimeConnectionInfo *connection) __attribute__((warn_unused_result));

int plc_backend_inspect(const plcInspectionMode type, const backendConnectionInfo *backend, const runtimeConnectionInfo *connection, char **element) __attribute__((warn_unused_result));

int plc_backend_wait(const backendConnectionInfo *backend, const runtimeConnectionInfo *connection) __attribute__((warn_unused_result));

int plc_backend_delete(const backendConnectionInfo *backend, const runtimeConnectionInfo *connection) __attribute__((warn_unused_result));

#endif /* PLC_BACKEND_API_H */
