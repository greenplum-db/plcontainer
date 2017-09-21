/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#ifndef PLC_BACKEND_API_H
#define PLC_BACKEND_API_H

#include "postgres.h"

#include "plc_configuration.h"
#include "plc_docker_api_common.h"

extern char api_error_message[256];

typedef int ( * PLC_FPTR_create)     (plcContainerConf *conf, char **name, int container_slot);
typedef int ( * PLC_FPTR_start)      (char *name);
typedef int ( * PLC_FPTR_kill)       (char *name);
typedef int ( * PLC_FPTR_inspect)    (char *name, char **element, plcInspectionMode type);
typedef int ( * PLC_FPTR_wait)       (char *name);
typedef int ( * PLC_FPTR_delete)     (char *name);

struct PLC_FunctionEntriesData{
    PLC_FPTR_create      create;
    PLC_FPTR_start       start;
    PLC_FPTR_kill        kill;
    PLC_FPTR_inspect     inspect;
    PLC_FPTR_wait        wait;
    PLC_FPTR_delete      delete_backend;
};

typedef struct PLC_FunctionEntriesData  PLC_FunctionEntriesData;
typedef struct PLC_FunctionEntriesData *PLC_FunctionEntries;

enum PLC_BACKEND_TYPE {
    BACKEND_DOCKER = 0,
    BACKEND_GARDEN,   /* not implemented yet*/
    BACKEND_PROCESS,  /* not implemented yet*/
    UNIMPLEMENT_TYPE
};

void plc_backend_prepareImplementation(enum PLC_BACKEND_TYPE imptype);

/* interface for plc backend*/
int plc_backend_create(plcContainerConf *conf, char **name, int container_slot);
int plc_backend_start(char *name);
int plc_backend_kill(char *name);
int plc_backend_inspect(char *name, char **element, plcInspectionMode type);
int plc_backend_wait(char *name);
int plc_backend_delete(char *name);

#endif /* PLC_BACKEND_API_H */
