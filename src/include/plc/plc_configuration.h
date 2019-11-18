/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */


#ifndef PLC_CONFIGURATION_H
#define PLC_CONFIGURATION_H

#include "fmgr.h"
#include "utils/hsearch.h"
#include <json-c/json.h>
#include "plc/plcontainer.h"

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
	plcSharedDir *sharedDirs;
	bool useContainerNetwork;
	bool useContainerLogging;
	bool useUserControl;
} runtimeConfEntry;

/* entrypoint for all plcontainer procedures */
Datum refresh_plcontainer_config(PG_FUNCTION_ARGS);

Datum show_plcontainer_config(PG_FUNCTION_ARGS);

runtimeConfEntry *plc_get_runtime_configuration(const char *id);

HTAB *load_runtime_configuration(void);

bool plc_check_user_privilege(char *users);

int plc_refresh_container_config(bool verbose);

char *get_sharing_options(runtimeConfEntry *conf, bool *has_error, char **uds_dir);

extern HTAB *runtime_conf_table;

#endif /* PLC_CONFIGURATION_H */
