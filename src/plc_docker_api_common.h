/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */


#ifndef PLC_DOCKER_API_H
#define PLC_DOCKER_API_H

#include "plc_configuration.h"
#include "plc_docker_common.h"

#define CURL_BUFFER_SIZE 8192

typedef enum {
	PLC_HTTP_GET = 0,
	PLC_HTTP_POST,
	PLC_HTTP_DELETE
} plcCurlCallType;

typedef struct {
	char *data;
	size_t bufsize;
	size_t size;
	int status;
} plcCurlBuffer;

int plc_docker_create_container(runtimeConfEntry *conf, char **name, int container_slot, char **uds_dir);

int plc_docker_start_container(const char *name);

int plc_docker_kill_container(const char *name);

int plc_docker_inspect_container(const char *name, char **element, plcInspectionMode type);

int plc_docker_wait_container(const char *name);

int plc_docker_delete_container(const char *name);

int plc_docker_list_container(char **result);

int plc_docker_get_container_state(const char *name, char **result);

#endif /* PLC_DOCKER_API_H */
