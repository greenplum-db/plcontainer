/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */


#include "postgres.h"
#include "utils/guc.h"
#include "libpq/libpq.h"
#include "miscadmin.h"
#include "libpq/libpq-be.h"
#include "common/comm_utils.h"
#include "plc_process_api.h"
#include "plc_backend_api.h"
#ifndef PLC_PG
  #include "cdb/cdbvars.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <sys/wait.h>

int plc_process_create_container(
		const runtimeConfEntry *conf,            // input the runtime config
		const backendConnectionInfo *backend,    // input the backend connection info
		const int container_slot,                // input the slot id used to generate uds name
		runtimeConnectionInfo *connection        // output the new process connection info
) {
    // no shared volumes should not be treated as an error (TODO why?), so we use has_error to
    // identifier whether there is an error when parse sharing options.
    char *volumeShare;
    bool has_error = generate_sharing_options_and_uds_address(conf, backend, container_slot, connection, &volumeShare);
    if (has_error == true || volumeShare == NULL) {
        return -1;
    }
    pfree(volumeShare);

    pid_t pid = -1;
    int res = 0;
    if ((pid = fork()) == -1) {
        res = -1;
    } else if (pid == 0) {
        char *env_str;
        char binaryPath[1024] = {0};
        if ((env_str = getenv("GPHOME")) == NULL) {
            plc_elog (ERROR, "GPHOME is not set");
        } else {
            if (strstr(conf->command, "pyclient") != NULL) {
                sprintf(binaryPath, "%s/bin/plcontainer_clients/pyclient", env_str);
            } else {
                sprintf(binaryPath, "%s/bin/plcontainer_clients/rclient", env_str);
            }
        }
        char uid_string[1024] = {0};
        sprintf(uid_string, "EXECUTOR_UID=%d", getuid());
        char gid_string[1024] = {0};
        sprintf(gid_string, "EXECUTOR_GID=%d", getgid());
        char plc_client[1024] = {0};
        sprintf(plc_client, "PLC_CLIENT=%s", conf->client_name);
        // TODO add more environment variables needed.
        char *const env[] = {
            "USE_CONTAINER_NETWORK=false",
            "LOCAL_PROCESS_MODE=1",
            uid_string,
            gid_string,
            plc_client,
            NULL
        };

        execle(binaryPath, binaryPath, NULL, env);
        exit(EXIT_FAILURE);
    }

    // parent, continue......
    connection->identity = palloc(64);
    snprintf(connection->identity, 64, "%d", pid);

    backend_log(LOG, "create backend process with name:%s", connection->identity);
    return res;
}

int plc_process_start_container(
		const backendConnectionInfo *backend,    // input the backend connection info
		runtimeConnectionInfo *connection        // output the new process connection info
) {
	(void)backend;

    int res = 0;
    backend_log(LOG, "start backend process with name:%s", connection->identity);
    return res;
}

int plc_process_kill_container(
		const backendConnectionInfo *backend,    // input the backend connection info
		const runtimeConnectionInfo *connection  // input the new process connection info
) {
	(void)backend;

    int res = 0;
    int pid = atoi(connection->identity);
	Assert(pid != 0);

    kill(pid, SIGKILL);
    backend_log(LOG, "kill backend process with name:%d", pid);
    return res;
}

int plc_process_inspect_container(
		const plcInspectionMode type,            // the inspect method
		const backendConnectionInfo *backend,    // input the backend connection info
		const runtimeConnectionInfo *connection, // input the new process connection info
		char **element                           // the output
) {
	(void)backend;

    int res = 0;
    *element = palloc(64);
    sprintf(*element, "process:%s type:%d", connection->identity, type);
    backend_log(LOG, "inspect backend process with name:%s", connection->identity);
    return res;
}

int plc_process_wait_container(
		const backendConnectionInfo *backend,    // input the backend connection info
		const runtimeConnectionInfo *connection  // input the new process connection info
) {
	(void)backend;

    int res = 0;
    int pid = atoi(connection->identity);
    waitpid(pid, &res, 0);
    backend_log(LOG, "wait backend process with name:%d", pid);
    return res;
}

int plc_process_delete_container(
		const backendConnectionInfo *backend,    // input the backend connection info
		const runtimeConnectionInfo *connection  // input the new process connection info
) {
	(void)backend;

    int res = 0;
    int pid = atoi(connection->identity);
	Assert(pid != 0);

    kill(pid, SIGKILL);
    backend_log(LOG, "delete backend process with name:%d", pid);
    return res;
}
