/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#include <assert.h>
#include <errno.h>
#include <netinet/ip.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>

#include "common/comm_channel.h"
#include "common/comm_utils.h"
#include "common/comm_connectivity.h"
#include "common/comm_server.h"
#include "pycall.h"
#include "pyerror.h"

int main(int argc UNUSED, char **argv UNUSED) {
	int sock;
	plcConn *conn;
	int status;

	assert(sizeof(int8) == 1);
	assert(sizeof(int16) == 2);
	assert(sizeof(int32) == 4);
	assert(sizeof(uint32) == 4);
	assert(sizeof(int64) == 8);
	assert(sizeof(float4) == 4);
	assert(sizeof(float8) == 8);

	set_signal_handlers();

	setbuf(stdout, NULL);
	time_t rawtime;
	struct tm *timeinfo;

	time(&rawtime);
	timeinfo = localtime(&rawtime);
	/* do not overwrite, if the CLIENT_NAME has already set */
	setenv("CLIENT_LANGUAGE", "pythonclient", 0);

	sock = start_listener();
	lprintf(LOG, "Client has started execution at %s", asctime(timeinfo));

	// Initialize Python
	status = python_init();

	connection_wait(sock);
	conn = connection_init(sock);
	if (status == 0) {
		receive_loop(handle_call, conn);
	} else {
		plc_raise_delayed_error();
	}

	lprintf(LOG, "Client has finished execution");
	return 0;
}
