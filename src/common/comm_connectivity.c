/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <poll.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <libgen.h>

#include "comm_utils.h"
#include "comm_connectivity.h"
#ifndef PLC_CLIENT
  #include "miscadmin.h"
#endif

static ssize_t plcSocketRecv(plcConn *conn, void *ptr, size_t len);

static ssize_t plcSocketSend(plcConn *conn, const void *ptr, size_t len);

static int plcBufferMaybeFlush(plcConn *conn, bool isForse);

static void plcBufferMaybeReset(plcConn *conn, int bufType);

static int plcBufferMaybeResize(plcConn *conn, int bufType, size_t bufAppend);

static void
plc_gettimeofday(struct timeval *tv)
{
	int retval;
	retval = gettimeofday(tv, NULL);
	if (retval < 0)
		plc_elog(ERROR, "Failed to get time: %s", strerror(errno));
}

/*
 *  Read data from the socket
 */
static ssize_t plcSocketRecv(plcConn *conn, void *ptr, size_t len) {
	ssize_t sz = 0;
	int time_count = 0;
	int intr_count = 0;
	struct timeval start_ts, end_ts;

	while((sz=recv(conn->sock, ptr, len, 0))<0) {
#ifndef PLC_CLIENT
		CHECK_FOR_INTERRUPTS();
#endif
		if (errno == EINTR && intr_count++ < 5)
			continue;
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			if (time_count==0) {
				plc_gettimeofday(&start_ts);
				time_count++;
			} else {
				plc_gettimeofday(&end_ts);
				if ((end_ts.tv_sec - start_ts.tv_sec) > conn->rx_timeout_sec) {
					plc_elog(ERROR, "rx timeout (%ds > %ds)",
						(int) (end_ts.tv_sec - start_ts.tv_sec), conn->rx_timeout_sec);
					return -1;
				}
			}
		} else {
			plc_elog(ERROR, "Failed to recv data: %s", strerror(errno));
		}
	}

	/* Log info if needed. */
	if (sz == 0) {
		plc_elog(LOG, "The peer has shut down the connection.");
	}

	return sz;
}

/*
 *  Write data to the socket
 */
static ssize_t plcSocketSend(plcConn *conn, const void *ptr, size_t len) {
	ssize_t sz;
	int n=0;
	while((sz=send(conn->sock, ptr, len, 0))==-1) {
#ifndef PLC_CLIENT
		CHECK_FOR_INTERRUPTS();
#endif
		if (errno == EINTR && n++ < 5)
			continue;
		plc_elog(ERROR, "Failed to send: %s", strerror(errno));
		break;
	}
	return sz;
}

/*
 * Function flushes the output buffer if it has reached a certain margin in
 * size or if the isForse parameter has passed to it
 *
 * Returns 0 on success, -1 on failure
 */
static int plcBufferMaybeFlush(plcConn *conn, bool isForse) {
	plcBuffer *buf = conn->buffer[PLC_OUTPUT_BUFFER];

	/*
	 * Flush the buffer if it has less than PLC_BUFFER_MIN_FREE of free space
	 * available or data size in the buffer is greater than initial buffer size
	 * or if we are forced to flush everything
	 */
	if (buf->bufSize - buf->pEnd < PLC_BUFFER_MIN_FREE
	    || buf->pEnd - buf->pStart > PLC_BUFFER_SIZE
	    || isForse) {
		// Flushing the data into channel
		while (buf->pStart < buf->pEnd) {
			int sent = 0;

			sent = plcSocketSend(conn,
			                     buf->data + buf->pStart,
			                     buf->pEnd - buf->pStart);
			if (sent < 0) {
				plc_elog(LOG, "plcBufferMaybeFlush: Socket write failed, send "
					"return code is %d, error message is '%s'",
					sent, strerror(errno));
				return -1;
			}
			buf->pStart += sent;
		}

		// After the flush we should consider resetting the buffer
		plcBufferMaybeReset(conn, PLC_OUTPUT_BUFFER);
	}

	return 0;
}

/*
 * Function resets the buffer from the current position to the beginning of
 * the buffer array if it has reached the middle of the buffer or the buffer
 * is empty
 *
 */
static void plcBufferMaybeReset(plcConn *conn, int bufType) {
	plcBuffer *buf = conn->buffer[bufType];

	// If the buffer has no data we can reset both pointers to 0
	if (buf->pStart == buf->pEnd) {
		buf->pStart = 0;
		buf->pEnd = 0;
	}

	/*
	 * If our start point in a buffer has passed half of its size, we need
	 * to move the data to the start of the buffer
	 */
	else if (buf->pStart > buf->bufSize / 2) {
	// memmove is more meaningful, but here memcpy is safe
		memcpy(buf->data, buf->data + buf->pStart, buf->pEnd - buf->pStart);
		buf->pEnd = buf->pEnd - buf->pStart;
		buf->pStart = 0;
	}
}

/*
 * Function checks whether we need to increase or decrease the size of the buffer.
 * Buffer grows if we want to insert more bytes than the amount of free space in
 * this buffer (given we leave PLC_BUFFER_MIN_FREE free bytes). Buffer shrinks
 * if we occupy less than 20% of its total space
 *
 * Returns 0 on success, -1 on failure
 */
static int plcBufferMaybeResize(plcConn *conn, int bufType, size_t bufAppend) {
	plcBuffer *buf = conn->buffer[bufType];
	int dataSize;
	int newSize;
	char *newBuffer = NULL;
	int isReallocated = 0;

	// Minimum buffer size required to hold the data
	dataSize = (buf->pEnd - buf->pStart) + (int) bufAppend + PLC_BUFFER_MIN_FREE;

	// If the amount of data buffer currently holds and plan to hold after the
	// next insert is less than 20% of the buffer size, and if we have
	// previously increased the buffer size, we shrink it
	if (dataSize < buf->bufSize / 5 && buf->bufSize > PLC_BUFFER_SIZE) {
		// Buffer size is twice as large as the data we need to hold, rounded
		// to the nearest PLC_BUFFER_SIZE bytes
		newSize = ((dataSize * 2) / PLC_BUFFER_SIZE + 1) * PLC_BUFFER_SIZE;
		newBuffer = (char *) PLy_malloc(newSize);
		if (newBuffer == NULL) {
			// shrink failed, should not be an error
			plc_elog(WARNING, "plcBufferMaybeFlush: Cannot allocate %d bytes "
				"for output buffer", newSize);
			return 0;
		}
		isReallocated = 1;
	}

		// If we don't have enough space in buffer to handle the amount of data we
		// want to put there - we should increase its size
	else if (buf->pEnd + (int) bufAppend > buf->bufSize - PLC_BUFFER_MIN_FREE) {
		// Growing the buffer we need to just hold all the data we receive
		newSize = (dataSize / PLC_BUFFER_SIZE + 1) * PLC_BUFFER_SIZE;
		newBuffer = (char *) PLy_malloc(newSize);
		if (newBuffer == NULL) {
			plc_elog(ERROR, "plcBufferMaybeGrow: Cannot allocate %d bytes for buffer",
				    newSize);
			return -1;
		}
		isReallocated = 1;
	}

	// If we have reallocated the buffer - copy the data over and free the old one
	if (isReallocated) {
		memcpy(newBuffer,
		       buf->data + buf->pStart,
		       (size_t) (buf->pEnd - buf->pStart));
		pfree(buf->data);
		buf->data = newBuffer;
		buf->pEnd = buf->pEnd - buf->pStart;
		buf->pStart = 0;
		buf->bufSize = newSize;
	}

	return 0;
}

/*
 * Append some data to the buffer. This function does not guarantee that the
 * data would be immediately sent, you have to forcefully flush buffer to
 * achieve this
 *
 * Returns 0 on success, -1 if failed
 */
int plcBufferAppend(plcConn *conn, char *srcBuffer, size_t nBytes) {
	int res = 0;
	plcBuffer *buf = conn->buffer[PLC_OUTPUT_BUFFER];

	// If we don't have enough space in the buffer to hold the data
	if (buf->bufSize - buf->pEnd < (int) nBytes) {

		// First thing to check - whether we can reset the data to the beginning
		// of the buffer, freeing up some space in the end of it
		plcBufferMaybeReset(conn, PLC_OUTPUT_BUFFER);

		// Second check - whether we need to flush the buffer as it holds much data
		res = plcBufferMaybeFlush(conn, false);
		if (res < 0)
			return res;

		// Third check - whether we need to resize our buffer after these manipulations
		res = plcBufferMaybeResize(conn,
		                           PLC_OUTPUT_BUFFER,
		                           nBytes);
		if (res < 0)
			return res;
	}

	// Appending data to the buffer
	memcpy(buf->data + buf->pEnd, srcBuffer, nBytes);
	buf->pEnd = buf->pEnd + nBytes;
	assert(buf->pEnd <= buf->bufSize);
	return 0;
}

/*
 * Read some data from the buffer. If buffer does not have enough data in it,
 * it will ask the socket to receive more data and put it into the buffer
 *
 * Returns 0 on success, -1 if failed
 */
int plcBufferRead(plcConn *conn, char *resBuffer, size_t nBytes) {
	plcBuffer *buf = conn->buffer[PLC_INPUT_BUFFER];
	int res = 0;

	res = plcBufferReceive(conn, nBytes);
	if (res == 0) {
		memcpy(resBuffer, buf->data + buf->pStart, nBytes);
		buf->pStart = buf->pStart + nBytes;
	}

	return res;
}

/*
 * Function checks whether we have nBytes bytes in the buffer. If not, it reads
 * the data from the socket. If the buffer is too small, it would be grown
 *
 * Returns 0 on success, -1 if failed.
 */
int plcBufferReceive(plcConn *conn, size_t nBytes) {
	int res = 0;
	plcBuffer *buf = conn->buffer[PLC_INPUT_BUFFER];

	// If we don't have enough data in the buffer already
	if (buf->pEnd - buf->pStart < (int) nBytes) {
		int nBytesToReceive;
		int recBytes;

		// First thing to consider - resetting the data in buffer to the beginning
		// freeing up the space in the end to receive the data
		plcBufferMaybeReset(conn, PLC_INPUT_BUFFER);

		// Second step - check whether we really need to resize the buffer after this
		res = plcBufferMaybeResize(conn, PLC_INPUT_BUFFER, nBytes);
		if (res < 0)
			return res;

		// When we sure we have enough space - receive the related data
		nBytesToReceive = (int) nBytes - (buf->pEnd - buf->pStart);
		while (nBytesToReceive > 0) {
			recBytes = plcSocketRecv(conn,
			                         buf->data + buf->pEnd,
			                         buf->bufSize - buf->pEnd);
			if (recBytes <= 0) {
				return -1;
			}
			buf->pEnd += recBytes;
			nBytesToReceive -= recBytes;
			assert(buf->pEnd <= buf->bufSize);
		}
	}

	return 0;
}

/*
 * Function forcefully flushes the buffer
 *
 * Returns 0 on success, -1 if failed
 */
int plcBufferFlush(plcConn *conn) {
	return plcBufferMaybeFlush(conn, true);
}

/*
 *  Initialize plcConn data structure and input/output buffers.
 *  For network connection, uds_fn means nothing.
 */
plcConn *plcConnInit(int sock) {
	plcConn *conn;

	// Initializing main structures
	conn = (plcConn *) PLy_malloc(sizeof(plcConn));
	conn->buffer[PLC_INPUT_BUFFER] = (plcBuffer *) PLy_malloc(sizeof(plcBuffer));
	conn->buffer[PLC_OUTPUT_BUFFER] = (plcBuffer *) PLy_malloc(sizeof(plcBuffer));

	// Initializing buffers
	conn->buffer[PLC_INPUT_BUFFER]->data = (char *) PLy_malloc(PLC_BUFFER_SIZE);
	conn->buffer[PLC_INPUT_BUFFER]->bufSize = PLC_BUFFER_SIZE;
	conn->buffer[PLC_INPUT_BUFFER]->pStart = 0;
	conn->buffer[PLC_INPUT_BUFFER]->pEnd = 0;
	conn->buffer[PLC_OUTPUT_BUFFER]->data = (char *) PLy_malloc(PLC_BUFFER_SIZE);
	conn->buffer[PLC_OUTPUT_BUFFER]->bufSize = PLC_BUFFER_SIZE;
	conn->buffer[PLC_OUTPUT_BUFFER]->pStart = 0;
	conn->buffer[PLC_OUTPUT_BUFFER]->pEnd = 0;

	// Initializing control parameters
	conn->sock = sock;

	return conn;
}

#ifndef PLC_CLIENT

/* A bit ugly. Maybe move pplan stuffs out of conn* later. */
extern void init_pplan_slots(plcConn *conn);

extern void deinit_pplan_slots(plcConn *conn);

/*
 *  Connect to the specified host of the localhost and initialize the plcConn
 *  data structure
 */
plcConn *plcConnect_inet(int port) {
	struct hostent *server;
	struct sockaddr_in raddr; /** Remote address */
	plcConn *result = NULL;
	struct timeval tv;

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		plc_elog(ERROR, "PLContainer: Cannot create socket: %s", strerror(errno));
		goto err_out1;
	}

	server = gethostbyname("localhost");
	if (server == NULL) {
		close(sock);
		plc_elog(ERROR, "PLContainer: Failed to call gethostbyname('localhost'):"
			" %s", hstrerror(h_errno));
		return NULL;
	}

	raddr.sin_family = AF_INET;
	memcpy(&((raddr).sin_addr.s_addr), (char *) server->h_addr_list[0],
	       server->h_length);

	raddr.sin_port = htons(port);
	if (connect(sock, (const struct sockaddr *) &raddr,
	            sizeof(raddr)) < 0) {
		char ipAddr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &(raddr.sin_addr), ipAddr, INET_ADDRSTRLEN);
		plc_elog(DEBUG1, "PLContainer: Failed to connect to %s: %s", ipAddr,
			    strerror(errno));
		goto err_out2;
	}

	/* FIXME: Do we need them? */
	/* Set socket receive timeout to 500ms */
	tv.tv_sec = 0;
	tv.tv_usec = 500000;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *) &tv, sizeof(struct timeval));

	result = plcConnInit(sock);
	init_pplan_slots(result);
	result->uds_fn = NULL;

	return result;

err_out2:
	close(sock);
err_out1:
	return NULL;
}

/*
 *  Connect to the specified host of the localhost and initialize the plcConn
 *  data structure
 */
plcConn *plcConnect_ipc(char *uds_fn) {
	plcConn *result = NULL;
	struct timeval tv;
	int sock;
	struct sockaddr_un raddr;

	if (strlen(uds_fn) >= sizeof(raddr.sun_path)) {
		plc_elog(ERROR, "PLContainer: The path for unix domain socket "
			"connection is too long: %s", uds_fn);
		return NULL;
	}

	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		plc_elog(ERROR, "PLContainer: Cannot create unix domain socket: %s",
			    strerror(errno));
		return NULL;
	}

	memset(&raddr, 0, sizeof(raddr));

	raddr.sun_family = AF_UNIX;
	strcpy(raddr.sun_path, uds_fn);

	if (connect(sock, (const struct sockaddr *) &raddr,
	            sizeof(raddr)) < 0) {
		plc_elog(DEBUG1, "PLContainer: Failed to connect to %s: %s",
			    uds_fn, strerror(errno));
		goto err_out;
	}

	/* Set socket receive timeout to 500ms */
	tv.tv_sec = 0;
	tv.tv_usec = 500000;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *) &tv, sizeof(struct timeval));

	result = plcConnInit(sock);
	init_pplan_slots(result);
	result->uds_fn = plc_top_strdup(uds_fn);

	return result;

err_out:
	close(sock);
	return NULL;
}

/*
 *  Close the plcConn connection and deallocate the buffers
 */
void plcDisconnect(plcConn *conn) {
	char *uds_fn;

	if (conn != NULL) {
		close(conn->sock);

		uds_fn = conn->uds_fn;
		if (uds_fn != NULL) {
			unlink(uds_fn);
			rmdir(dirname(uds_fn));
			pfree(uds_fn);
			conn->uds_fn = NULL;
		}

		pfree(conn->buffer[PLC_INPUT_BUFFER]->data);
		pfree(conn->buffer[PLC_OUTPUT_BUFFER]->data);
		pfree(conn->buffer[PLC_INPUT_BUFFER]);
		pfree(conn->buffer[PLC_OUTPUT_BUFFER]);
		conn->buffer[PLC_INPUT_BUFFER] = NULL;
		conn->buffer[PLC_OUTPUT_BUFFER] = NULL;
		deinit_pplan_slots(conn);
		pfree(conn);
	}
	return;
}

#endif
