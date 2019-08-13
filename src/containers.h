/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#ifndef PLC_CONTAINERS_H
#define PLC_CONTAINERS_H

#include <regex.h>

#include "common/comm_connectivity.h"
#include "plc_configuration.h"

#define CONTAINER_CONNECT_TIMEOUT_MS 10000
#define CONTAINER_ID_MAX_LENGTH 128
/* given source code of the function, extract the container name */
char *parse_container_meta(const char *source);

/* return the port of a started container, -1 if the container isn't started */
plcContext *get_container_context(const char *runtime_id);

/* Function deletes all the containers */
void reset_containers(void);

#endif /* PLC_CONTAINERS_H */
