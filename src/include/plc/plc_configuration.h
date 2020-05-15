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
#include "plc/runtime_config.h"
#define PLC_PROPERTIES_FILE "plcontainer_configuration.xml"

#define MAX_EXPECTED_RUNTIME_NUM 32
#define RES_GROUP_PATH_MAX_LENGTH 256



/* entrypoint for all plcontainer procedures */
Datum refresh_plcontainer_config(PG_FUNCTION_ARGS);

Datum show_plcontainer_config(PG_FUNCTION_ARGS);

Datum containers_summary(PG_FUNCTION_ARGS);
runtimeConfEntry *plc_get_runtime_configuration(const char *id);

HTAB *load_runtime_configuration(void);

bool plc_check_user_privilege(char *users);

int plc_refresh_container_config(bool verbose);

char *get_sharing_options(runtimeConfEntry *conf, bool *has_error, char **uds_dir);

extern HTAB *runtime_conf_table;
typedef struct {
    char *idStr;
    char *statusStr;
    char *ownerStr;
    char *dbidStr;
    int64_t memUsage;
} containerStatus;
#endif /* PLC_CONFIGURATION_H */
