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

//#define CONTAINER_DEBUG
#define CONTAINER_CONNECT_TIMEOUT_MS 5000

/* currently the declaration format for the container in function is:
 * #container:name
 */
#define DECLARATION_MIN_LENGTH (signed)(strlen("#container:") + 1)

/* given source code of the function, extract the container name */
char *parse_container_meta(const char *source);

/* return the port of a started container, -1 if the container isn't started */
plcConn *find_container(const char *image);

/* start a new docker container using the given image  */
plcConn *start_backend(plcContainerConf *conf);

/* Function deletes all the containers */
void delete_containers(void);

#endif /* PLC_CONTAINERS_H */
