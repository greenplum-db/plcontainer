/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */


#ifndef PLC_DOCKER_API_H
#define PLC_DOCKER_API_H

#include "plc_configuration.h"

typedef enum {
    PLC_CALL_HTTPGET = 0,
    PLC_CALL_POST,
    PLC_CALL_DELETE
} plcCurlCallType;

typedef struct {
    char   *data;
    size_t  bufsize;
    size_t  size;
    int     status;
} plcCurlBuffer;

#ifdef CURL_DOCKER_API
    int plc_docker_connect(void);
    int plc_docker_create_container(int sockfd, plcContainerConf *conf, char **name);
    int plc_docker_start_container(int sockfd, char *name);
    int plc_docker_kill_container(int sockfd, char *name);
    int plc_docker_inspect_container(int sockfd, char *name, int *port);
    int plc_docker_wait_container(int sockfd, char *name);
    int plc_docker_delete_container(int sockfd, char *name);
    int plc_docker_disconnect(int sockfd);
#endif

#endif /* PLC_DOCKER_API_H */
