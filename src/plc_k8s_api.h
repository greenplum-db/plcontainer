/*
 * ------------------------------------------------------------------------------
 *
 * Portions Copyright (c) 2022 - Present VMware, Inc. or its affiliates.
 *
 * ------------------------------------------------------------------------------
 */

#ifndef PLC_K8S_API_H
#define PLC_K8S_API_H

#include "plc_configuration.h"

typedef enum PLC_BACKEND_K8S_NETWORK_MODEL {
	PLC_BACKEND_K8S_NETWORK_CLUSTERIP = 1,
	PLC_BACKEND_K8S_NETWORK_HOSTPORT = 2,
} PLC_BACKEND_K8S_NETWORK_MODEL;

typedef enum PLC_BACKEND_K8S_SETUP_METHOD {
	PLC_BACKEND_K8S_SETUP_CLIENT_IN_CONTAINER = 1,
	PLC_BACKEND_K8S_SETUP_CLIENT_IN_INITCONTAINER = 2,
	PLC_BACKEND_K8S_SETUP_CLIENT_IN_HOSTPATH = 3,
} PLC_BACKEND_K8S_SETUP_METHOD;

int plc_k8s_create_container(
		const runtimeConfEntry *conf,      // input the runtime config
		backendConnectionInfo *backend,    // output the backend connection info
		const int container_slot,          // input the slot id used to generate uds name
		runtimeConnectionInfo *connection  // output the new process connection info
);

int plc_k8s_start_container(
		const backendConnectionInfo *backend,    // input the backend connection info
		runtimeConnectionInfo *connection        // output the new process connection info
);

int plc_k8s_kill_container(
		const backendConnectionInfo *backend,    // input the backend connection info
		const runtimeConnectionInfo *connection  // input the new process connection info
);

int plc_k8s_inspect_container(
		const plcInspectionMode type,            // the inspect method
		const backendConnectionInfo *backend,    // input the backend connection info
		const runtimeConnectionInfo *connection, // input the new process connection info
		char **element                           // the output
);

int plc_k8s_wait_container(
		const backendConnectionInfo *backend,     // input the backend connection info
		const runtimeConnectionInfo *connection   // input the new process connection info
);

int plc_k8s_delete_container(
		const backendConnectionInfo *backend,    // input the backend connection info
		const runtimeConnectionInfo *connection  // input the new process connection info
);

#endif
