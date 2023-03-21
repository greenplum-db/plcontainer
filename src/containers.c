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
#include "miscadmin.h"
#include "utils/guc.h"
#ifndef PLC_PG
  #include "cdb/cdbvars.h"
  #include "utils/faultinjector.h"
#else
  #include "catalog/pg_type.h"
  #include "miscadmin.h"
  #include "utils/guc.h"
  #include <stdarg.h>
#endif
#include "storage/ipc.h"
#include "libpq/pqsignal.h"
#include "utils/ps_status.h"
#include "common/comm_utils.h"
#include "common/comm_channel.h"
#include "common/comm_connectivity.h"
#include "common/messages/messages.h"
#include "plc_configuration.h"
#include "containers.h"
#include "plc_container_info.h"
#include "plc_backend_api.h"

typedef struct container_t {
	char *runtimeid;
	backendConnectionInfo *backend;
	runtimeConnectionInfo *connection;
	plcConn *conn;
} container_t;

#define MAX_CONTAINER_NUMBER 10
#define CLEANUP_SLEEP_SEC 3
#define CLEANUP_CONTAINER_CONNECT_RETRY_TIMES 60

static volatile int containers_init = 0;
static volatile container_t* volatile containers;
static char *uds_fn_for_cleanup;
static char *dockerid_for_cleanup;

static void init_containers();

static int check_runtime_id(const char *id);

#ifndef CONTAINER_DEBUG

static int check_container_if_oomkilled(const backendConnectionInfo *backend, const runtimeConnectionInfo *connection) {
	char *element = NULL;

	int return_code = 0;
	int res;

	/*
	 * sleep CLEANUP_SLEEP_SEC seconds to make sure the docker container
	 * status is synced
	 */
	sleep(CLEANUP_SLEEP_SEC);

	res = plc_backend_inspect(PLC_INSPECT_OOM, backend, connection, &element);
	if (res < 0) {
		return_code = res;
	} else if (element != NULL) {
		if ((strcmp("false", element) == 0))
			return_code = 0;
		else if ((strcmp("true", element) == 0))
			return_code = 1;
		else
			return_code = -1;

		pfree(element);
	}

	return return_code;
}

static int delete_backend_if_exited(const backendConnectionInfo *backend, const runtimeConnectionInfo *connection) {
	char *element = NULL;

	int return_code = 1;
	int res;
	res = plc_backend_inspect(PLC_INSPECT_STATUS, backend, connection, &element);
	if (res < 0) {
		return_code = res;
	} else if (element != NULL) {
		if ((strcmp("exited", element) == 0 ||
		     strcmp("false", element) == 0)) {
			if (check_container_if_oomkilled(backend, connection) == 1) {
				/*
				 * check if the process is QE or not
				 */
				if (getppid() == PostmasterPid) {
					plc_elog(WARNING, "docker reports container has been terminated due to out of memory." 
							 " This could be either the container was over memory limit or inside program crashed");
				} else {
					write_log("plcontainer cleanup process: container %s has been killed by oomkiller", connection->identity);
				}
			}

			return_code = plc_backend_delete(backend, connection);

		} else if (strcmp("unexist", element) == 0) {
			return_code = 0;
		}

		pfree(element);
	}

	return return_code;
}

static void cleanup_uds(runtimeConnectionInfo *info) {
	if (info == NULL || info->tag != PLC_RUNTIME_CONNECTION_UDS)
		return;

	if (info->connection_uds.uds_address == NULL)
		return;

	unlink(info->connection_uds.uds_address);
	rmdir(dirname(info->connection_uds.uds_address));
}

static void cleanup_atexit_callback() {
	runtimeConnectionInfo info = {
		.tag = PLC_RUNTIME_CONNECTION_UDS,
		.connection_uds.uds_address = uds_fn_for_cleanup,
	};

	cleanup_uds(&info);
	free(uds_fn_for_cleanup);

    /* Remove entry from shm */
    del_containerid_entry(dockerid_for_cleanup);
    free(dockerid_for_cleanup);

    dockerid_for_cleanup = NULL;
	uds_fn_for_cleanup = NULL;
}

static void cleanup(const backendConnectionInfo *backend, const runtimeConnectionInfo *info) {
	/* fork the process to synchronously wait for backend to exit */
	pid_t pid = fork();
	if (pid < 0) {
		plc_elog(ERROR, "Could not create cleanup process for container %s", info->identity);
	}

	if (pid == 0 /* child */ ) {
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

		if (info->tag == PLC_RUNTIME_CONNECTION_UDS) {
			uds_fn_for_cleanup = strdup(info->connection_uds.uds_address);
		} else {
			uds_fn_for_cleanup = NULL; /* use network TCP/IP, no need to clean up uds file */
		}

        /* dup the container id for clean purpose */
        dockerid_for_cleanup = strdup(info->identity);

#ifdef HAVE_ATEXIT
		atexit(cleanup_atexit_callback);
#else
	#ifdef PLC_PG
		on_exit(cleanup_atexit_callback, NULL);
	#else
		on_exit(cleanup_atexit_callback, NULL);
	#endif
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
		char psname[200];
		snprintf(psname, sizeof(psname), "plcontainer cleaner %s", info->identity);
		set_ps_display(psname, false);

		int res = 0;
		int wait_times = 0;
		PG_TRY();
		{
			/* elog need to be try-catch in cleanup process to avoid longjump*/
			write_log("plcontainer cleanup process launched for docker id: %s and executor process %d",
			          info->identity, getppid());
			while (1) {
				// Check parent pid whether parent process is alive or not.
				if (log_min_messages <= DEBUG1)
					write_log("plcontainer cleanup process: Checking whether QE is alive");

				bool qe_is_alive = (getppid() != 1);

				if (log_min_messages <= DEBUG1)
					write_log("plcontainer cleanup process: QE alive status: %d", qe_is_alive);

				// qe dead. kill the backend
				if (!qe_is_alive) {
					res = plc_backend_kill(backend, info);
					if (res == 0) { // backend has been successfully deleted.
						break;
					} else if (res < 0) { // backend delete API reports an error.
						write_log("plcontainer cleanup process: Failed to kill backend in cleanup process (%s). "
								"retry %d times.", backend_error_message, wait_times);
						wait_times++;
					}
				} else {
					// backend still alive, check container status.
					wait_times = 0;
				}

				/* Check whether conatiner is exited or not. if exited, remove the container. */
				if (log_min_messages <= DEBUG1)
					write_log("plcontainer cleanup process: Checking whether the backend is alive");

				res = delete_backend_if_exited(backend, info);

				if (log_min_messages <= DEBUG1)
					write_log("plcontainer cleanup process: Backend alive status: %d", res);

				if (res > 0) { // container still alive, sleep and check again.
					wait_times = 0;
				} else if (res == 0) { // container exited, container has been successfully deleted.
					break;
				} else if (res < 0) { // docker API error
					wait_times++;
					write_log(
						"plcontainer cleanup process: Failed to inspect or delete backend in cleanup process (%s). "
						"Will retry later.", backend_error_message);
				}

				if (wait_times >= CLEANUP_CONTAINER_CONNECT_RETRY_TIMES) {
					write_log("plcontainer cleanup process: Docker API fails after %d retries. cleanup process will exit.", wait_times);
					break;
				}

				sleep(CLEANUP_SLEEP_SEC);
			}

			write_log("plcontainer cleanup process deleted docker %s with return value %d",
			          info->identity, res);
			exit(res);
		}
		PG_CATCH();
		{
			/* Do not rethrow to previous stack context. exit immediately.*/
			write_log("plcontainer cleanup process should not reach here. Anyway it should"
			          " not hurt. Exiting. dockerid is %s. You might need to check"
			          " and delete the container manually ('docker rm').", info->identity);
			exit(-1);
		}
		PG_END_TRY();
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

static void insert_container_slot(char *runtime_id,
		const backendConnectionInfo *backend,
		const runtimeConnectionInfo *connection,
		int slot) {
	containers[slot].runtimeid = plc_top_strdup(runtime_id);
	containers[slot].backend = NULL;
	containers[slot].connection = NULL;
	if (backend != NULL) {
		containers[slot].backend = runtime_conf_copy_backend_connection_info(backend);
	}
	if (connection != NULL) {
		containers[slot].connection = runtime_conf_copy_runtime_connection_info(connection);
	}
	return;
}

static void init_containers() {
	containers = (container_t *) PLy_malloc(MAX_CONTAINER_NUMBER * sizeof(container_t));
	memset((void *)containers, 0, MAX_CONTAINER_NUMBER * sizeof(container_t));
	containers_init = 1;
}

plcConn *get_container_conn(const char *runtime_id) {
	size_t i;
	if (containers_init == 0) {
		init_containers();
	}

#ifndef PLC_PG
	SIMPLE_FAULT_INJECTOR("plcontainer_before_container_connected");
#endif
	for (i = 0; i < MAX_CONTAINER_NUMBER; i++) {
		if (containers[i].runtimeid != NULL &&
		    strcmp(containers[i].runtimeid, runtime_id) == 0) {
			return containers[i].conn;
		}
	}

	return NULL;
}

char *get_container_id(const char *runtime_id)
{
   	size_t i;
	if (containers_init == 0) {
		init_containers();
	}

	for (i = 0; i < MAX_CONTAINER_NUMBER; i++) {
		if (containers[i].runtimeid != NULL &&
		    strcmp(containers[i].runtimeid, runtime_id) == 0) {
			return containers[i].connection->identity;
		}
	}

	return NULL;
}

// should in comm_connectivity.c, but cannot due to link time error.
static plcConn *plcConnect(struct runtimeConnectionInfo *info) {
	switch (info->tag) {
		case PLC_RUNTIME_CONNECTION_UNKNOWN:
			Assert(!"not implemented");
			break; // make our ancient compile happy
		case PLC_RUNTIME_CONNECTION_TCP:
			return plcConnect_inet(info->connection_tcp.hostname, info->connection_tcp.port);
		case PLC_RUNTIME_CONNECTION_UDS:
			return plcConnect_ipc(info->connection_uds.uds_address);
	}

	Assert(!"unreachable");
	return NULL;
}

plcConn *start_backend(runtimeConfEntry *conf) {
	plcConn *conn = NULL;

	int res = 0, loop_count = 0;

	int container_slot = find_container_slot();
	backendConnectionInfo *backend = runtime_conf_get_backend_connection_info(conf->backend);
	runtimeConnectionInfo *connection = runtime_conf_get_runtime_connection_info(backend);

	/*
	 * We need to block signal when we are creating a container from docker,
	 * until the created container is registered in the container slot, which
	 * can be used to cleanup the residual container when exeption happens.
	 */
	PG_SETMASK(&BlockSig);
	while ((res = plc_backend_create(conf, backend, container_slot, connection)) < 0) {
		if (++loop_count >= 3)
			break;
		pg_usleep(2000 * 1000L);
		plc_elog(LOG, "plc_backend_create() fails. Retrying [%d]", loop_count);
	}

	if (res < 0) {
		ereport(ERROR,
			(errmsg("plcontainer: backend create error"),
			 errdetail("%s", backend_error_message))
		);
		return NULL;
	}
	plc_elog(DEBUG1, "docker created with id %s.", connection->identity);


	/*
	 * Insert it into containers[] so that in case below operations fails,
	 * it could longjump to plcontainer_call_handler()->delete_containers()
	 * to delete all the containers. We will fill in conn after the connection is
	 * established.
	 */
	insert_container_slot(conf->runtimeid, backend, connection, container_slot);

	loop_count = 0;
	while ((res = plc_backend_start(backend, connection)) < 0) {
		if (++loop_count >= 3)
			break;
		pg_usleep(2000 * 1000L);
		plc_elog(LOG, "plc_backend_start() fails. Retrying [%d]", loop_count);
	}
	if (res < 0) {
		cleanup_uds(connection);
		ereport(ERROR,
			(errmsg("plcontainer: backend start error"),
			 errdetail("%s", backend_error_message))
		);
		return NULL;
	}

	time_t current_time = time(NULL);
	plc_elog(DEBUG1, "container %s has started at %s", connection->identity, ctime(&current_time));

	/*
	 * reap zoombie cleanup processes here. (for process backend and cleanup process)
	 * zoombie process occurs only when process exits abnormally and QE process
	 * exists, which should not happen.
	 */
	int wait_status;
#ifdef HAVE_WAITPID
	while (waitpid(-1 /* any child */, &wait_status, WNOHANG) > 0);
#else
	while (wait3(&wait_status, WNOHANG, NULL /* any child */) > 0);
#endif

	/* Create a process to clean up the container after it finishes */
	cleanup(backend, connection);

	/*
	 * Unblock signals after we insert the container identifier into the
	 * container slot for later cleanup.
	 */
	PG_SETMASK(&UnBlockSig);


#ifndef PLC_PG
	SIMPLE_FAULT_INJECTOR("plcontainer_before_container_started");
#endif

	/*
	 * Making a series of connection attempts unless connection timeout of
	 * CONTAINER_CONNECT_TIMEOUT_MS is reached. Exponential backoff for
	 * reconnecting first attempts: 25ms, 50ms, 100ms, 200ms, 200ms, etc.
	 */
	unsigned int sleepus = 25000, sleepms = 0;
	plcMsgPing mping = {.msgtype = MT_PING};
	while (sleepms < CONTAINER_CONNECT_TIMEOUT_MS) {
		conn = plcConnect(connection);

		if (conn == NULL) {
			plc_elog(DEBUG1, "failed to connect to client. Maybe expected. dockerid: %s", connection->identity);
			goto err;
		}

		plc_elog(DEBUG1, "Connected to container via %s", connection->tag == PLC_RUNTIME_CONNECTION_TCP ? "network" : "unix domain socket");
		conn->container_slot = container_slot;

		plcMessage *mresp = NULL;
		res = plcontainer_channel_send(conn, (plcMessage *) &mping);
		if (res == 0) {
			res = plcontainer_channel_receive(conn, &mresp, MT_PING_BIT);

			if (mresp != NULL)
				pfree(mresp);

			if (res == 0) {
				break;
			}

			plc_elog(DEBUG1, "Failed to receive pong from client. Maybe expected. dockerid: %s", connection->identity);
			plcDisconnect(conn);
		} else {
			plc_elog(DEBUG1, "Failed to send ping to client. Maybe expected. dockerid: %s", connection->identity);
			plcDisconnect(conn);
		}

		/*
		 * Note about the plcDisconnect(conn) code above:
		 *
		 * in NAT container network, send() is ok but receive() will RST.
		 * when container is not ready. do the retry here.
		 */

err:
		usleep(sleepus);
		plc_elog(DEBUG1, "Waiting for %u ms for before reconnecting", sleepus / 1000);
		sleepms += sleepus / 1000;
		sleepus = sleepus >= 200000 ? 200000 : sleepus * 2;
	} // end while

	if (sleepms >= CONTAINER_CONNECT_TIMEOUT_MS) {
		plcDisconnect(conn);
		conn = NULL;

		cleanup_uds(connection);
		plc_elog(ERROR, "Cannot connect to the container, %d ms timeout reached. "
			"Check container logs for details.", CONTAINER_CONNECT_TIMEOUT_MS);
	} else {
		set_container_conn(conn);
	}

	runtime_conf_free_backend_connection_info(backend);
	runtime_conf_free_runtime_connection_info(connection);

	return conn;
}

void delete_containers() {
	if (containers_init == 0)
		return;

	for (int i = 0; i < MAX_CONTAINER_NUMBER; i++) {
		const volatile container_t *container = &containers[i];

		if (container == NULL)
			continue;

		/*
		 * Disconnect at first so that container has chance to exit gracefully.
		 * When running code coverage for client code, client needs to
		 * have chance to flush the gcda files thus direct kill-9 is not
		 * proper.
		 */
		plcConn *conn	= containers[i].conn;
		backendConnectionInfo *backend = containers[i].backend;
		runtimeConnectionInfo *connection = containers[i].connection;
		containers[i].backend = NULL;
		containers[i].connection  = NULL;
		containers[i].conn	= NULL;
		plcDisconnect(conn);

		if (connection == NULL || backend == NULL)
			continue;

		/* Terminate container process */
		int res;

		/* Check to see whether backend is exited or not. */
		int _loop_cnt = 0;
		while ((res = delete_backend_if_exited(backend, connection)) != 0 && _loop_cnt++ < 5) {
			pg_usleep(200 * 1000L);
		}

		/* Force to delete the backend if needed. */
		if (res != 0) {
			_loop_cnt = 0;
			while ((res = plc_backend_delete(backend, connection)) < 0 && _loop_cnt++ < 3)
				pg_usleep(1000 * 1000L);
		}

		/*
		 * On rhel6/centos6 there is chance that delete api could fail here
		 * since cleanup process might have just called delete api.
		 * That is due to the docker issue below:
		 *   https://github.com/moby/moby/issues/17170
		 * Thus here we should not expose the log to usual users else
		 * that will confuse them. In the long run, when our cleanup
		 * process is more stable (e.g. PG background worker process
		 * or as an independent service with HA), things might be
		 * different - QE is not responsbile for container deletion.
		 */
		if (res < 0)
			plc_elog(LOG, "Backend delete error: %s", backend_error_message);

		runtime_conf_free_backend_connection_info(backend);
		runtime_conf_free_runtime_connection_info(connection);
	}

	containers_init = 0;
}

char *parse_container_meta(const char *source) {
	int first, last, len;
	char *runtime_id = NULL;
	int regt;

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

	if (last - first + 1 + 1 > RUNTIME_ID_MAX_LENGTH) {
		plc_elog(ERROR, "Runtime id should not be longer than 63 bytes.");
	}
	runtime_id = (char *) pmalloc(last - first + 1 + 1);
	memcpy(runtime_id, &source[first], last - first + 1);

	runtime_id[last - first + 1] = '\0';

	regt = check_runtime_id(runtime_id);
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
	int i = 0;

	// test if match with "^[a-zA-Z0-9][a-zA-Z0-9_.-]*$"
	for (i = 0; id[i] != '\0'; i++) {
		char d = id[i];

		if (d == '_' || d == '-' || d == '.') {
			// . and - can not at start
			if (i == 0)
				return -1;
			else
				continue;
		}

		if ('0' <= d && d <= '9')
			continue;

		if ('a' <= d && d <= 'z')
			continue;

		if ('A' <= d && d <= 'Z')
			continue;

		if (d == '.' || d == '_' || d == '-')
			continue;

		return -1;
	}

	return i != 0 ? 0 : -1;
}
