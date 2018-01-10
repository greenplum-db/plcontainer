/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <libgen.h>
#include <sys/wait.h>

#include "postgres.h"
#include "cdb/cdbvars.h"
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
	char *runtimeid;
	char *dockerid;
	plcConn *conn;
} container_t;

#define MAX_CONTAINER_NUMBER 10
#define CLEANUP_SLEEP_SEC 2
#define CLEANUP_CONTAINER_CONNECT_RETRY_TIMES 60

static volatile int containers_init = 0;
static volatile container_t* volatile containers;
static char *uds_fn_for_cleanup;

static void init_containers();

static int check_runtime_id(const char *id);

#ifndef CONTAINER_DEBUG

static int qe_is_alive(char *dockerid) {

	/*
	 * default return code to tell cleanup process that everything
	 * is normal
	 */
	int return_code = 1;

	if (getppid() == 1) {
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
		     strcmp("false", element) == 0)) {
			return_code = plc_backend_delete(dockerid);
		} else if (strcmp("unexist", element) == 0) {
			return_code = 0;
		}

		pfree(element);
	}

	return return_code;
}

static void cleanup_uds(char *uds_fn) {
	if (uds_fn != NULL) {
		unlink(uds_fn);
		rmdir(dirname(uds_fn));
	}
}

static void cleanup_atexit_callback() {
	cleanup_uds(uds_fn_for_cleanup);
	free(uds_fn_for_cleanup);
	uds_fn_for_cleanup = NULL;
}

static void cleanup(char *dockerid, char *uds_fn) {
	pid_t pid = 0;

	/* We fork the process to synchronously wait for backend to exit */
	pid = fork();
	if (pid == 0) {
		char psname[200];
		int res;
		int wait_times = 0;

		MyProcPid = getpid();

		/* We do not need proc_exit() callbacks of QE. Besides, we
		 * do not use on_proc_exit() + proc_exit() since it may invovle
		 * some QE related operations, * e.g. Quit interconnect, etc, which
		 * might finally damage QE. Instead we register our own onexit
		 * callback functions and invoke them via exit().
		 * Finally we might better invoke a pg/gp independent program
		 * to manage the lifecycles of backends.
		 */
		on_exit_reset();
		if (uds_fn != NULL) {
			uds_fn_for_cleanup = strdup(uds_fn);
		} else {
			/* use network TCP/IP, no need to clean up uds file */
			uds_fn_for_cleanup = NULL;
		}
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
		snprintf(psname, sizeof(psname), "plcontainer cleaner %s", dockerid);
		set_ps_display(psname, false);
		res = 0;
		PG_TRY();
		{
			/* elog need to be try-catch in cleanup process to avoid longjump*/
			write_log("plcontainer cleanup process launched for docker id: %s and executor process %d",
			          dockerid, getppid());

			while (1) {

				/* Check parent pid whether parent process is alive or not.
				 * If not, kill and remove the container.
				 */
				if (log_min_messages <= DEBUG1)
					write_log("plcontainer cleanup process: Checking whether QE is alive");
				res = qe_is_alive(dockerid);
				if (log_min_messages <= DEBUG1)
					write_log("plcontainer cleanup process: QE alive status: %d", res);

				/* res = 0, backend exited, backend has been successfully deleted.
				 * res < 0, backend exited, backend delete API reports an error.
				 * res > 0, backend still alive, check container status.
				 */
				if (res == 0) {
					break;
				} else if (res < 0) {
					wait_times++;
					write_log("plcontainer cleanup process: Failed to delete backend in cleanup process (%s). "
					          "Will retry later.", api_error_message);
				} else {
					wait_times = 0;
				}

				/* Check whether conatiner is exited or not.
				 * If exited, remove the container.
				 */
				if (log_min_messages <= DEBUG1)
					write_log("plcontainer cleanup process: Checking whether the backend is alive");
				res = container_is_alive(dockerid);
				if (log_min_messages <= DEBUG1)
					write_log("plcontainer cleanup process: Backend alive status: %d", res);

				/* res = 0, container exited, container has been successfully deleted.
				 * res < 0, docker API reports an error.
				 * res > 0, container still alive, sleep and check again.
				 */
				if (res == 0) {
					break;
				} else if (res < 0) {
					wait_times++;
					write_log(
						"plcontainer cleanup process: Failed to inspect or delete backend in cleanup process (%s). "
						"Will retry later.", api_error_message);
				} else {
					wait_times = 0;
				}

				if (wait_times >= CLEANUP_CONTAINER_CONNECT_RETRY_TIMES) {
					write_log("plcontainer cleanup process: Docker API fails after %d retries. cleanup "
					          "process will exit.", wait_times);
					break;
				}

				sleep(CLEANUP_SLEEP_SEC);
			}

			write_log("plcontainer cleanup process deleted docker %s with return value %d",
			          dockerid, res);
			exit(res);
		}
		PG_CATCH();
		{
			/* Do not rethrow to previous stack context. exit immediately.*/
			write_log("plcontainer cleanup process should not reach here. Anyway it should"
			          " not hurt. Exiting. dockerid is %s. You might need to check"
			          " and delete the container manually ('docker rm').", dockerid);
			exit(-1);
		}
		PG_END_TRY();
	} else if (pid < 0) {
		plc_elog(ERROR, "Could not create cleanup process for container %s", dockerid);
	}
}

#endif /* not CONTAINER_DEBUG */

static int find_container_slot() {
	int i;

	for (i = 0; i < MAX_CONTAINER_NUMBER; i++) {
		if (containers[i].runtimeid == NULL) {
			return i;
		}
	}
	// Fatal would cause the session to be closed
	plc_elog(FATAL, "Single session cannot handle more than %d open containers simultaneously", MAX_CONTAINER_NUMBER);

}

static void set_container_conn(plcConn *conn) {
	int slot = conn->container_slot;

	containers[slot].conn = conn;
}

static void insert_container_slot(char *runtime_id, char *dockerid, int slot) {
	containers[slot].runtimeid = plc_top_strdup(runtime_id);
	containers[slot].dockerid = NULL;
	if (dockerid != NULL) {
		containers[slot].dockerid = plc_top_strdup(dockerid);
	}
	return;
}

static void delete_container_slot(int slot) {
	if (containers[slot].runtimeid != NULL) {
		pfree(containers[slot].runtimeid);
		containers[slot].runtimeid = NULL;
	}

	if (containers[slot].dockerid != NULL) {
		pfree(containers[slot].dockerid);
		containers[slot].dockerid = NULL;
	}

	containers[slot].conn = NULL;

	return;
}

static void init_containers() {
	containers = (container_t *) plc_top_alloc(MAX_CONTAINER_NUMBER * sizeof(container_t));
	memset((void *)containers, 0, MAX_CONTAINER_NUMBER * sizeof(container_t));
	containers_init = 1;
}

plcConn *get_container_conn(const char *runtime_id) {
	size_t i;
	if (containers_init == 0) {
		init_containers();
	}

	SIMPLE_FAULT_NAME_INJECTOR("plcontainer_before_container_connected");

	for (i = 0; i < MAX_CONTAINER_NUMBER; i++) {
		if (containers[i].runtimeid != NULL &&
		    strcmp(containers[i].runtimeid, runtime_id) == 0) {
			return containers[i].conn;
		}
	}

	return NULL;
}

static char *get_uds_fn(char *uds_dir) {
	char *uds_fn = NULL;
	int sz;

	/* filename: IPC_GPDB_BASE_DIR + "." + PID + "." + DOMAIN_SOCKET_NO  + "." + container_slot / UDS_SHARED_FILE */
	sz = strlen(uds_dir) + 1 + MAX_SHARED_FILE_SZ + 1;
	uds_fn = pmalloc(sz);
	snprintf(uds_fn, sz, "%s/%s", uds_dir, UDS_SHARED_FILE);

	return uds_fn;
}

plcConn *start_backend(runtimeConfEntry *conf) {
	int port = 0;
	unsigned int sleepus = 25000;
	unsigned int sleepms = 0;
	plcMsgPing *mping = NULL;
	plcConn *conn = NULL;
	char *dockerid = NULL;
	char *uds_fn = NULL;
	char *uds_dir = NULL;
	int container_slot;
	int res = 0;
	int wait_status, _loop_cnt;

	container_slot = find_container_slot();

	enum PLC_BACKEND_TYPE plc_backend_type = BACKEND_DOCKER;
	plc_backend_prepareImplementation(plc_backend_type);

	/*
	 *  Here the uds_dir is only used by connection of domain socket type.
	 *  It remains NULL for connection of non domain socket type.
	 *
	 */
	_loop_cnt = 0;

	/*
	 * We need to block signal when we are creating a container from docker,
	 * until the created container is registered in the container slot, which 
	 * can be used to cleanup the residual container when exeption happens.
	 */
	PG_SETMASK(&BlockSig);
	while ((res = plc_backend_create(conf, &dockerid, container_slot, &uds_dir)) < 0) {
		if (++_loop_cnt >= 3)
			break;
		pg_usleep(2000 * 1000L);
		plc_elog(LOG, "plc_backend_create() fails. Retrying [%d]", _loop_cnt);
	}

	if (res < 0) {
		plc_elog(ERROR, "Backend create error: %s", api_error_message);
		return NULL;
	}
	plc_elog(DEBUG1, "docker created with id %s.", dockerid);

	/* Insert it into containers[] so that in case below operations fails,
	 * it could longjump to plcontainer_call_handler()->delete_containers()
	 * to delete all the containers. We will fill in conn after the connection is
	 * established.
	 */
	insert_container_slot(conf->runtimeid, dockerid, container_slot);
	/*
	 * Unblock signals after we insert the container identifier into the 
	 * container slot for later cleanup.
	 */
	PG_SETMASK(&UnBlockSig);

	pfree(dockerid);
	dockerid = containers[container_slot].dockerid;

	if (!conf->isNetworkConnection)
		uds_fn = get_uds_fn(uds_dir);

	_loop_cnt = 0;
	while ((res = plc_backend_start(dockerid)) < 0) {
		if (++_loop_cnt >= 3)
			break;
		pg_usleep(2000 * 1000L);
		plc_elog(LOG, "plc_backend_start() fails. Retrying [%d]", _loop_cnt);
	}
	if (res < 0) {
		if (!conf->isNetworkConnection)
			cleanup_uds(uds_fn);
		plc_elog(ERROR, "Backend start error: %s", api_error_message);
		return NULL;
	}

	time_t rawtime;
	struct tm *timeinfo;
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	plc_elog(DEBUG1, "container %s has started at %s", dockerid, asctime(timeinfo));

	/* For network connection only. */
	if (conf->isNetworkConnection) {
		char *element = NULL;
		res = plc_backend_inspect(dockerid, &element, PLC_INSPECT_PORT);
		if (res < 0) {
			if (!conf->isNetworkConnection)
				cleanup_uds(uds_fn);
			plc_elog(ERROR, "Backend inspect error: %s", api_error_message);
			return NULL;
		}
		port = (int) strtol(element, NULL, 10);
		pfree(element);
	}

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
		int res = 0;
		plcMessage *mresp = NULL;

		if (!conf->isNetworkConnection)
			conn = plcConnect_ipc(uds_fn);
		else
			conn = plcConnect_inet(port);

		if (conn != NULL) {
			plc_elog(DEBUG1, "Connected to container via %s",
			     conf->isNetworkConnection ? "network" : "unix domain socket");
			conn->container_slot = container_slot;

			res = plcontainer_channel_send(conn, (plcMessage *) mping);
			if (res == 0) {
				res = plcontainer_channel_receive(conn, &mresp, MT_PING_BIT);
				if (mresp != NULL)
					pfree(mresp);
				if (res == 0) {
					break;
				} else {
					plc_elog(DEBUG1, "Failed to receive pong from client. Maybe expected. dockerid: %s", dockerid);
					plcDisconnect(conn);
				}
			} else {
				plc_elog(DEBUG1, "Failed to send ping to client. Maybe expected. dockerid: %s", dockerid);
				plcDisconnect(conn);
			}

			/*
			 * Note about the plcDisconnect(conn) code above:
			 *
			 * We saw the case that connection() + send() are ok, but rx
			 * fails with "reset by peer" while the client program has not started
			 * listen()-ing. That happens with the docker bridging + NAT network
			 * solution when the QE connects via the lo interface (i.e. 127.0.0.1).
			 * We did not try other solutions like macvlan, etc yet. It appears
			 * that this is caused by the docker proxy program. We could work
			 * around this by setting docker userland-proxy as false or connecting via
			 * non-localhost on QE, however to make our code tolerate various
			 * configurations, we allow reconnect here since that does not seem
			 * to harm the normal case although since client will just accept()
			 * the tcp connection once reconnect should never happen.
			 */
		} else {
			plc_elog(DEBUG1, "Failed to connect to client. Maybe expected. dockerid: %s", dockerid);
		}

		usleep(sleepus);
		plc_elog(DEBUG1, "Waiting for %u ms for before reconnecting", sleepus / 1000);
		sleepms += sleepus / 1000;
		sleepus = sleepus >= 200000 ? 200000 : sleepus * 2;
	}

	if (sleepms >= CONTAINER_CONNECT_TIMEOUT_MS) {
		if (!conf->isNetworkConnection)
			cleanup_uds(uds_fn);
		plc_elog(ERROR, "Cannot connect to the container, %d ms timeout reached",
		     CONTAINER_CONNECT_TIMEOUT_MS);
		conn = NULL;
	} else {
		set_container_conn(conn);
	}

	if (uds_fn != NULL)
		pfree(uds_fn);

	return conn;
}

void delete_containers() {
	int i;

	if (containers_init != 0) {
		for (i = 0; i < MAX_CONTAINER_NUMBER; i++) {
			if (containers[i].runtimeid != NULL) {

				/* Terminate container process */
				if (containers[i].dockerid != NULL) {
					int res;
					int _loop_cnt = 0;

					while ((res = plc_backend_delete(containers[i].dockerid)) < 0 && _loop_cnt++ < 3)
						pg_usleep(2000 * 1000L);
					if (res < 0)
						plc_elog(NOTICE, "Backend delete error: %s", api_error_message);
				}

				plcDisconnect(containers[i].conn);

				delete_container_slot(i);
			}
		}
	}

	containers_init = 0;
}

char *parse_container_meta(const char *source) {
	int first, last, len;
	char *runtime_id = NULL;

	first = 0;
	len = strlen(source);

	/* If the string is not starting with hash, fail */
	/* Must call isspace() since there is heading '\n'. */
	while (first < len && isspace(source[first]))
		first++;
	if (first == len || source[first] != '#') {
		plc_elog(ERROR, "Runtime declaration format should be '#container: runtime_id': (No '#' is found): %d %d %d", first, len, (int) source[first]);
		return runtime_id;
	}
	first++;

	/* If the string does not proceed with "container", fail */
	while (first < len && isblank(source[first]))
		first++;
	if (first == len || strncmp(&source[first], "container", strlen("container")) != 0) {
		plc_elog(ERROR, "Runtime declaration format should be '#container: runtime_id': (Not 'container'): %d %d %d", first, len, (int) source[first]);
		return runtime_id;
	}
	first += strlen("container");

	/* If no colon found - bad declaration */
	while (first < len && isblank(source[first]))
		first++;
	if (first == len || source[first] != ':') {
		plc_elog(ERROR, "Runtime declaration format should be '#container: runtime_id': (No ':' is found after 'container'): %d %d %d", first, len, (int) source[first]);
		return runtime_id;
	}
	first++;

	/* Ignore blanks before runtime_id. */
	while (first < len && isblank(source[first]))
		first++;
	if (first == len) {
		plc_elog(ERROR, "Runtime declaration format should be '#container: runtime_id': (runtime id is empty)");
		return runtime_id;
	}

 	/* Read everything up to the first newline or end of string */
	last = first;
	while (last < len && source[last] != '\n' && source[last] != '\r')
		last++;
	if (last == len) {
		plc_elog(ERROR, "Runtime declaration format should be '#container: runtime_id': (no carriage return in code)");
		return runtime_id;
	}
	last--; /* For '\n' or '\r' */

	/* Ignore whitespace in the end of the line */
	while (last >= first && isblank(source[last]))
		last--;
	if (first > last) {
		plc_elog(ERROR, "Runtime id cannot be empty");
		return NULL;
	}

	/*
	 * Allocate container id variable and copy container id 
	 * the character length of id is last-first.
	 */
	runtime_id = (char *) pmalloc(last - first + 1 + 1);
	memcpy(runtime_id, &source[first], last - first + 1);

	runtime_id[last - first + 1] = '\0';

	int regt = check_runtime_id(runtime_id);
	if (regt == -1) {
		plc_elog(ERROR, "Container id '%s' contains illegal character for container.", runtime_id);
	}

	return runtime_id;
}

/*
 * check whether configuration id specified in function declaration
 * satisfy the regex which follow docker container/image naming conventions.
 */
static int check_runtime_id(const char *id) {
	int status;
	regex_t re;
	if (regcomp(&re, "^[a-zA-Z0-9][a-zA-Z0-9_.-]*$", REG_EXTENDED | REG_NOSUB | REG_NEWLINE) != 0) {
		return -1;
	}
	status = regexec(&re, id, (size_t) 0, NULL, 0);
	regfree(&re);
	if (status != 0) {
		return -1;
	}
	return 0;
}
