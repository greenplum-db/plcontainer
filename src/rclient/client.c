/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */
#include <stdlib.h>
#include <assert.h>

#include "common/comm_channel.h"
#include "common/comm_utils.h"
#include "common/comm_connectivity.h"
#include "common/comm_server.h"
#include "rcall.h"

int main(int argc UNUSED, char **argv UNUSED) {
	int sock;
	plcConn *conn;
	int status;

	sanity_check_client();

	set_signal_handlers();

	/* do not overwrite, if the CLIENT_NAME has already set */
	setenv("CLIENT_LANGUAGE", "rclient", 0);

	client_log_level = WARNING;

	// Bind the socket and start listening the port
	sock = start_listener();

	// Initialize R
	plc_elog(LOG, "Client start to listen execution");
	status = r_init();

	connection_wait(sock);
	conn = connection_init(sock);
	if (status == 0) {
		receive_loop(handle_call, conn);
	} else {
		plc_raise_delayed_error(conn);
	}

	plc_elog(LOG, "Client has finished execution");
	return 0;
}



