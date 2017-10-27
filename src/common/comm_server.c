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

#include "comm_channel.h"
#include "comm_utils.h"
#include "comm_connectivity.h"
#include "comm_server.h"
#include "messages/messages.h"

/* For unix domain socket connection only. */
static char *uds_client_fn;

/*
 * Function binds the socket and starts listening on it: tcp
 */
static int start_listener_inet() {
    struct sockaddr_in addr;
    int                sock;

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
#ifdef _DEBUG_CLIENT
    int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0){
        lprintf(ERROR, "setsockopt(SO_REUSEADDR) failed");
    }
#endif
    if (listen(sock, 10) == -1) {
        lprintf(ERROR, "Cannot listen the socket: %s", strerror(errno));
    }

	lprintf(DEBUG1, "Listening via network with port: %d", SERVER_PORT);

    return sock;
}

/*
 * Function binds the socket and starts listening on it: unix domain socket.
 */
static int start_listener_ipc(char **puds_fn) {
    struct sockaddr_un addr;
    int                sock;
	char              *uds_fn;
	int                sz;

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

	/* So that QE can access this socket file.
	 * FIXME: Is there solution or is it necessary to set less permission?
	 * Dynamically create uid same as that in GPDB in container?
	 */
	chmod(uds_fn, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH); /* 0666*/

	/* Save it since we need to unlink the file when container process exits. */
	if (puds_fn != NULL) {
		unlink(*puds_fn);
		pfree(*puds_fn);
	}
	*puds_fn = uds_fn;

#ifdef _DEBUG_CLIENT
    int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0){
        lprintf(ERROR, "setsockopt(SO_REUSEADDR) failed");
    }
#endif
    if (listen(sock, 10) == -1) {
        lprintf(ERROR, "Cannot listen the socket: %s", strerror(errno));
    }

	lprintf(DEBUG1, "Listening via unix domain socket with file: %s", uds_fn);

    return sock;
}

static void atexit_cleanup_udsfile()
{
	if (uds_client_fn != NULL) {
		unlink(uds_client_fn);
		pfree(uds_client_fn);
	}
}

int start_listener()
{
	int sock;

	char* network;
	if(getenv("USE_NETWORK") == NULL){
		network = "no";
		lprintf(WARNING, "USE_NETWORK is not set, use default value \"no\".");
	} else {
		network = getenv("USE_NETWORK");
	}
	if (strcasecmp("true", network) == 0 || strcasecmp("yes", network) == 0) {
		sock = start_listener_inet();
	} else {
		sock = start_listener_ipc(&uds_client_fn);
		atexit(atexit_cleanup_udsfile);
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
