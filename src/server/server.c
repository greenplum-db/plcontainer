/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>

#include "common/comm_channel.h"
#include "common/comm_connectivity.h"
#include "common/comm_dummy.h"
#include "common/messages/messages.h"
#include "server/server.h"

/*
 * Function binds the socket and starts listening on it: tcp
 */
static int start_listener_inet() {
	char address[32];
	snprintf(address, sizeof(address), ":%u", SERVER_PORT);
	return plcListenServer("tcp", address);
}

/*
 * Function binds the socket and starts listening on it: unix domain socket.
 */
static int start_listener_ipc(const char *stand_alone_uds) {
	int sock;
	char uds_fn[1024];
	char *env_str, *endptr;
	uid_t qe_uid;
	gid_t qe_gid;
	long val;

	/* filename: IPC_CLIENT_DIR + '/' + UDS_SHARED_FILE */
	if (stand_alone_uds == NULL) {
		snprintf(uds_fn, sizeof(uds_fn), "%s/%s", IPC_CLIENT_DIR, UDS_SHARED_FILE);
	} else {
		strncpy(uds_fn, stand_alone_uds, 1024);
	}
	unlink(uds_fn);
	sock = plcListenServer("unix", uds_fn);
	if (sock == -1) {
		plc_elog(ERROR, "system call socket() fails: %s", strerror(errno));
	}

	/*
	 * The path owner should be generally the uid, but we are not 100% sure
	 * about this for current/future backends, so we still use environment
	 * variable, instead of extracting them via reading the owner of the path.
	 */

	/* Get executor uid: for permission of the unix domain socket file. */
	if (stand_alone_uds != NULL) {
		qe_uid = getuid();
	} else {
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
	}
	/* Get executor gid: for permission of the unix domain socket file. */
	if (stand_alone_uds != NULL){
		qe_gid = getgid();
	} else {
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
	}
	/* Change ownership & permission for the file for unix domain socket so
	 * code on the QE side could access it and clean up it later.
	 */
	if (chown(uds_fn, qe_uid, qe_gid) < 0)
		plc_elog (ERROR, "Could not set ownership for file %s with owner %d, "
			"group %d: %s", uds_fn, qe_uid, qe_gid, strerror(errno));
	if (chmod(uds_fn, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) < 0) /* 0666*/
		plc_elog (ERROR, "Could not set permission for file %s: %s",
			         uds_fn, strerror(errno));
	/*
	 * Set passed UID from OS is currently disabled due to the container OS may different with host OS.
	 * Hence use passed CLIENT_UID may has some risks.
	 * */

	plc_elog(DEBUG1, "Listening via unix domain socket with file: %s", uds_fn);

	return sock;
}

/*
 * Start listener based on network environment setting
 */
int start_listener(const char *stand_alone_uds) {
	int sock = -1;
	char *use_container_network;

	use_container_network = getenv("USE_CONTAINER_NETWORK");
	if (use_container_network == NULL) {
		use_container_network = "false";
		plc_elog(WARNING, "USE_CONTAINER_NETWORK is not set, use default value \"no\".");
	}

	if (strcasecmp("true", use_container_network) == 0) {
		sock = start_listener_inet();
	} else if (strcasecmp("false", use_container_network) == 0){
		if ((geteuid() != 0 || getuid() != 0) && stand_alone_uds == NULL) {
			plc_elog(ERROR, "Must run as root and then downgrade to usual user in container mode.");
		}
		sock = start_listener_ipc(stand_alone_uds);
	} else {
		plc_elog(ERROR, "USE_CONTAINER_NETWORK is set to wrong value '%s'", use_container_network);
	}

	return sock;
}

/*
 * Function waits for the socket to accept connection for finite amount of time
 * and errors out when the timeout is reached and no client connected
 */
void connection_wait(int sock) {
	struct timeval timeout;
	int rv;
	fd_set fdset;

	FD_ZERO(&fdset);    /* clear the set */
	FD_SET(sock, &fdset); /* add our file descriptor to the set */
	timeout.tv_sec = TIMEOUT_SEC;
	timeout.tv_usec = 0;

	rv = select(sock + 1, &fdset, NULL, NULL, &timeout);
	if (rv == -1) {
		plc_elog(ERROR, "Failed to select() socket: %s", strerror(errno));
	}
	if (rv == 0) {
		plc_elog(ERROR, "Socket timeout - no client connected within %d "
			"seconds", TIMEOUT_SEC);
	}
}

/*
 * Function accepts the connection and initializes structure for it
 */
plcConn *connection_init(int sock) {
	socklen_t raddr_len;
	struct sockaddr_in raddr;
	struct timeval tv;
	int connection;
	plcConn *plcconn;

	raddr_len = sizeof(raddr);
	connection = accept(sock, (struct sockaddr *) &raddr, &raddr_len);
	if (connection == -1) {
		plc_elog(ERROR, "failed to accept connection: %s", strerror(errno));
	}

	/* Set socket receive timeout to 500ms */
	tv.tv_sec = 0;
	tv.tv_usec = 500000;
	setsockopt(connection, SOL_SOCKET, SO_RCVTIMEO, (char *) &tv, sizeof(struct timeval));

	plcconn = (plcConn*) palloc(sizeof(plcConn));

	plcConnInit(plcconn);

	plcconn->sock = connection;

	return plcconn;
}

/*
 * Start computing unit server
 */
plcConn *start_server(const char *stand_alone_uds) {
	int sock;
	sock = start_listener(stand_alone_uds);
	if (sock == -1) {
		plc_elog(ERROR, "Cannot start listener %s", strerror(errno));
	}
	connection_wait(sock);
	return connection_init(sock);
}

