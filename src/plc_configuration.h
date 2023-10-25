/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */


#ifndef PLC_CONFIGURATION_H
#define PLC_CONFIGURATION_H

#include "postgres.h"
#include "fmgr.h"

#include "plcontainer.h"

// rename a type name in json-c, to avoid name conflict
#define json_object jsonc_json_object
#include <json-c/json.h>
#undef jsonc_json_object

#define PLC_PROPERTIES_FILE "plcontainer_configuration.xml"
#define RUNTIME_ID_MAX_LENGTH 64
#define MAX_EXPECTED_RUNTIME_NUM 32
#define RES_GROUP_PATH_MAX_LENGTH 256

typedef enum {
	PLC_ACCESS_READONLY = 0,
	PLC_ACCESS_READWRITE = 1
} plcFsAccessMode;

typedef enum {
	PLC_INSPECT_STATUS = 0,
	PLC_INSPECT_PORT = 1,
	PLC_INSPECT_NAME = 2,
	PLC_INSPECT_OOM = 3,
	PLC_INSPECT_PORT_UNKNOWN,
} plcInspectionMode;

typedef struct plcSharedDir {
	char *host;
	char *container;
	plcFsAccessMode mode;
} plcSharedDir;

typedef struct plcDeviceRequest {
	char *driver; // .Driver in Docker API.

	char **deviceid; // eg: ['1', 'UUID=xxxx'], defined by runtime
	int ndeviceid;

	char **capabilities; // eg: ['gpu', 'compute', 'utility']
	int ncapabilities;

	int _count; // field only for docker. fill in when doing serialization
} plcDeviceRequest;

typedef enum PLC_BACKEND_TYPE {
	PLC_BACKEND_UNIMPLEMENT = 0,
	PLC_BACKEND_DOCKER = 1,
	PLC_BACKEND_REMOTE_DOCKER = 2,
	PLC_BACKEND_PROCESS = 3,
} PLC_BACKEND_TYPE;

const char* PLC_BACKEND_TYPE_TO_STRING(PLC_BACKEND_TYPE b);

typedef struct plcBackendLocalDocker {
	char *uds_address;
} plcBackendLocalDocker;

typedef struct plcBackendRemoteDocker {
	char *address;
	int64_t port;
	char *username;
	char *password;
	// true means need append segment index on address
	// eg: foo.local => ${GpIdentity.segindex}.foo.local
	bool add_segment_index;
} plcBackendRemoteDocker;

typedef struct plcBackend {
	char *name;
	PLC_BACKEND_TYPE tag;
	union {
		plcBackendLocalDocker localdocker;
		plcBackendRemoteDocker remotedocker;
	};
} plcBackend;

const plcBackend *take_plcbackend_byname(const char *name);

/*
 * Struct runtimeConfEntry is the entry of hash table.
 * The key of hash table must be the first field of struct.
 */
typedef struct runtimeConfEntry {
	char runtimeid[RUNTIME_ID_MAX_LENGTH];
	char *image;
	char *command;
	char *roles;
	Oid resgroupOid;
	int memoryMb;
	int cpuShare;
	int nSharedDirs;
	const plcBackend *backend; // pointer to session global level runtime_backend_table[]
	plcSharedDir *sharedDirs;
	bool useContainerLogging;
	bool useUserControl;
	bool enableNetwork;
	int ndevicerequests;
	plcDeviceRequest *devicerequests;
	char *client_name;
} runtimeConfEntry;

/* entrypoint for all plcontainer procedures */
Datum refresh_plcontainer_config(PG_FUNCTION_ARGS);

Datum show_plcontainer_config(PG_FUNCTION_ARGS);

runtimeConfEntry *plc_get_runtime_configuration(char *id);

bool plc_check_user_privilege(char *users);

typedef struct backendConnectionInfo {
	PLC_BACKEND_TYPE tag;
	union {
		struct backend_docker {
			char *uds_address;
		} plcBackendLocalDocker;

		struct backend_remote_docker {
			char* hostname;
			uint32_t port;
			char* username;
			char* password;
		} plcBackendRemoteDocker;
	};
} backendConnectionInfo;

backendConnectionInfo *runtime_conf_get_backend_connection_info(const plcBackend *backend);
backendConnectionInfo *runtime_conf_copy_backend_connection_info(const backendConnectionInfo *a);
void runtime_conf_free_backend_connection_info(backendConnectionInfo *info);

typedef enum PLC_RUNTIME_CONNECTION_TYPE {
	PLC_RUNTIME_CONNECTION_UNKNOWN = 0,
	PLC_RUNTIME_CONNECTION_UDS = 1,
	PLC_RUNTIME_CONNECTION_TCP = 2,
} PLC_RUNTIME_CONNECTION_TYPE;

typedef struct runtimeConnectionInfo {
	char *identity; // the connection identity. docker id in docker backend, pid in process backend.

	PLC_RUNTIME_CONNECTION_TYPE tag;
	union {
		struct connection_uds {
			char *uds_address;
		} connection_uds;

		struct connection_tcp {
			char* hostname;
			uint32_t port;
		} connection_tcp;
	};
} runtimeConnectionInfo;

runtimeConnectionInfo *runtime_conf_get_runtime_connection_info(const backendConnectionInfo *backend);
runtimeConnectionInfo* runtime_conf_copy_runtime_connection_info(const runtimeConnectionInfo *a);
void runtime_conf_free_runtime_connection_info(runtimeConnectionInfo *info);

int generate_sharing_options_and_uds_address(const runtimeConfEntry *conf, const backendConnectionInfo *backend,
						const int container_slot, runtimeConnectionInfo *connection,
						char **docekr_sharing_options);

#endif /* PLC_CONFIGURATION_H */
