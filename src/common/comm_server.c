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
#include <unistd.h>

#include "comm_channel.h"
#include "comm_utils.h"
#include "comm_connectivity.h"
#include "comm_server.h"
#include "messages/messages.h"

/*
 * Functoin binds the socket and starts listening on it: tcp
 */
int start_listener_inet() {
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

    return sock;
}

/*
 * Functoin binds the socket and starts listening on it: unix domain socket.
 */
int start_listener_ipc() {
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
		lprintf(ERROR, "Cannot delete the file for unix domain socket connection: %s", uds_fn);

    if (bind(sock, (const struct sockaddr *)&addr, sizeof(addr)) == -1) {
        lprintf(ERROR, "Cannot bind the addr: %s", strerror(errno));
    }

	pfree(uds_fn);

#ifdef _DEBUG_CLIENT
    int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0){
        lprintf(ERROR, "setsockopt(SO_REUSEADDR) failed");
    }
#endif
    if (listen(sock, 10) == -1) {
        lprintf(ERROR, "Cannot listen the socket: %s", strerror(errno));
    }

    return sock;
}

/*
 * Fuction waits for the socket to accept connection for finite amount of time
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
        lprintf(ERROR, "Socket timeout - no client connected within %d seconds", TIMEOUT_SEC);
    }
}

/*
 * Function accepts the connection and initializes structure for it
 */
plcConn* connection_init(int sock) {
    socklen_t          raddr_len;
    struct sockaddr_in raddr;
    int                connection;

    raddr_len  = sizeof(raddr);
    connection = accept(sock, (struct sockaddr *)&raddr, &raddr_len);
    if (connection == -1) {
        lprintf(ERROR, "failed to accept connection: %s", strerror(errno));
    }

    return plcConnInit(connection);
}

/*
 * The loop of receiving commands from the Greenplum process and processing them
 */
void receive_loop( void (*handle_call)(plcMsgCallreq*, plcConn*), plcConn* conn) {
    plcMessage *msg;
    int res = 0;

    res = plcontainer_channel_receive(conn, &msg);
    if (res < 0) {
        lprintf(ERROR, "Error receiving data from the backend, %d", res);
        return;
    }
    if (msg->msgtype != MT_PING) {
        lprintf(ERROR, "First received message should be 'ping' message, got '%c' instead", msg->msgtype);
        return;
    } else {
        res = plcontainer_channel_send(conn, msg);
        if (res < 0) {
            lprintf(ERROR, "Cannot send 'ping' message response");
            return;
        }
    }
    pfree(msg);

    while (1) {
        res = plcontainer_channel_receive(conn, &msg);
        
        if (res == -3) {
            lprintf(NOTICE, "Backend must have closed the connection");
            break;
        }
        if (res < 0) {
            lprintf(ERROR, "Error receiving data from the backend, %d", res);
            break;
        }

        switch (msg->msgtype) {
            case MT_CALLREQ:
                handle_call((plcMsgCallreq*)msg, conn);
                free_callreq((plcMsgCallreq*)msg, false, false);
                break;
            default:
                lprintf(ERROR, "received unknown message: %c", msg->msgtype);
        }
    }
}
