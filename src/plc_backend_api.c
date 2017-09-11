/*-----------------------------------------------------------------------------
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#include "plc_backend_api.h"

#include "plc_docker_common.h"

static PLC_FunctionEntriesData CurrentPLCImp;

void plc_backend_prepareImplementation(enum PLC_BACKEND_TYPE imptype) {
    /* 
     * Initialize plc backend implement handlers. 
     * Currenty plcontainer only support BACKEND_DOCKER type,
     * it will support BACKEND_GARDEN, BACKEND_PROCESS in the future.
     */
    memset(&CurrentPLCImp, 0, sizeof(PLC_FunctionEntriesData));
    switch (imptype) {
        case BACKEND_DOCKER:
            plc_docker_init(&CurrentPLCImp);
            break;
        default:
            elog(ERROR, "Unsupported plc backend type: %d", imptype);
    }
}

int plc_backend_connect(void){
    return CurrentPLCImp.connect != NULL ? CurrentPLCImp.connect() : FUNC_RETURN_OK;
}

int plc_backend_create(int sockfd, plcContainerConf *conf, char **name, int container_slot){
    return CurrentPLCImp.create != NULL ? CurrentPLCImp.create(sockfd, conf, name, container_slot) : FUNC_RETURN_OK;
}

int plc_backend_start(int sockfd, char *name){
    return CurrentPLCImp.start != NULL ? CurrentPLCImp.start(sockfd, name) : FUNC_RETURN_OK;
}

int plc_backend_kill(int sockfd, char *name){
    return CurrentPLCImp.kill != NULL ? CurrentPLCImp.kill(sockfd, name) : FUNC_RETURN_OK;
}

int plc_backend_inspect(int sockfd, char *name, char **element, plcInspectionMode type){
    return CurrentPLCImp.inspect != NULL ? CurrentPLCImp.inspect(sockfd, name, element, type) : FUNC_RETURN_OK;
}

int plc_backend_wait(int sockfd, char *name){
    return CurrentPLCImp.wait != NULL ? CurrentPLCImp.wait(sockfd, name) : FUNC_RETURN_OK;
}

int plc_backend_delete(int sockfd, char *name){
    return CurrentPLCImp.delete_backend != NULL ? CurrentPLCImp.delete_backend(sockfd, name) : FUNC_RETURN_OK;
}

int plc_backend_disconnect(int sockfd){
    return CurrentPLCImp.disconnect != NULL ? CurrentPLCImp.disconnect(sockfd) : FUNC_RETURN_OK;
}
