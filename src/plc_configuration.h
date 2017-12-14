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

typedef struct plcContainerConf {
	char *id;
	char *image;
	char *command;
	int memoryMb;
	int nSharedDirs;
	plcSharedDir *sharedDirs;
	bool isNetworkConnection;
	bool enable_log;
} plcContainerConf;

/* entrypoint for all plcontainer procedures */
Datum refresh_plcontainer_config(PG_FUNCTION_ARGS);

Datum show_plcontainer_config(PG_FUNCTION_ARGS);

Datum containers_summary(PG_FUNCTION_ARGS);

plcContainerConf *plc_get_container_config(char *id);

char *get_sharing_options(plcContainerConf *conf, int container_slot, bool *has_error, char **uds_dir);

#endif /* PLC_CONFIGURATION_H */
