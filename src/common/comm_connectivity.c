/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common/comm_connectivity.h"
#include "common/comm_dummy.h"

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

int plcBufferInit(plcBuffer *buffer)
{
	buffer->data = (char *)txn_palloc(PLC_BUFFER_SIZE);
	buffer->pStart = 0;
	buffer->pEnd = 0;
	buffer->bufSize = PLC_BUFFER_SIZE;
	return 0;
}

void plcBufferRelease(plcBuffer *buffer)
{
	if (buffer->data) {
		pfree(buffer->data);
	}
	buffer->data = NULL;
	buffer->pStart = 0;
	buffer->pEnd = 0;
	buffer->bufSize = 0;
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
#ifndef PLC_SERVER
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
#ifndef PLC_SERVER
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
	plcBuffer *buf = &conn->buffer[PLC_OUTPUT_BUFFER];

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
	plcBuffer *buf = &conn->buffer[bufType];

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
// TODO: alloc buffer memory in the transaction level memory context, instead of
// TopMemoryContext
static int plcBufferMaybeResize(plcConn *conn, int bufType, size_t bufAppend) {
	plcBuffer *buf = &conn->buffer[bufType];
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
		newBuffer = (char *) txn_palloc(newSize);
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
		newBuffer = (char *) txn_palloc(newSize);
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
	plcBuffer *buf = &conn->buffer[PLC_OUTPUT_BUFFER];

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
	plcBuffer *buf = &conn->buffer[PLC_INPUT_BUFFER];
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
	plcBuffer *buf = &conn->buffer[PLC_INPUT_BUFFER];

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
 */
void plcConnInit(plcConn *conn) {
	memset(conn, 0, sizeof(*conn));
	plcBufferInit(&conn->buffer[PLC_INPUT_BUFFER]);
	plcBufferInit(&conn->buffer[PLC_OUTPUT_BUFFER]);

	// Initializing control parameters
	conn->sock = -1;
}

static void plcDisconnect_(plcConn *conn);

/*
 *  Close the plcConn connection and deallocate the buffers
 */
void plcDisconnect(plcConn *conn) {
    plcDisconnect_(conn);
    pfree(conn);
}
static void plcDisconnect_(plcConn *conn) {
	close(conn->sock);
	pfree(conn->buffer[PLC_INPUT_BUFFER].data);
	pfree(conn->buffer[PLC_OUTPUT_BUFFER].data);
}

void plcContextInit(plcContext *ctx)
{
	plcConnInit(&ctx->conn);
	init_pplan_slots(ctx);
	ctx->service_address = NULL;
}

/*
 * This function only release buffers and reset plan array.
 * NOTE: socket fd keeps open and service address is still valid.
 * We INTEND to reuse connection to the container.
 */
void plcReleaseContext(plcContext *ctx)
{
	int n = 2;
	while (--n>=0)
		plcBufferRelease(&ctx->conn.buffer[n]);
	deinit_pplan_slots(ctx);
}

/*
 *  This function is used for re-init buffer and plan slot
 *  when a new query coming and connection is resued.
 */

void plcContextReset(plcContext *ctx)
{
	plcBufferInit(&ctx->conn.buffer[PLC_INPUT_BUFFER]);
	plcBufferInit(&ctx->conn.buffer[PLC_OUTPUT_BUFFER]);
	init_pplan_slots(ctx);
}

/*
 * clearup the container connection context
 */
void plcFreeContext(plcContext *ctx)
{
	close(ctx->conn.sock);
	plcReleaseContext(ctx);
	pfree(ctx->service_address);
	pfree(ctx);
}
