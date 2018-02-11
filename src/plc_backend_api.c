/*-----------------------------------------------------------------------------
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#include "plc_backend_api.h"
#include "plc_docker_api.h"
#include "common/comm_utils.h"

static PLC_FunctionEntriesData *CurrentBackend;

/* If backend api returns error, the string will include more details. */
char backend_error_message[256];

static PLC_FunctionEntriesData DockerBackend =
{
	plc_docker_create_container,
	plc_docker_start_container,
	plc_docker_kill_container,
	plc_docker_inspect_container,
	plc_docker_wait_container,
	plc_docker_delete_container,
};

/*
 * NOTE: Do not call plc_elog(>=ERROR, ...) in backend api code. Let the callers
 * handle according to the return value and error message string.
 */

void plc_backend_prepareImplementation(enum PLC_BACKEND_TYPE imptype) {
	/*
	 * Initialize plc backend implement handlers.
	 * Currenty plcontainer only supports the BACKEND_DOCKER type.
	 * Other possible backends include BACKEND_GARDEN, BACKEND_PROCESS, etc.
	 */
	switch (imptype) {
		case BACKEND_DOCKER:
			CurrentBackend = &DockerBackend;
			break;
		default:
			plc_elog(ERROR, "Unsupported plc backend type: %d", imptype);
	}
}

int plc_backend_create(runtimeConfEntry *conf, char **name, int container_slot, char **uds_dir) {
	if (CurrentBackend == NULL || CurrentBackend->create_backend == NULL) {
		snprintf(backend_error_message, sizeof(backend_error_message), "Fail to get interface for backend create");
		return -1;
	}

	return CurrentBackend->create_backend(conf, name, container_slot, uds_dir);
}

int plc_backend_start(const char *name) {
	if (CurrentBackend == NULL || CurrentBackend->start_backend == NULL) {
		snprintf(backend_error_message, sizeof(backend_error_message), "Fail to get interface for backend start");
		return -1;
	}

	return CurrentBackend->start_backend(name);
}

int plc_backend_kill(const char *name) {
	if (CurrentBackend == NULL || CurrentBackend->kill_backend == NULL) {
		snprintf(backend_error_message, sizeof(backend_error_message), "Fail to get interface for backend kill");
		return -1;
	}

	return CurrentBackend->kill_backend(name);
}

int plc_backend_inspect(const char *name, char **element, plcInspectionMode type) {
	if (CurrentBackend == NULL || CurrentBackend->inspect_backend == NULL) {
		snprintf(backend_error_message, sizeof(backend_error_message), "Fail to get interface for backend inspect");
		return -1;
	}

	return CurrentBackend->inspect_backend(name, element, type);
}

int plc_backend_wait(const char *name) {
	if (CurrentBackend == NULL || CurrentBackend->wait_backend == NULL) {
		snprintf(backend_error_message, sizeof(backend_error_message), "Fail to get interface for backend wait");
		return -1;
	}

	return CurrentBackend->wait_backend(name);
}

int plc_backend_delete(const char *name) {
	if (CurrentBackend == NULL || CurrentBackend->delete_backend == NULL) {
		snprintf(backend_error_message, sizeof(backend_error_message), "Fail to get interface for backend delete");
		return -1;
	}

	return CurrentBackend->delete_backend(name);
}
