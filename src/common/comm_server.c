/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */
#ifdef PLC_CLIENT

#include <errno.h>
#include <netinet/ip.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

#include "comm_channel.h"
#include "comm_utils.h"
#include "comm_connectivity.h"
#include "comm_server.h"
#include "comm_log.h"
#include "messages/messages.h"

// init variable
char *dbUsername;
char *dbName;
int dbQePid;
char *clientLanguage;
int client_log_level;

#define MAX_CONNECTIONS 10

/*
 * Function binds the socket and starts listening on it: tcp
 */
static int start_listener_inet() {
	struct sockaddr_in addr;
	int sock;

	/* FIXME: We might want to downgrade from root to normal user in container.*/

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		plc_elog(ERROR, "system call socket() fails: %s", strerror(errno));
	}

	addr = (struct sockaddr_in) {
		.sin_family = AF_INET,
		.sin_port   = htons(SERVER_PORT),
		.sin_addr = {.s_addr = INADDR_ANY},
	};
	if (bind(sock, (const struct sockaddr *) &addr, sizeof(addr)) == -1) {
		plc_elog(ERROR, "Cannot bind the port: %s", strerror(errno));
	}

	if (listen(sock, 10) == -1) {
		plc_elog(ERROR, "Cannot listen the socket: %s", strerror(errno));
	}

	plc_elog(DEBUG1, "Listening via network with port: %d", SERVER_PORT);

	return sock;
}

/*
 * Function binds the socket and starts listening on it: unix domain socket.
 */
static int start_listener_ipc() {
	struct sockaddr_un addr;
	int sock;
	char *uds_fn;
	int sz;
	char *env_str, *endptr;
	uid_t qe_uid;
	gid_t qe_gid;
	long val;

	if (mkdir(IPC_CLIENT_DIR, 0700) < 0) {
		if (errno != EEXIST) {
			plc_elog(ERROR, "Failed to create directory for Unix-Domain Socket: %s", strerror(errno));
		}
	}

	/* filename: IPC_CLIENT_DIR + '/' + UDS_SHARED_FILE */
	sz = strlen(IPC_CLIENT_DIR) + 1 + MAX_SHARED_FILE_SZ + 1;
	uds_fn = pmalloc(sz);
	sprintf(uds_fn, "%s/%s", IPC_CLIENT_DIR, UDS_SHARED_FILE);
	if (strlen(uds_fn) >= sizeof(addr.sun_path)) {
		plc_elog(ERROR, "PLContainer: The path for unix domain socket "
			"connection is too long: %s", uds_fn);
	}

	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock == -1) {
		plc_elog(ERROR, "system call socket() fails: %s", strerror(errno));
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, uds_fn);

	unlink(uds_fn);
	if (access(uds_fn, F_OK) == 0)
		plc_elog (ERROR, "Cannot delete the file for unix domain socket "
			"connection: %s", uds_fn);

	if (bind(sock, (const struct sockaddr *) &addr, sizeof(addr)) == -1) {
		plc_elog(ERROR, "Cannot bind the addr: %s", strerror(errno));
	}

	if (getenv("LOCAL_PROCESS_MODE") == NULL) {
		/*
		 * The path owner should be generally the uid, but we are not 100% sure
		 * about this for current/future backends, so we still use environment
		 * variable, instead of extracting them via reading the owner of the path.
		 */

		/* Get executor uid: for permission of the unix domain socket file. */
		if ((env_str = getenv("EXECUTOR_UID")) == NULL)
			plc_elog (ERROR, "EXECUTOR_UID is not set, something wrong on QE side");
		errno = 0;
		val = strtol(env_str, &endptr, 10);
		if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) ||
			(errno != 0 && val == 0) ||
			endptr == env_str ||
			*endptr != '\0') {
			plc_elog(ERROR, "EXECUTOR_UID is wrong:'%s'", env_str);
		}
		qe_uid = val;

		/* Get executor gid: for permission of the unix domain socket file. */
		if ((env_str = getenv("EXECUTOR_GID")) == NULL)
			plc_elog (ERROR, "EXECUTOR_GID is not set, something wrong on QE side");
		errno = 0;
		val = strtol(env_str, &endptr, 10);
		if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) ||
			(errno != 0 && val == 0) ||
			endptr == env_str ||
			*endptr != '\0') {
			plc_elog(ERROR, "EXECUTOR_GID is wrong:'%s'", env_str);
		}
		qe_gid = val;

		/* Change ownership & permission for the file for unix domain socket so
		 * code on the QE side could access it and clean up it later.
		 */
		if (chown(uds_fn, qe_uid, qe_gid) < 0)
			plc_elog (ERROR, "Could not set ownership for file %s with owner %d, "
				"group %d: %s", uds_fn, qe_uid, qe_gid, strerror(errno));
		if (chmod(uds_fn, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) < 0) /* 0666*/
			plc_elog (ERROR, "Could not set permission for file %s: %s",
						uds_fn, strerror(errno));
	}

	if (listen(sock, 10) == -1) {
		plc_elog(ERROR, "Cannot listen the socket: %s", strerror(errno));
	}

	/*
	 * Set passed UID from OS is currently disabled due to the container OS may different with host OS.
	 * Hence use passed CLIENT_UID may has some risks.
	 * */

	plc_elog(DEBUG1, "Listening via unix domain socket with file: %s", uds_fn);

	return sock;
}

int start_listener() {
	int sock;
	char *use_container_network;
	char *env_str, *endptr;
	long val;

	/* Get current db user name and database name and QE PID*/
	if ((env_str = getenv("DB_USER_NAME")) == NULL) {
		dbUsername = "unknown";
	} else {
		dbUsername = strdup(env_str);
	}

	if ((env_str = getenv("DB_NAME")) == NULL) {
		dbName = "unknown";
	} else {
		dbName = strdup(env_str);
	}

	if ((env_str = getenv("DB_QE_PID")) == NULL) {
		dbQePid = -1;
	} else {
		val = strtol(env_str, &endptr, 10);
		if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) ||
		    (errno != 0 && val == 0) ||
		    endptr == env_str ||
		    *endptr != '\0') {
			plc_elog(ERROR, "DB_QE_PID is wrong:'%s'", env_str);
		}
		dbQePid = (int) val;
	}

	if ((env_str = getenv("CLIENT_LANGUAGE")) == NULL) {
		clientLanguage = "unknown";
	} else {
		clientLanguage = strdup(env_str);
	}

	use_container_network = getenv("USE_CONTAINER_NETWORK");
	if (use_container_network == NULL) {
		use_container_network = "false";
		plc_elog(WARNING, "USE_CONTAINER_NETWORK is not set, use default value \"no\".");
	}

	if (strcasecmp("true", use_container_network) == 0) {
		sock = start_listener_inet();
	} else if (strcasecmp("false", use_container_network) == 0){
		if ((getenv("LOCAL_PROCESS_MODE") == NULL) && (geteuid() != 0 || getuid() != 0)) {
			plc_elog(ERROR, "Must run as root and then downgrade to usual user.");
			return -1;
		}
		sock = start_listener_ipc();
	} else {
		sock = -1;
		plc_elog(ERROR, "USE_CONTAINER_NETWORK is set to wrong value '%s'", use_container_network);
	}

	return sock;
}


/*
 * Function waits for the socket to accept connection for finite amount of time
 * and errors out when the timeout is reached and no client connected
 */
int connection_wait(fd_set* fdset, int sock, plcConn* all_connections[]) {
	struct timeval timeout;
	int rv;

	FD_ZERO(fdset);    /* clear the set */
	FD_SET(sock, fdset); /* add our file descriptor to the set */
	timeout.tv_sec = TIMEOUT_SEC;
	timeout.tv_usec = 0;

	int max_fd = sock;
	for (int i = 0; i < MAX_CONNECTIONS; i++) {
		if (all_connections[i] == NULL) continue;
		FD_SET(all_connections[i]->sock, fdset);
		if (all_connections[i]->sock > max_fd) {
			max_fd = all_connections[i]->sock;
		}
	}

	rv = select(max_fd + 1, fdset, NULL, NULL, &timeout);
	if (rv == -1) {
		plc_elog(ERROR, "Failed to select() socket: %s", strerror(errno));
	}
	if (rv == 0) {
		plc_elog(LOG, "Socket timeout - no incoming event within %d "
			"seconds", TIMEOUT_SEC);
	}
	return rv;
}

static void connection_close(plcConn *conn) {
	plc_elog(LOG, "Closing connection");
	close(conn->sock);
	pfree(conn);
}

/*
 * Function accepts the connection and initializes structure for it
 */
plcConn *connection_init(int sock) {
	socklen_t raddr_len;
	struct sockaddr_in raddr;
	struct timeval tv;
	int connection;

	raddr_len = sizeof(raddr);
	connection = accept(sock, (struct sockaddr *) &raddr, &raddr_len);
	plc_elog(LOG, "Accepting a connection: %d (%s)", connection, strerror(errno));
	if (connection == -1) {
		plc_elog(LOG, "Accepting a connection: %d (%s)", connection, strerror(errno));
		goto error;
	}

	/* Set socket receive timeout to 500ms */
	tv.tv_sec = 0;
	tv.tv_usec = 500000;
	setsockopt(connection, SOL_SOCKET, SO_RCVTIMEO, (char *) &tv, sizeof(struct timeval));

	plcConn *conn = plcConnInit(connection);
	plcMessage *msg = NULL;
	int res = 0;

	res = plcontainer_channel_receive(conn, &msg, MT_PING_BIT);
	if (res < 0) {
		plc_elog(LOG, "Error receiving data from the backend, %d", res);
		goto error;
	}

	res = plcontainer_channel_send(conn, msg);
	if (res < 0) {
		plc_elog(LOG, "Cannot send 'ping' message response");
		goto error;
	}
	pfree(msg);
	return conn;

error:
	pfree(msg);
	connection_close(conn);
	return NULL;
}

static int connection_add(plcConn *all_connections[], plcConn *new_conn) {
	for (int i = 0; i < MAX_CONNECTIONS; i++) {
		if (all_connections[i] == NULL) {
			all_connections[i] = new_conn;
			return 0;
		}
	}
	plc_elog(LOG, "Too many connections");
	return -1;
}

/*
 * The loop of receiving commands from the Greenplum process and processing them
 */
void receive_loop(void (*handle_call)(plcMsgCallreq *, plcConn *)) {
	plcConn *all_connections[MAX_CONNECTIONS] = {NULL};
	/* Different from PostMaster, we only listen to one socket. */
	int listen_sock = start_listener();
	if (listen_sock + 1 > FD_SETSIZE) {
		plc_elog(ERROR, "Fd %d exceeded FD_SETSIZE", listen_sock);
	}
	fd_set fdset;
	struct timeval timeout;

	while (1) {
		int num_fds = connection_wait(&fdset, listen_sock, all_connections);
		if (num_fds <= 0) continue;
		if (FD_ISSET(listen_sock, &fdset)) {
			plcConn *conn = connection_init(listen_sock);
			if (conn) {
				int ret = connection_add(all_connections, conn);
				if (ret < 0)
					connection_close(conn);
			}
		}
		for (int i = 0; i < MAX_CONNECTIONS; i++) {
			plcConn *conn = all_connections[i];
			if (conn == NULL) continue;
			if (FD_ISSET(conn->sock, &fdset)) {
				plcMessage *msg = NULL;
				int res = plcontainer_channel_receive(conn, &msg, MT_CALLREQ_BIT);

				if (res < 0) {
					plc_elog(ERROR, "Error receiving data from the peer: %d (%s)", res, strerror(errno));
					break;
				}
				plc_elog(DEBUG1, "Client receive a request: called function oid %u", ((plcMsgCallreq *) msg)->objectid);
				handle_call((plcMsgCallreq *) msg, conn);
				free_callreq((plcMsgCallreq *) msg, false, false);
			}
		}
	}
}

#endif
