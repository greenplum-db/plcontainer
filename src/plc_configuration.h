/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */


#ifndef PLC_CONFIGURATION_H
#define PLC_CONFIGURATION_H

#include "fmgr.h"
#include <json-c/json.h>
#include "plcontainer.h"

#define PLC_PROPERTIES_FILE "plcontainer_configuration.xml"
#define SHARED_DIR_MAX_LENGTH 512
#define RUNTIME_ID_MAX_LENGTH 512
#define IMAGE_NAME_MAX_LENGTH 512
#define COMMAND_MAX_LENGTH 512
#define MAX_SHARED_DIR_NUM 16
#define MAX_EXPECTED_RUNTIME_NUM 32

typedef enum {
	PLC_ACCESS_READONLY = 0,
	PLC_ACCESS_READWRITE = 1
} plcFsAccessMode;

typedef enum {
	PLC_INSPECT_STATUS = 0,
	PLC_INSPECT_PORT = 1,
	PLC_INSPECT_NAME = 2,
	PLC_INSPECT_PORT_UNKNOWN,
} plcInspectionMode;

typedef struct plcSharedDir {
	char *host;
	char *container;
	plcFsAccessMode mode;
} plcSharedDir;

typedef struct runtimeConfEntry {
	char runtimeid[RUNTIME_ID_MAX_LENGTH];
	char *image;
	char *command;
	int memoryMb;
	int nSharedDirs;
	plcSharedDir* sharedDirs;
	bool isNetworkConnection;
	bool enable_log;
} runtimeConfEntry;

/* entrypoint for all plcontainer procedures */
Datum refresh_plcontainer_config(PG_FUNCTION_ARGS);

Datum show_plcontainer_config(PG_FUNCTION_ARGS);

Datum containers_summary(PG_FUNCTION_ARGS);

runtimeConfEntry *plc_get_runtime_configuration(char *id);

char *get_sharing_options(runtimeConfEntry *conf, int container_slot, bool *has_error, char **uds_dir);

#endif /* PLC_CONFIGURATION_H */
