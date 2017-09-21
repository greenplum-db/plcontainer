/*-----------------------------------------------------------------------------
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#include "plc_backend_api.h"

static PLC_FunctionEntriesData CurrentPLCImp;

char api_error_message[256];

static void plc_docker_init(PLC_FunctionEntries entries){
    entries->create         = plc_docker_create_container;
    entries->start          = plc_docker_start_container;
    entries->kill           = plc_docker_kill_container;
    entries->inspect        = plc_docker_inspect_container;
    entries->wait           = plc_docker_wait_container;
    entries->delete_backend = plc_docker_delete_container;
}

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

int plc_backend_create(plcContainerConf *conf, char **name, int container_slot){
    return CurrentPLCImp.create != NULL ? CurrentPLCImp.create(conf, name, container_slot) : 0;
}

int plc_backend_start(char *name){
    return CurrentPLCImp.start != NULL ? CurrentPLCImp.start(name) : 0;
}

int plc_backend_kill(char *name){
    return CurrentPLCImp.kill != NULL ? CurrentPLCImp.kill(name) : 0;
}

int plc_backend_inspect(char *name, char **element, plcInspectionMode type){
    return CurrentPLCImp.inspect != NULL ? CurrentPLCImp.inspect(name, element, type) : 0;
}

int plc_backend_wait(char *name){
    return CurrentPLCImp.wait != NULL ? CurrentPLCImp.wait(name) : 0;
}

int plc_backend_delete(char *name){
    return CurrentPLCImp.delete_backend != NULL ? CurrentPLCImp.delete_backend(name) : 0;
}
