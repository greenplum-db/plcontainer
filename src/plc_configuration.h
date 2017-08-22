/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016, Pivotal.
 *
 *------------------------------------------------------------------------------
 */


#ifndef PLC_CONFIGURATION_H
#define PLC_CONFIGURATION_H

#include "fmgr.h"

#include "plcontainer.h"

#define PLC_PROPERTIES_FILE "plcontainer_configuration.xml"

typedef enum {
    PLC_ACCESS_READONLY  = 0,
    PLC_ACCESS_READWRITE = 1
} plcFsAccessMode;

typedef struct plcSharedDir {
    char            *host;
    char            *container;
    plcFsAccessMode  mode;
} plcSharedDir;

typedef struct plcContainerConf {
    char         *name;
    char         *dockerid;
    char         *command;
    int           memoryMb;
    int           nSharedDirs;
    plcSharedDir *sharedDirs;
} plcContainerConf;

/* entrypoint for all plcontainer procedures */
Datum refresh_plcontainer_config(PG_FUNCTION_ARGS);
Datum show_plcontainer_config(PG_FUNCTION_ARGS);
plcContainerConf *plc_get_container_config(char *name);
char *get_sharing_options(plcContainerConf *cont);

#endif /* PLC_CONFIGURATION_H */
