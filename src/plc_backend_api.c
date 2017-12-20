/*-----------------------------------------------------------------------------
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#include "plc_backend_api.h"

static PLC_FunctionEntriesData CurrentPLCImp;

char api_error_message[256];

/*
 * NOTE: Do not call elog(>=ERROR, ...) in backend api code. Let the callers
 * handle according to the return value and error message string.
 */

static void plc_docker_init(PLC_FunctionEntries entries) {
	entries->create = plc_docker_create_container;
	entries->start = plc_docker_start_container;
	entries->kill = plc_docker_kill_container;
	entries->inspect = plc_docker_inspect_container;
	entries->wait = plc_docker_wait_container;
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

int plc_backend_create(runtimeConf *conf, char **name, int container_slot, char **uds_dir) {
	return CurrentPLCImp.create != NULL ? CurrentPLCImp.create(conf, name, container_slot, uds_dir) : 0;
}

int plc_backend_start(const char *name) {
	return CurrentPLCImp.start != NULL ? CurrentPLCImp.start(name) : 0;
}

int plc_backend_kill(const char *name) {
	return CurrentPLCImp.kill != NULL ? CurrentPLCImp.kill(name) : 0;
}

int plc_backend_inspect(const char *name, char **element, plcInspectionMode type) {
	return CurrentPLCImp.inspect != NULL ? CurrentPLCImp.inspect(name, element, type) : 0;
}

int plc_backend_wait(const char *name) {
	return CurrentPLCImp.wait != NULL ? CurrentPLCImp.wait(name) : 0;
}

int plc_backend_delete(const char *name) {
	return CurrentPLCImp.delete_backend != NULL ? CurrentPLCImp.delete_backend(name) : 0;
}
