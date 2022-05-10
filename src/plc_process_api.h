/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */


#ifndef PLC_PROCESS_API_H
#define PLC_PROCESS_API_H

#include "plc_configuration.h"

int plc_process_create_container(
		const runtimeConfEntry *conf,            // input the runtime config
		const backendConnectionInfo *backend,    // input the backend connection info
		const int container_slot,                // input the slot id used to generate uds name
		runtimeConnectionInfo *connection        // output the new process connection info
);

int plc_process_start_container(
		const backendConnectionInfo *backend,    // input the backend connection info
		runtimeConnectionInfo *connection        // output the new process connection info
);

int plc_process_kill_container(
		const backendConnectionInfo *backend,    // input the backend connection info
		const runtimeConnectionInfo *connection  // input the new process connection info
);

int plc_process_inspect_container(
		const plcInspectionMode type,            // the inspect method
		const backendConnectionInfo *backend,    // input the backend connection info
		const runtimeConnectionInfo *connection, // input the new process connection info
		char **element                           // the output
);

int plc_process_wait_container(
		const backendConnectionInfo *backend,    // input the backend connection info
		const runtimeConnectionInfo *connection  // input the new process connection info
);

int plc_process_delete_container(
		const backendConnectionInfo *backend,    // input the backend connection info
		const runtimeConnectionInfo *connection  // input the new process connection info
);

#endif /* PLC_PROCESS_API_H */
