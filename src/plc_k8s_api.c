/*
 * ------------------------------------------------------------------------------
 *
 * Portions Copyright (c) 2022 - Present VMware, Inc. or its affiliates.
 *
 * ------------------------------------------------------------------------------
 */

#include "plc_k8s_api.h"

#include "cdb/cdbvars.h"
#include "postgres.h"
#include "miscadmin.h"
#include "lib/stringinfo.h"
#include "libpq/libpq-be.h"
#include "common/comm_utils.h"
#include "utils/elog.h"
#include "utils/palloc.h"

#include "plc_configuration.h"
#include "plc_backend_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int plc_k8s_create_container(
		const runtimeConfEntry *conf,            // input the runtime config
		backendConnectionInfo *backend,    // output the backend connection info
		const int container_slot,                // input the slot id used to generate uds name
		runtimeConnectionInfo *connection        // output the new process connection info
) {
	(void)container_slot;

	StringInfoData container_spec = {};
	initStringInfoOfSize(&container_spec, 1024);

	// example in ./k8s/config/samples/plcontainer_v1_plcontainerbackend.yaml
	appendStringInfo(&container_spec, "apiVersion: plcontainer.greenplum.org/v1\n");
	appendStringInfo(&container_spec, "kind: PLContainerBackend\n");
	appendStringInfo(&container_spec, "metadata:\n");
	appendStringInfo(&container_spec, "  generateName: plcontainerbackend-\n");
	appendStringInfo(&container_spec, "  labels:\n");
	appendStringInfo(&container_spec, "    app.kubernetes.io/name: plcontainerbackend\n");
	appendStringInfo(&container_spec, "    app.kubernetes.io/instance: plcontainerbackend\n");
	appendStringInfo(&container_spec, "    app.kubernetes.io/part-of: plcontainer\n");
	appendStringInfo(&container_spec, "    app.kubernetes.io/managed-by: plcontainer\n");
	appendStringInfo(&container_spec, "    app.kubernetes.io/created-by: plcontainer\n");
	appendStringInfo(&container_spec, "spec:\n");
	appendStringInfo(&container_spec, "  instanceid: \"%s\"\n", "TODO");
	switch (conf->backend->k8s.setupmethod) {
		case PLC_BACKEND_K8S_SETUP_CLIENT_IN_HOSTPATH:
			appendStringInfo(&container_spec, "  setupmethod: Hostpath\n");
			break;
		case PLC_BACKEND_K8S_SETUP_CLIENT_IN_CONTAINER:
			appendStringInfo(&container_spec, "  setupmethod: ClientInContainer\n");
			break;
		case PLC_BACKEND_K8S_SETUP_CLIENT_IN_INITCONTAINER:
			appendStringInfo(&container_spec, "  setupmethod: ClientInInitContainer\n");
			break;
	}
	switch (conf->backend->k8s.network) {
		case PLC_BACKEND_K8S_NETWORK_CLUSTERIP:
			appendStringInfo(&container_spec, "  networkmode: ClusterIP\n");
			break;
		case PLC_BACKEND_K8S_NETWORK_HOSTPORT:
			appendStringInfo(&container_spec, "  networkmode: HostPort\n");
			break;
	}
	appendStringInfo(&container_spec, "  portrange:\n");
	appendStringInfo(&container_spec, "    min: %d\n", conf->backend->k8s.portrange_min);
	appendStringInfo(&container_spec, "    max: %d\n", conf->backend->k8s.portrange_max);
	appendStringInfo(&container_spec, "  command: \"%s\"\n", conf->command);
	appendStringInfo(&container_spec, "  args: []\n"); // args is always empty
	appendStringInfo(&container_spec, "  uid: %d\n", getuid());
	appendStringInfo(&container_spec, "  gid: %d\n", getgid());
	appendStringInfo(&container_spec, "  username: \"%s\"\n", GetUserNameFromId(GetUserId(), false));
	appendStringInfo(&container_spec, "  databasename: \"%s\"\n", MyProcPort->database_name);
	appendStringInfo(&container_spec, "  qepid: %d\n", MyProcPid);
	appendStringInfo(&container_spec, "  dbid: %d\n", MyDatabaseId);
	appendStringInfo(&container_spec, "  segindex: %d\n", GpIdentity.segindex);
	appendStringInfo(&container_spec, "  image: \"%s\"\n", conf->image);
	appendStringInfo(&container_spec, "  cpushare: %d\n", conf->cpuShare);
	appendStringInfo(&container_spec, "  memorymb: %d\n", conf->memoryMb);
	if (conf->nSharedDirs == 0) {
		appendStringInfo(&container_spec, "  hostvolume: []\n");
	}
	if (conf->nSharedDirs != 0) {
		appendStringInfo(&container_spec, "  hostvolume:\n");
		for (int i = 0; i < conf->nSharedDirs; i++) {
			const plcSharedDir *dir = &conf->sharedDirs[i];
			switch (dir->mode) {
				case PLC_ACCESS_READONLY:
					appendStringInfo(&container_spec, "    - \"%s:%s:ro\"\n", dir->host, dir->container);
					break;
				case PLC_ACCESS_READWRITE:
					appendStringInfo(&container_spec, "    - \"%s:%s:rw\"\n", dir->host, dir->container);
					break;
			}
		}
	}

	connection->tag = PLC_RUNTIME_CONNECTION_TCP;
	backend->tag = PLC_BACKEND_K8S;
	backend->plcBackendK8s.name = palloc0(64);

	StringInfoData kubectl_command = {};
	initStringInfoOfSize(&kubectl_command, 128);
	appendStringInfo(&kubectl_command, "%s create -f - -o=go-template --template='{{.metadata.name}}'",
			conf->backend->k8s.kubectl_path == NULL ? "kubectl" : conf->backend->k8s.kubectl_path);

	FILE *kubectl_io = popen(kubectl_command.data, "rwe");
	fwrite(container_spec.data, container_spec.len, 1, kubectl_io);
	fread(&backend->plcBackendK8s.name, 64, 1, kubectl_io);

	pfree(container_spec.data);
	pfree(kubectl_command.data);

	if(pclose(kubectl_io) != 0) {
		backend_log(WARNING, "failed to run kubectl create.");
		snprintf(backend_error_message, sizeof(backend_error_message), "failed to run kubectl create.");
		return -1;
	}

	return 0;
}

int plc_k8s_wait_container(
		const backendConnectionInfo *backend, // input the backend connection info
		runtimeConnectionInfo *connection     // output the new process connection info
) {
	StringInfoData kubectl_command = {};
	initStringInfoOfSize(&kubectl_command, 128);

	// kubectl wait plcontainerbackends.plcontainer.greenplum.org \
	//              plcontainerbackend-<name> \
	//              --for=jsonpath='{.status.status}'=Running \
	//              -o=go-template --template='{{.status.hostip}} {{.status.hostport}}'
	appendStringInfo(&kubectl_command, "%s wait plcontainerbackends.plcontainer.greenplum.org",
			backend->plcBackendK8s.kubectl_path == NULL ? "kubectl" : backend->plcBackendK8s.kubectl_path);
	appendStringInfo(&kubectl_command, " %s", backend->plcBackendK8s.name);
	appendStringInfo(&kubectl_command, " --for=jsonpath='{.status.status}'=Running");
	appendStringInfo(&kubectl_command, " -o=go-template --template='{{.status.hostip}} {{.status.hostport}}'");

	FILE *kubectl_io = popen(kubectl_command.data, "rwe");
	fwrite(kubectl_command.data, kubectl_command.len, 1, kubectl_io);
	fscanf(kubectl_io, "%s %d", connection->connection_tcp.hostname, &connection->connection_tcp.port);

	pfree(kubectl_command.data);

	if(pclose(kubectl_io) != 0) {
		backend_log(WARNING, "failed to run kubectl wait plcontainerbackends.plcontainer.greenplum.org %s.", backend->plcBackendK8s.name);
		snprintf(backend_error_message, sizeof(backend_error_message),
				"failed to run kubectl wait plcontainerbackends.plcontainer.greenplum.org %s.", backend->plcBackendK8s.name);
		return -1;
	}

	return 0;
}

int plc_k8s_inspect_container(
		const plcInspectionMode type,            // the inspect method
		const backendConnectionInfo *backend,    // input the backend connection info
		const runtimeConnectionInfo *connection, // input the new process connection info
		char **element                           // the output
) {
	(void)type;
	(void)backend;
	(void)connection;
	(void)element;

	// the container will be managed at the k8s side, inspect is not useful at gpdb side
	Assert(!"not implemented");
	return 0;
}

int plc_k8s_start_container(
		const backendConnectionInfo *backend,    // input the backend connection info
		runtimeConnectionInfo *connection        // output the new process connection info
) {
	(void)backend;
	(void)connection;

	// container will be started automatically. nothing to do here
	Assert(!"not implemented");
	return 0;
}

int plc_k8s_kill_container(
		const backendConnectionInfo *backend,    // input the backend connection info
		const runtimeConnectionInfo *connection  // input the new process connection info
) {
	(void)backend;
	(void)connection;

	// the plcontainer operator will kill the container automatically
	Assert(!"not implemented");
	return 0;
}

int plc_k8s_delete_container(
		const backendConnectionInfo *backend,    // input the backend connection info
		const runtimeConnectionInfo *connection  // input the new process connection info
) {
	(void)backend;
	(void)connection;

	// the plcontainer operator will delete the container automatically
	Assert(!"not implemented");
	return 0;
}
