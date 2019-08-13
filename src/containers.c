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
  #include<stdarg.h>  
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
#include "plc_backend_api.h"



typedef struct {
	char *runtimeid;
	plcContext *ctx;
} container_t;

#define MAX_CONTAINER_NUMBER 10

static container_t containers[MAX_CONTAINER_NUMBER];
static int containers_size = 0;

static int check_runtime_id(const char *id);
static char *get_coordinator_address(void);
static int get_new_container_from_coordinator(const char *runtime_id, plcContext *ctx);
static plcContext *get_new_container_ctx(const char *runtime_id);
static int init_container_connection(plcContext *ctx);
static void insert_container_ctx(const char *runtime_id, plcContext *ctx, int slot);

static void insert_container_ctx(const char *runtime_id, plcContext *ctx, int slot)
{
	containers[slot].runtimeid = plc_top_strdup(runtime_id);
	containers[slot].ctx = ctx;
}

plcContext *get_container_context(const char *runtime_id)
{
	int i;
	plcContext *newCtx = NULL;

#ifndef PLC_PG
	SIMPLE_FAULT_INJECTOR("plcontainer_before_container_connected");
#endif
	for (i = 0; i < containers_size; i++)
		if (strcmp(containers[i].runtimeid, runtime_id) == 0)
			return containers[i].ctx;

	i = containers_size+1;
	/* No connection available */
	if (i >= MAX_CONTAINER_NUMBER)
		plc_elog(ERROR, "too many plcontainer runtime in a session");

	/* Container Context could not be NULL, otherwise an elog(ERROR) will be thrown out */
	newCtx = get_new_container_ctx(runtime_id);

	insert_container_ctx(runtime_id, newCtx, i);
	containers_size++;
	return newCtx;
}
// return a plcContext that connected to the server
// TODO: complete impl
static plcContext *get_new_container_ctx(const char *runtime_id)
{
	plcContext *ctx = NULL;
	int res = 0;

	/* TODO: the initialize refactor? */
	ctx = (plcContext*) palloc0(sizeof(plcContext));
	plcContextInit(ctx);

	res = get_new_container_from_coordinator(runtime_id, ctx);

	if (res != 0){
		/* TODO: Using errors instead of elog */
		elog(ERROR, "Cannot find an available container");
	}

	res = init_container_connection(ctx);

	if (res != 0)
	{
		/* TODO: Using errors instead of elog */
		plcFreeContext(ctx);
		elog(ERROR, "Cannot connect to container server");
	}

	return ctx;
}

/* TODO: using emun to instead of int? */
static int get_new_container_from_coordinator(const char *runtime_id, plcContext *ctx)
{
	plcConn *conn; // a connection used to connect to coordinator
	int ret = 0;
	(void)runtime_id;
	(void)ctx;
	conn = (plcConn*) palloc0(sizeof(plcConn));

	/* current only uds is supported */
	conn->sock = plcDialToServer("unix", get_coordinator_address());

	/* send message to coordinator */

	/* release the connection */
	plcDisconnect(conn);

	return ret;
}

// This function is called by plcontainer only.
// when socket fd of plcConn is ready, make a shakehand to the server
// returns 0 if successfully, otherwise -1 if failed.
static int init_container_connection(plcContext *ctx)
{
	plcMsgPing *mping = NULL;
	plcConn *conn = (plcConn *)ctx;
	unsigned int sleepus = 25000;
	unsigned int sleepms = 0;

	mping = (plcMsgPing*) palloc(sizeof(plcMsgPing));
	mping->msgtype = MT_PING;

	while (sleepms < CONTAINER_CONNECT_TIMEOUT_MS) {
		int res = 0;
		plcMessage *mresp = NULL;

		res = plcontainer_channel_send(conn, (plcMessage *) mping);
		if (res == 0)
		{
			res = plcontainer_channel_receive(conn, &mresp, MT_PING_BIT);
			if (mresp != NULL)
				pfree(mresp);

			if (res == 0)
			{
				pfree(mping);
				return 0;
			}
			else
			{
				plc_elog(DEBUG1, "Failed to receive pong from client.");
			}
		} else {
			plc_elog(DEBUG1, "Failed to send ping to client.");
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
			 * configurations, we allow reconnect here since that does not seemi
			 * to harm the normal case although since client will just accept()
			 * the tcp connection once reconnect should never happen.
			 */


		/* TODO: using pg_sleep()? */
		usleep(sleepus);
		plc_elog(DEBUG1, "Waiting for %u ms for before reconnecting", sleepus / 1000);
		sleepms += sleepus / 1000;
		sleepus = sleepus >= 200000 ? 200000 : sleepus * 2;
	}

	pfree(mping);
	return -1;
}

// TODO: read shm to get the address of coordinator
static char *get_coordinator_address(void) {
	return NULL;
}

void reset_containers() {
	int i;

	for (i = 0; i < containers_size; i++) {
		if (containers[i].runtimeid != NULL) {
			/*
				* Disconnect at first so that container has chance to exit gracefully.
				* When running code coverage for client code, client needs to
				* have chance to flush the gcda files thus direct kill-9 is not
				* proper.
				*/
			plcContext *ctx = containers[i].ctx;
			char *runtimeid = containers[i].runtimeid;
			containers[i].runtimeid = NULL;
			containers[i].ctx = NULL;
			if (ctx)
				plcFreeContext(ctx);
			pfree(runtimeid);
		}
	}
	containers_size = 0;
	memset((void *)containers, 0, sizeof(containers));
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
	runtime_id = (char *) palloc(last - first + 1 + 1);
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
