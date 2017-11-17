/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */
#include <errno.h>
#include <netinet/ip.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

#include "comm_channel.h"
#include "comm_utils.h"
#include "comm_connectivity.h"
#include "comm_server.h"
#include "messages/messages.h"

/*
 * Function binds the socket and starts listening on it: tcp
 */
static int start_listener_inet() {
    struct sockaddr_in addr;
    int                sock;

	/* FIXME: We might want to downgrade from root to normal user in container.*/

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        lprintf(ERROR, "system call socket() fails: %s", strerror(errno));
    }

    addr = (struct sockaddr_in){
        .sin_family = AF_INET,
        .sin_port   = htons(SERVER_PORT),
        .sin_addr = {.s_addr = INADDR_ANY},
    };
    if (bind(sock, (const struct sockaddr *)&addr, sizeof(addr)) == -1) {
        lprintf(ERROR, "Cannot bind the port: %s", strerror(errno));
    }

    if (listen(sock, 10) == -1) {
        lprintf(ERROR, "Cannot listen the socket: %s", strerror(errno));
    }

	lprintf(DEBUG1, "Listening via network with port: %d", SERVER_PORT);

    return sock;
}

/*
 * Function binds the socket and starts listening on it: unix domain socket.
 */
static int start_listener_ipc() {
    struct sockaddr_un addr;
    int                sock;
	char              *uds_fn;
	int                sz;
	char              *env_str, *endptr;
	uid_t              srv_uid, clt_uid;
	gid_t              srv_gid;
	long               val;

	/* filename: IPC_CLIENT_DIR + '/' + UDS_SHARED_FILE */
	sz = strlen(IPC_CLIENT_DIR) + 1 + MAX_SHARED_FILE_SZ + 1;
	uds_fn = pmalloc(sz);
	sprintf(uds_fn, "%s/%s", IPC_CLIENT_DIR, UDS_SHARED_FILE);
	if (strlen(uds_fn) >= sizeof(addr.sun_path)) {
		lprintf(ERROR, "PLContainer: The path for unix domain socket "
				"connection is too long: %s", uds_fn);
	}

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock == -1) {
        lprintf(ERROR, "system call socket() fails: %s", strerror(errno));
    }

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, uds_fn);

	unlink(uds_fn);
	if (access(uds_fn, F_OK) == 0)
		lprintf(ERROR, "Cannot delete the file for unix domain socket "
				"connection: %s", uds_fn);

    if (bind(sock, (const struct sockaddr *)&addr, sizeof(addr)) == -1) {
        lprintf(ERROR, "Cannot bind the addr: %s", strerror(errno));
    }

	/*
	 * The path owner should be generally the uid, but we are not 100% sure
	 * about this for current/future backends, so we still use environment
	 * variable, instead of extracting them via reading the owner of the path.
	 */
	if((env_str = getenv("EXECUTOR_UID")) == NULL)
		lprintf(ERROR, "EXECUTOR_UID is not set, something wrong on QE side");
	errno = 0;
	val = strtol(env_str, &endptr, 10);
	if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) ||
		(errno != 0 && val == 0) ||
		endptr == env_str ||
		*endptr != '\0') {
		lprintf(ERROR, "EXECUTOR_UID is wrong:'%s'", env_str);
	}
	srv_uid = val;

	/* Get gid */
	if((env_str = getenv("EXECUTOR_GID")) == NULL)
		lprintf(ERROR, "EXECUTOR_GID is not set, something wrong on QE side");
	errno = 0;
	val = strtol(env_str, &endptr, 10);
	if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) ||
		(errno != 0 && val == 0) ||
		endptr == env_str ||
		*endptr != '\0') {
		lprintf(ERROR, "EXECUTOR_GID is wrong:'%s'", env_str);
	}
	srv_gid = val;

	/* Change ownership & permission for the file for unix domain socket */
	if (chown(uds_fn, srv_uid, srv_gid) < 0)
		lprintf(ERROR, "Could not set ownership for file %s with owner %d, "
				"group %d: %s", uds_fn, srv_uid, srv_gid, strerror(errno));
	if (chmod(uds_fn, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH) < 0) /* 0666*/
		lprintf(ERROR, "Could not set permission for file %s: %s",
				uds_fn, strerror(errno));

    if (listen(sock, 10) == -1) {
        lprintf(ERROR, "Cannot listen the socket: %s", strerror(errno));
    }

	/*
	 * Now we need to use another uid that can not write the path for safety
	 * since the owner of the path is srv_uid generally.
	 */
	setuid(srv_uid+1);
	setgid(srv_gid+1);

	/*
	 * Note: It is said on some platforms that if it is a setuid program,
	 * it could setuid back to root if the real uid is root although this is not
	 * the case on Linux. So we checks getuid() here also.
	 */
	clt_uid = getuid();
	if (clt_uid == 0 || clt_uid == srv_uid || clt_uid != geteuid()) {
		close(sock);
		unlink(uds_fn);
		lprintf(ERROR, "New uid (%d) is wrong: (%d %d %d): %s\n",
				clt_uid, 0, srv_uid, geteuid(), strerror(errno));
		return -1;
	}

	lprintf(DEBUG1, "Listening via unix domain socket with file: %s", uds_fn);

    return sock;
}

int start_listener()
{
	int   sock;
	char* network;

	network = getenv("USE_NETWORK");
	if(network == NULL){
		network = "no";
		lprintf(WARNING, "USE_NETWORK is not set, use default value \"no\".");
	}

	if (strcasecmp("true", network) == 0 || strcasecmp("yes", network) == 0) {
		sock = start_listener_inet();
	} else {
		if (geteuid() != 0 || getuid() != 0) {
			lprintf(ERROR, "Must run as root and then downgrade to usual user.");
			return -1;
		}
		sock = start_listener_ipc();
	}

	return sock;
}

/*
 * Function waits for the socket to accept connection for finite amount of time
 * and errors out when the timeout is reached and no client connected
 */
void connection_wait(int sock) {
    struct timeval     timeout;
    int                rv;
    fd_set             fdset;

    FD_ZERO(&fdset);    /* clear the set */
    FD_SET(sock, &fdset); /* add our file descriptor to the set */
    timeout.tv_sec  = TIMEOUT_SEC;
    timeout.tv_usec = 0;

    rv = select(sock + 1, &fdset, NULL, NULL, &timeout);
    if (rv == -1) {
        lprintf(ERROR, "Failed to select() socket: %s", strerror(errno));
    }
    if (rv == 0) {
        lprintf(ERROR, "Socket timeout - no client connected within %d "
				"seconds", TIMEOUT_SEC);
    }
}

/*
 * Function accepts the connection and initializes structure for it
 */
plcConn* connection_init(int sock) {
    socklen_t          raddr_len;
    struct sockaddr_in raddr;
    struct timeval     tv;
    int                connection;

    raddr_len  = sizeof(raddr);
    connection = accept(sock, (struct sockaddr *)&raddr, &raddr_len);
    if (connection == -1) {
        lprintf(ERROR, "failed to accept connection: %s", strerror(errno));
    }

	/* Set socket receive timeout to 500ms */
    tv.tv_sec  = 0;
    tv.tv_usec = 500000;
    setsockopt(connection, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));

    return plcConnInit(connection);
}

/*
 * The loop of receiving commands from the Greenplum process and processing them
 */
void receive_loop( void (*handle_call)(plcMsgCallreq*, plcConn*), plcConn* conn) {
    plcMessage *msg;
    int res = 0;

    res = plcontainer_channel_receive(conn, &msg, MT_PING_BIT);
    if (res < 0) {
        lprintf(ERROR, "Error receiving data from the backend, %d", res);
        return;
    }

	res = plcontainer_channel_send(conn, msg);
	if (res < 0) {
		lprintf(ERROR, "Cannot send 'ping' message response");
		return;
    }
    pfree(msg);

    while (1) {
        res = plcontainer_channel_receive(conn, &msg, MT_CALLREQ_BIT);
        
        if (res < 0) {
            lprintf(ERROR, "Error receiving data from the peer: %d", res);
            break;
        }

		handle_call((plcMsgCallreq*)msg, conn);
		free_callreq((plcMsgCallreq*)msg, false, false);
    }
}
