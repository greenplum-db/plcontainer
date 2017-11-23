/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#ifndef PLC_DOCKER_COMMON_H
#define PLC_DOCKER_COMMON_H

#include "postgres.h"
#include <json-c/json.h>
#include "plc_backend_api.h"

int docker_inspect_string(char *buf, char **element, plcInspectionMode type);

#endif /* PLC_DOCKER_COMMON_H */
