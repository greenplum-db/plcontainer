/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */


#ifndef PLC_CONTAINER_INFO_H
#define PLC_CONTAINER_INFO_H

#include "fmgr.h"
#include <json-c/json.h>
#include "plcontainer.h"


struct UdfContainerIdMap
{
    char container_id[32];
    char udf_name[224];
};
typedef struct UdfContainerIdMap UdfContainerIdMap;

Datum containers_summary(PG_FUNCTION_ARGS);
Datum list_running_containers(PG_FUNCTION_ARGS);

void plcontainer_shmem_startup(void);

void init_plcontainer_shmem(void);

void add_containerid_entry(char *dockerid, char *udf);

void del_containerid_entry(char *dockerid);

#endif /* PLC_CONTAINER_INFO_H */
