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

int plc_process_create_container(runtimeConfEntry *conf, char **name, int container_id, char **uds_dir) {
    (void)(name);
    bool has_error;
    char *volumeShare = get_sharing_options(conf, container_id, &has_error, uds_dir);
    (void)(volumeShare);
/*
 *  no shared volumes should not be treated as an error, so we use has_error to
 *  identifier whether there is an error when parse sharing options.
 */
    if (has_error == true) {
        return -1;
    }

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
        char gid_string[1024] = {0};
        sprintf(uid_string, "EXECUTOR_UID=%d", getuid());
        sprintf(gid_string, "EXECUTOR_GID=%d", getgid());
        // TODO add more environment variables needed.
        char *const env[] = {
            "USE_CONTAINER_NETWORK=false",
            "LOCAL_PROCESS_MODE=1",
            uid_string,
            gid_string,
            NULL
        };

        execle(binaryPath, binaryPath, NULL, env);
        exit(EXIT_FAILURE);
    }

    // parent, continue......
    *name = palloc(64);
    sprintf(*name, "%d", pid);

    backend_log(LOG, "create backend process with name:%s", *name);
    return res;
}

int plc_process_start_container(const char *name) {
    int res = 0;
    backend_log(LOG, "start backend process with name:%s", name);
    return res;
}

int plc_process_kill_container(const char *name) {
    int res = 0;
    int pid = atoi(name);
    kill(pid, SIGKILL);
    backend_log(LOG, "kill backend process with name:%s", name);
    return res;
}

int plc_process_inspect_container(const char *name, char **element, plcInspectionMode type) {
    int res = 0;
    *element = palloc(64);
    sprintf(*element, "process:%s type:%d", name, type);
    backend_log(LOG, "inspect backend process with name:%s", name);
    return res;
}

int plc_process_wait_container(const char *name) {
    int res = 0;
    int pid = atoi(name);
    waitpid(pid, &res, 0);
    backend_log(LOG, "wait backend process with name:%s", name);
    return res;
}

int plc_process_delete_container(const char *name) {
    int res = 0;
    int pid = atoi(name);
    kill(pid, SIGKILL);
    backend_log(LOG, "delete backend process with name:%s", name);
    return res;
}
