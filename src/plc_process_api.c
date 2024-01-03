/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#include "plc_process_api.h"

int plc_process_create_container(
		const runtimeConfEntry *conf,            // input the runtime config
		const backendConnectionInfo *backend,    // input the backend connection info
		const int container_slot,                // input the slot id used to generate uds name
		runtimeConnectionInfo *connection        // output the new process connection info
) {
    // no shared volumes should not be treated as an error (TODO why?), so we use has_error to
    // identifier whether there is an error when parse sharing options.
    char *volumeShare;
    bool has_error = generate_sharing_options_and_uds_address(conf, backend, container_slot, connection, &volumeShare);
    if (has_error == true || volumeShare == NULL) {
        return -1;
    }
    pfree(volumeShare);

	/* Identity is unknown since the backend is started manually. */
    connection->identity = palloc(64);
    snprintf(connection->identity, 64, "<unknown>");
    return 0;
}

int plc_process_start_container(
		const backendConnectionInfo *backend,    // input the backend connection info
		runtimeConnectionInfo *connection        // output the new process connection info
) {
	/* Do nothing since the backend is started manually. */
    return 0;
}

int plc_process_kill_container(
		const backendConnectionInfo *backend,    // input the backend connection info
		const runtimeConnectionInfo *connection  // input the new process connection info
) {
	/* Do nothing since the backend is started manually. */
    return 0;
}

int plc_process_inspect_container(
		const plcInspectionMode type,            // the inspect method
		const backendConnectionInfo *backend,    // input the backend connection info
		const runtimeConnectionInfo *connection, // input the new process connection info
		char **element                           // the output
) {
	/* Do nothing since the backend is started manually. */
    return 0;
}

int plc_process_wait_container(
		const backendConnectionInfo *backend,    // input the backend connection info
		const runtimeConnectionInfo *connection  // input the new process connection info
) {
	/* Do nothing since the backend is started manually. */
    return 0;
}

int plc_process_delete_container(
		const backendConnectionInfo *backend,    // input the backend connection info
		const runtimeConnectionInfo *connection  // input the new process connection info
) {
	/* Do nothing since the backend is started manually. */
    return 0;
}
