/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "postgres.h"
#include "utils/ps_status.h"

#include "common/comm_utils.h"
#include "common/comm_channel.h"
#include "common/messages/messages.h"
#include "plc_configuration.h"
#include "containers.h"

#ifdef CURL_DOCKER_API
    #include "plc_docker_curl_api.h"
#else
    #include "plc_docker_api.h"
#endif

typedef struct {
    char    *name;
    char    *dockerid;
    plcConn *conn;
} container_t;

#define CONTAINER_NUMBER 10
static int containers_init = 0;
static container_t *containers;

static void insert_container(char *image, char *dockerid, plcConn *conn);
static void init_containers();
static inline bool is_whitespace (const char c);
static int check_container_name(const char *name);

#ifndef CONTAINER_DEBUG

static void cleanup(char *dockerid) {
    pid_t pid = 0;

    /* We fork the process to syncronously wait for container to exit */
    pid = fork();
    if (pid == 0) {
        char    psname[200];
        int     res;
        int     sockfd;
        int     attempt = 0;
        clock_t start;
        clock_t end;

        /* Setting application name to let the system know it is us */
        sprintf(psname, "plcontainer cleaner %s", dockerid);
        set_ps_display(psname, false);

        /* We make 5 attempts to start waiting for the container */
        for (attempt = 0; attempt < 5; attempt++) {

            /* Connect to the Docker API and execute "wait" command for the
             * target container to wait for its termination */
            start = clock();
            sockfd = plc_docker_connect();
            if (sockfd > 0) {
                res = plc_docker_wait_container(sockfd, dockerid);
                plc_docker_disconnect(sockfd);
            } else {
                res = -1;
            }
            end = clock();

            /* If "wait" has finished successfully - container has finished
             * and we can remove the container */
            if (res == 0) {
                break;
            }

            /* If we "waited" for more than 1 minute and wait failed, this might
             * happen due to network issue and we should reset the attempt
             * counter and start over */
            if ( (end - start) / CLOCKS_PER_SEC > 60 ) {
                attempt = 0;
            }

            /* Sleep for 1 second in case of failed attempt */
            sleep(1);
        }

        /* Connect to the Docker API to remove the container */
        sockfd = plc_docker_connect();
        if (sockfd > 0) {
            res = plc_docker_delete_container(sockfd, dockerid);
            if (res < 0) {
                _exit(1);
            }
        }
        plc_docker_disconnect(sockfd);

        _exit(0);
    }
}

#endif /* not CONTAINER_DEBUG */

static void insert_container(char *image, char *dockerid, plcConn *conn) {
    size_t i;
    for (i = 0; i < CONTAINER_NUMBER; i++) {
        if (containers[i].name == NULL) {
            containers[i].name     = plc_top_strdup(image);
            containers[i].conn     = conn;
            containers[i].dockerid = NULL;
            if (dockerid != NULL) {
                containers[i].dockerid = plc_top_strdup(dockerid);
            }
            return;
        }
    }
    // Fatal would cause the session to be closed
    elog(FATAL, "Single session cannot handle more than %d open containers simultaneously", CONTAINER_NUMBER);
}

static void init_containers() {
    containers = (container_t*)plc_top_alloc(CONTAINER_NUMBER * sizeof(container_t));
    memset(containers, 0, CONTAINER_NUMBER * sizeof(container_t));
    containers_init = 1;
}

plcConn *find_container(const char *image) {
    size_t i;
    if (containers_init == 0)
        init_containers();
    for (i = 0; i < CONTAINER_NUMBER; i++) {
        if (containers[i].name != NULL &&
            strcmp(containers[i].name, image) == 0) {
            return containers[i].conn;
        }
    }

    return NULL;
}

plcConn *start_container(plcContainerConf *conf) {
    int port;
    unsigned int sleepus = 25000;
    unsigned int sleepms = 0;
    plcMsgPing *mping = NULL;
    plcConn *conn = NULL;
    char *dockerid = NULL;

#ifdef CONTAINER_DEBUG

    port = 8080;

#else

    int sockfd;
    int res = 0;

    sockfd = plc_docker_connect();
    if (sockfd < 0) {
        elog(ERROR, "Cannot connect to the Docker API socket");
        return conn;
    }

    res = plc_docker_create_container(sockfd, conf, &dockerid);
    if (res < 0) {
        elog(ERROR, "Cannot create Docker container");
        return conn;
    }

    res = plc_docker_start_container(sockfd, dockerid);
    if (res < 0) {
        elog(ERROR, "Cannot start Docker container");
        return conn;
    }

    res = plc_docker_inspect_container(sockfd, dockerid, &port);
    if (res < 0) {
        elog(ERROR, "Cannot parse host port exposed by Docker container");
        return conn;
    }

    res = plc_docker_disconnect(sockfd);
    if (res < 0) {
        elog(ERROR, "Cannot disconnect from the Docker API socket");
        return conn;
    }

    /* Create a process to clean up the container after it finishes */
    cleanup(dockerid);

#endif // CONTAINER_DEBUG

    /* Making a series of connection attempts unless connection timeout of
     * CONTAINER_CONNECT_TIMEOUT_MS is reached. Exponential backoff for
     * reconnecting first attempts: 25ms, 50ms, 100ms, 200ms, 200ms, etc.
     */
    mping = palloc(sizeof(plcMsgPing));
    mping->msgtype = MT_PING;
    while (sleepms < CONTAINER_CONNECT_TIMEOUT_MS) {
        int         res = 0;
        plcMessage *mresp = NULL;

        conn = plcConnect(port);
        if (conn != NULL) {
            res = plcontainer_channel_send(conn, (plcMessage*)mping);
            if (res == 0) {
                res = plcontainer_channel_receive(conn, &mresp);
                if (mresp != NULL)
                    pfree(mresp);
                if (res == 0)
                    break;
            }
            plcDisconnect(conn);
        }

        usleep(sleepus);
        elog(DEBUG1, "Waiting for %u ms for before reconnecting", sleepus/1000);
        sleepms += sleepus / 1000;
        sleepus = sleepus >= 200000 ? 200000 : sleepus * 2;
    }

    if (sleepms >= CONTAINER_CONNECT_TIMEOUT_MS) {
        elog(ERROR, "Cannot connect to the container, %d ms timeout reached",
                    CONTAINER_CONNECT_TIMEOUT_MS);
        conn = NULL;
    } else {
        insert_container(conf->name, dockerid, conn);
    }

    pfree(dockerid);

    return conn;
}

void stop_containers() {
    size_t i;

    if (containers_init != 0) {
        for (i = 0; i < CONTAINER_NUMBER; i++) {
            if (containers[i].name != NULL) {

                /* Terminate connection to the container */
                if (containers[i].conn != NULL) {
                    plcDisconnect(containers[i].conn);
                }

                /* Terminate container process */
                if (containers[i].dockerid != NULL) {
                    int sockfd;

                    sockfd = plc_docker_connect();
                    if (sockfd > 0) {
                        plc_docker_kill_container(sockfd, containers[i].dockerid);
                        plc_docker_disconnect(sockfd);
                    }
                    pfree(containers[i].dockerid);
                }

                /* Set all fields to NULL as part of cleanup */
                pfree(containers[i].name);
                containers[i].name = NULL;
                containers[i].dockerid = NULL;
                containers[i].conn = NULL;
            }
        }
    }
    containers_init = 0;
}

static inline bool is_whitespace (const char c) {
    return (c == ' ' || c == '\n' || c == '\t' || c == '\r');
}

char *parse_container_meta(const char *source) {
    int first, last, len;
    char *name = NULL;

    first = 0;
    len = strlen(source);

    /* Ignore whitespaces in the beginning of the function declaration */
    while (first < len && is_whitespace(source[first]))
        first++;

    /* Read everything up to the first newline or end of string */
    last = first;
    while (last < len && source[last] != '\n' && source[last] != '\r')
        last++;

    /* If the string is too small or not starting with hash - no declaration */
    if (last - first < DECLARATION_MIN_LENGTH || source[first] != '#') {
        lprintf(ERROR, "Container declaration format should be '#container:container_name'");
        return name;
    }

    first++;
    /* Ignore whitespaces after the hash sign */
    while (first < len && is_whitespace(source[first]))
        first++;

    /* Line should be "# container :", fail if not so */
    if (strncmp(&source[first], "container", strlen("container")) != 0) {
        lprintf(ERROR, "Container declaration format should be '#container:container_name'");
        return name;
    }

    /* Follow the line up to colon sign */
    while (first < last && source[first] != ':')
        first++;
    first++;

    /* If no colon found - bad declaration */
    if (first >= last) {
        lprintf(ERROR, "Container declaration format should be '#container:container_name'");
        return name;
    }

    /* Ignore whitespace after colon sign */
    while (first < last && is_whitespace(source[first]))
        first++;
    /* Ignore whitespace in the end of the line */
    while (last > first && is_whitespace(source[last]))
        last--;
    /* when first meets last, the name is blankspace or only one char*/
    if (first == last && is_whitespace(source[first])){
        lprintf(ERROR, "Container name cannot be empty");
        return NULL;
    }
    /*
     * Allocate container name variable and copy container name
     * the character length of name is last-first+1
     * +1 for terminator
     */
    name = (char*)pmalloc(last - first + 1 + 1);
    memcpy(name, &source[first], last - first + 1);

    name[last - first + 1] = '\0';

    int regt = check_container_name(name);
    if (regt == -1) {
        lprintf(ERROR, "Container name '%s' contains illegal character for container.", name);
    }

    return name;
}

/*
 * check whether container name specified in function declaration
 * satisfy the regex which follow docker container/image naming conventions.
 */
static int check_container_name(const char *name){
    int    status;
    regex_t    re;
    if (regcomp(&re, "^[a-zA-Z0-9][a-zA-Z0-9_.-]*$", REG_EXTENDED | REG_NOSUB | REG_NEWLINE) != 0) {
        return -1;
    }
    status = regexec(&re, name, (size_t) 0, NULL, 0);
    regfree(&re);
    if (status != 0) {
        return -1;
    }
    return 0;
}
