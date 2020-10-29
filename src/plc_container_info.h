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

Datum containers_summary(PG_FUNCTION_ARGS);
Datum list_running_containers(PG_FUNCTION_ARGS);

#endif /* PLC_CONTAINER_INFO_H */