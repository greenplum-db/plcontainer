/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#include "plc_backend_api.h"

#include "plc_docker_common.h"

PLC_FunctionEntriesData CurrentPLCImp;

void plc_prepareImplementation(enum PLC_BACKEND_TYPE imptype) {
    /* Initialize resource broker implement handlers. */
    CurrentPLCImp.connect = NULL;
    CurrentPLCImp.create = NULL;
    CurrentPLCImp.start = NULL;
    CurrentPLCImp.kill = NULL;
    CurrentPLCImp.inspect = NULL;
    CurrentPLCImp.wait = NULL;
    CurrentPLCImp.delete_backend = NULL;
    CurrentPLCImp.disconnect = NULL;

    switch (imptype) {
        case DOCKER_CONTAINER:
            plc_docker_init(&CurrentPLCImp);
            break;
        default:
            Assert(false);
    }
}

int plc_connect(void){
    return CurrentPLCImp.connect != NULL ? CurrentPLCImp.connect() : FUNC_RETURN_OK;
}

int plc_create(int sockfd, plcContainerConf *conf, char **name){
    return CurrentPLCImp.create != NULL ? CurrentPLCImp.create(sockfd, conf, name) : FUNC_RETURN_OK;
}

int plc_start(int sockfd, char *name){
    return CurrentPLCImp.start != NULL ? CurrentPLCImp.start(sockfd, name) : FUNC_RETURN_OK;
}

int plc_kill(int sockfd, char *name){
    return CurrentPLCImp.kill != NULL ? CurrentPLCImp.kill(sockfd, name) : FUNC_RETURN_OK;
}

int plc_inspect(int sockfd, char *name, int *port){
    return CurrentPLCImp.inspect != NULL ? CurrentPLCImp.inspect(sockfd, name, port) : FUNC_RETURN_OK;
}

int plc_wait(int sockfd, char *name){
    return CurrentPLCImp.wait != NULL ? CurrentPLCImp.wait(sockfd, name) : FUNC_RETURN_OK;
}

int plc_delete(int sockfd, char *name){
    return CurrentPLCImp.delete_backend != NULL ? CurrentPLCImp.delete_backend(sockfd, name) : FUNC_RETURN_OK;
}

int plc_disconnect(int sockfd){
    return CurrentPLCImp.disconnect != NULL ? CurrentPLCImp.disconnect(sockfd) : FUNC_RETURN_OK;
}
