/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */


#ifndef PLC_DOCKER_API_H
#define PLC_DOCKER_API_H

#include "plc_configuration.h"

#define CURL_BUFFER_SIZE 8192

#define PLC_DOCKER_API_RES_OK 0
#define PLC_DOCKER_API_RES_NOT_FOUND -404

typedef enum {
	PLC_HTTP_GET = 0,
	PLC_HTTP_POST,
	PLC_HTTP_DELETE
} plcCurlCallType;

typedef struct {
	char *data;
	size_t bufsize;
	size_t size;
	int status;
} plcCurlBuffer;

int plc_docker_create_container(
		const runtimeConfEntry *conf,            // input the runtime config
		const backendConnectionInfo *backend,    // input the backend connection info
		const int container_slot,                // input the slot id used to generate uds name
		runtimeConnectionInfo *connection        // output the new process connection info
);

int plc_docker_start_container(
		const backendConnectionInfo *backend,    // input the backend connection info
		runtimeConnectionInfo *connection        // output the new process connection info
);

int plc_docker_kill_container(
		const backendConnectionInfo *backend,    // input the backend connection info
		const runtimeConnectionInfo *connection  // input the new process connection info
);

int plc_docker_inspect_container(
		const plcInspectionMode type,            // the inspect method
		const backendConnectionInfo *backend,    // input the backend connection info
		const runtimeConnectionInfo *connection, // input the new process connection info
		char **element                           // the output
);

int plc_docker_wait_container(
		const backendConnectionInfo *backend,    // input the backend connection info
		const runtimeConnectionInfo *connection  // input the new process connection info
);

int plc_docker_delete_container(
		const backendConnectionInfo *backend,    // input the backend connection info
		const runtimeConnectionInfo *connection  // input the new process connection info
);

/* FIXME: We may want below two functions or their callers to have common intefaces in backend code. */
int plc_docker_list_container(char **result, int dbid, const plcBackend* backend) __attribute__((warn_unused_result));

int plc_docker_get_container_state(char **result, const char *name, const plcBackend* backend_conf) __attribute__((warn_unused_result));

#endif /* PLC_DOCKER_API_H */
