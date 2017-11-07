/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
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
#include <libgen.h>
#include <sys/wait.h>

#include "postgres.h"
#include "storage/ipc.h"
#include "libpq/pqsignal.h"
#include "utils/ps_status.h"
#include "utils/faultinjector.h"

#include "common/comm_utils.h"
#include "common/comm_channel.h"
#include "common/comm_connectivity.h"
#include "common/messages/messages.h"
#include "plc_configuration.h"
#include "containers.h"

#include "plc_backend_api.h"

typedef struct {
    char    *name;
    char    *dockerid;
    plcConn *conn;
} container_t;

#define CONTAINER_NUMBER 10
#define CONATINER_WAIT_TIMEOUT 2
#define CONATINER_CONNECT_TIMEOUT 120

static int containers_init = 0;
static container_t *containers;
static char *uds_fn_for_cleanup;

static void init_containers();
static inline bool is_whitespace (const char c);
static int check_container_name(const char *name);

#ifndef CONTAINER_DEBUG

static int qe_is_alive(char *dockerid) {

	/*
	 * default return code to tell cleanup process that everything
	 * is normal
	 */
    int return_code = 1;

    if (getppid() == 1){
        /* backend exited, call docker delete API */
        return_code = plc_backend_delete(dockerid);
    }
    return return_code;
}

static int container_is_alive(char *dockerid) {
    char *element = NULL;
    
	int return_code = 1;

    int res;
    res = plc_backend_inspect(dockerid, &element, PLC_INSPECT_STATUS);
    if (res < 0) {
        return_code = res;
    } else if (element != NULL) {
        if ((strcmp("exited", element) == 0 ||
             strcmp("false", element) == 0 )) {
            return_code = plc_backend_delete(dockerid);
        } else if (strcmp("unexist", element) == 0) {
            return_code = 0;
        }
    }

    return return_code;

}

static void cleanup4uds(char *uds_fn) {
	if (uds_fn != NULL) {
		unlink(uds_fn);
		rmdir(dirname(uds_fn));
	}
}

static void cleanup_atexit_callback() {
	cleanup4uds(uds_fn_for_cleanup);
	free(uds_fn_for_cleanup);
	uds_fn_for_cleanup = NULL;
}

static void cleanup(char *dockerid, char *uds_fn) {
    pid_t pid = 0;

    /* We fork the process to synchronously wait for backend to exit */
    pid = fork();
    if (pid == 0) {
        char    psname[200];
        int     res;
		int     wait_time = 0;

		/* We do not need proc_exit() callbacks of QE. Besides, we
		 * do not use on_proc_exit() + proc_exit() since it may invovle
		 * some QE related operations, * e.g. Quit interconnect, etc, which
		 * might finally damage QE. Instead we register our own onexit
		 * callback functions and invoke them via exit().
		 * Finally we might better invoke a pg/gp independent program
		 * to manage the lifecycles of backends.
		 */
		on_exit_reset();
		uds_fn_for_cleanup = strdup(uds_fn);
#ifdef HAVE_ATEXIT
		atexit(cleanup_atexit_callback);
#else
		on_exit(cleanup_atexit_callback, NUL);
#endif

		pqsignal(SIGHUP, SIG_IGN);
		pqsignal(SIGINT, SIG_IGN);
		pqsignal(SIGTERM, SIG_IGN);
		pqsignal(SIGQUIT, SIG_IGN);
		pqsignal(SIGALRM, SIG_IGN);
		pqsignal(SIGPIPE, SIG_IGN);
		pqsignal(SIGUSR1, SIG_IGN);
		pqsignal(SIGUSR2, SIG_IGN);
		pqsignal(SIGCHLD, SIG_IGN);
		pqsignal(SIGCONT, SIG_IGN);

        /* Setting application name to let the system know it is us */
        sprintf(psname, "plcontainer cleaner %s", dockerid);
        set_ps_display(psname, false);

		res = 0;
		PG_TRY();
		{
			while(1) {

				/* Check parent pid whether parent process is alive or not.
				 * If not, kill and remove the container.
				 */
				elog(DEBUG1, "Checking whether QE is alive");
				res = qe_is_alive(dockerid);
				elog(DEBUG1, "QE alive status: %d", res);

				/* res = 0, backend exited, backend has been successfully deleted.
				 * res < 0, backend exited, backend delete API reports an error.
				 * res > 0, backend still alive, check container status.
				 */
				if (res == 0) {
					break;
				} else if (res < 0) {
					wait_time += CONATINER_WAIT_TIMEOUT;
					elog(LOG, "Failed to delete backend in cleanup process (%s). "
						"Will retry later.", api_error_message);
				} else {
					wait_time = 0;
				}

				/* Check whether conatiner is exited or not.
				 * If exited, remove the container.
				 */
				elog(DEBUG1, "Checking whether the backend is alive");
				res = container_is_alive(dockerid);
				elog(DEBUG1, "Backend alive status: %d", res);

				/* res = 0, container exited, container has been successfully deleted.
				 * res < 0, docker API reports an error.
				 * res > 0, container still alive, sleep and check again.
				 */
				if (res == 0) {
					break;
				} else if (res < 0) {
					wait_time += CONATINER_WAIT_TIMEOUT;
					elog(LOG, "Failed to inspect or delete backend in cleanup process (%s). "
						"Will retry later.", api_error_message);
				} else {
					wait_time = 0;
				}

				if (wait_time >= CONATINER_CONNECT_TIMEOUT) {
					 elog(LOG, "Docker API fails for %d seconds. cleanup "
						"process will exit.", wait_time);
					 break;
				}

				/* Check every CONATINER_WAIT_TIMEOUT*/
				sleep(CONATINER_WAIT_TIMEOUT);
			}

			exit(res);
		}
		PG_CATCH();
		{
			/* Do not rethrow to previous stack context. exit immediately.*/
			elog(LOG, "cleanup process should not reach here. Anyway it should not hurt. Exiting.");
			exit(-1);
		}
		PG_END_TRY();
    } else if (pid < 0) {
        elog(ERROR, "Could not create cleanup process for container %s", dockerid);
    }
}

#endif /* not CONTAINER_DEBUG */

static int find_container_slot() {
	int i;

    for (i = 0; i < CONTAINER_NUMBER; i++) {
        if (containers[i].name == NULL) {
            return i;
        }
    }
    // Fatal would cause the session to be closed
    elog(FATAL, "Single session cannot handle more than %d open containers simultaneously", CONTAINER_NUMBER);

}

static void set_container_conn(plcConn *conn) {
	int slot = conn->container_slot;

	containers[slot].conn     = conn;
}

static void insert_container(char *image, char *dockerid, int slot) {
	containers[slot].name     = plc_top_strdup(image);
	containers[slot].dockerid = NULL;
	if (dockerid != NULL) {
		containers[slot].dockerid = plc_top_strdup(dockerid);
	}
	return;
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

	SIMPLE_FAULT_NAME_INJECTOR("plcontainer_before_container_connected");
    
	for (i = 0; i < CONTAINER_NUMBER; i++) {
        if (containers[i].name != NULL &&
            strcmp(containers[i].name, image) == 0) {
            return containers[i].conn;
        }
    }

    return NULL;
}

static char *get_uds_fn(char* uds_dir) {
	char *uds_fn = NULL;
	int   sz;

	/* filename: IPC_GPDB_BASE_DIR + "." + PID + "." + DOMAIN_SOCKET_NO  + "." + container_slot / UDS_SHARED_FILE */
	sz = strlen(uds_dir) + 1 + MAX_SHARED_FILE_SZ + 1;
	uds_fn = pmalloc(sz);
	snprintf(uds_fn, sz, "%s/%s", uds_dir, UDS_SHARED_FILE);

	return uds_fn;
}

plcConn *start_backend(plcContainerConf *conf) {
    int port = 0;
    unsigned int sleepus = 25000;
    unsigned int sleepms = 0;
    plcMsgPing *mping = NULL;
    plcConn *conn = NULL;
    char *dockerid = NULL;
	char *uds_fn = NULL;
	char* uds_dir = NULL;
	int   container_slot;
    int   res = 0;
	int   wait_status;

	container_slot = find_container_slot();

    enum PLC_BACKEND_TYPE plc_backend_type = BACKEND_DOCKER;
    plc_backend_prepareImplementation(plc_backend_type);

    /*
     *  Here the uds_dir is only used by connection of domain socket type
     *  It remains NULL for connection of non domain socket type.
     *
     */
    res = plc_backend_create(conf, &dockerid, container_slot, &uds_dir);
    if (res < 0) {
        elog(ERROR, "%s", api_error_message);
        return conn;
    }
    elog(DEBUG1, "docker created with id %s.", dockerid);

	/* Insert it into containers[] so that in case below operations fails,
	 * it could longjump to plcontainer_call_handler()->delete_containers()
	 * to delete all the containers. We will fill in conn after the connection is
	 * established.
	 */
	insert_container(conf->name, dockerid, container_slot);

    res = plc_backend_start(dockerid);
    if (res < 0) {
        elog(ERROR, "%s", api_error_message);
        return conn;
    }

    time_t rawtime;
    struct tm * timeinfo;
    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    elog(DEBUG1, "container %s has started at %s", dockerid, asctime (timeinfo));

	/* For network connection only. */
	if (conf->isNetworkConnection) {
        char *element = NULL;
		res = plc_backend_inspect(dockerid, &element, PLC_INSPECT_PORT);
		if (res < 0) {
			elog(ERROR, "%s", api_error_message);
			return conn;
		}
        port = (int) strtol(element, NULL, 10);
	}

	if (!conf->isNetworkConnection)
		uds_fn = get_uds_fn(uds_dir);

	/* Give chance to reap some possible zoombie cleanup processes here.
	 * zoombie occurs only when container exits abnormally and QE process
	 * exists, which should not happen often. We could daemonize the cleanup
	 * process to avoid this but having QE as its parent seems to be more
	 * debug-friendly.
	 */
#ifdef HAVE_WAITPID
	while (waitpid(-1, &wait_status, WNOHANG) > 0);
#else
	while (wait3(&wait_status, WNOHANG, NULL) > 0);
#endif

    /* Create a process to clean up the container after it finishes */
    cleanup(dockerid, uds_fn);

    SIMPLE_FAULT_NAME_INJECTOR("plcontainer_before_container_started");

    /* Making a series of connection attempts unless connection timeout of
     * CONTAINER_CONNECT_TIMEOUT_MS is reached. Exponential backoff for
     * reconnecting first attempts: 25ms, 50ms, 100ms, 200ms, 200ms, etc.
     */
    mping = palloc(sizeof(plcMsgPing));
    mping->msgtype = MT_PING;
    while (sleepms < CONTAINER_CONNECT_TIMEOUT_MS) {
        int         res = 0;
        plcMessage *mresp = NULL;

		if (!conf->isNetworkConnection)
			conn = plcConnect_ipc(uds_fn);
		else
			conn = plcConnect_inet(port);

        if (conn != NULL) {
			elog(DEBUG1, "Connected to container via %s",
				conf->isNetworkConnection ? "network" : "unix domain socket");
			conn->container_slot = container_slot;
            res = plcontainer_channel_send(conn, (plcMessage*)mping);
            if (res == 0) {
                res = plcontainer_channel_receive(conn, &mresp, MT_PING_BIT);
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
        set_container_conn(conn);
    }

    pfree(dockerid);
	if (uds_fn != NULL)
		pfree(uds_fn);

    return conn;
}

void delete_containers() {
    int i;

    if (containers_init != 0) {
        for (i = 0; i < CONTAINER_NUMBER; i++) {
            if (containers[i].name != NULL) {

                /* Terminate container process */
                if (containers[i].dockerid != NULL) {
                    plc_backend_delete(containers[i].dockerid);
                    pfree(containers[i].dockerid);
                }

                plcDisconnect(containers[i].conn);

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
        elog(ERROR, "Container declaration format should be '#container:container_name'");
        return name;
    }

    first++;
    /* Ignore whitespaces after the hash sign */
    while (first < len && is_whitespace(source[first]))
        first++;

    /* Line should be "# container :", fail if not so */
    if (strncmp(&source[first], "container", strlen("container")) != 0) {
        elog(ERROR, "Container declaration format should be '#container:container_name'");
        return name;
    }

    /* Follow the line up to colon sign */
    while (first < last && source[first] != ':')
        first++;
    first++;

    /* If no colon found - bad declaration */
    if (first >= last) {
        elog(ERROR, "Container declaration format should be '#container:container_name'");
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
        elog(ERROR, "Container name cannot be empty");
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
        elog(ERROR, "Container name '%s' contains illegal character for container.", name);
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
