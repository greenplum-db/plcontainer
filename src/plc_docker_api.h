/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */


#ifndef PLC_DOCKER_API_H
#define PLC_DOCKER_API_H

#include "plc_configuration.h"

#ifndef CURL_DOCKER_API
    int plc_docker_connect(void);
    int plc_docker_create_container(int sockfd, plcContainerConf *conf, char **name, int container_slot);
    int plc_docker_start_container(int sockfd, char *name);
    int plc_docker_kill_container(int sockfd, char *name);
    int plc_docker_inspect_container(int sockfd, char *name, char **element, plcInspectionMode type);
    int plc_docker_wait_container(int sockfd, char *name);
    int plc_docker_delete_container(int sockfd, char *name);
    int plc_docker_disconnect(int sockfd);
#endif

#endif /* PLC_DOCKER_API_H */
