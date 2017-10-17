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
    int      sock;
    plcConn* conn;
    int      status;

    assert(sizeof(char) == 1);
    assert(sizeof(short) == 2);
    assert(sizeof(int) == 4);
    assert(sizeof(long long) == 8);
    assert(sizeof(float) == 4);
    assert(sizeof(double) == 8);

    setbuf(stdout, NULL);
    time_t rawtime;
    struct tm * timeinfo;

    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    lprintf(LOG, "Client has started execution at %s", asctime (timeinfo));
    sock = start_listener();

    // Initialize Python
    status = python_init();

    #ifdef _DEBUG_CLIENT
        // In debug mode we have a cycle of connections with infinite wait time
        while (true) {
            conn = connection_init(sock);
            if (status == 0) {
                receive_loop(handle_call, conn);
            } else {
                plc_raise_delayed_error();
                return -1;
            }
        }
    #else
        // In release mode we wait for incoming connection for limited time
        // and the client works for a single connection only
        connection_wait(sock);
        conn = connection_init(sock);
        if (status == 0) {
            receive_loop(handle_call, conn);
        } else {
            plc_raise_delayed_error();
        }
    #endif

    lprintf(NOTICE, "Client has finished execution");
    return 0;
}
