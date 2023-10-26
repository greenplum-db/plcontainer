/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#ifndef PLC_BACKEND_API_H
#define PLC_BACKEND_API_H

#include "postgres.h"

#include "plc_configuration.h"

extern char backend_error_message[256];

typedef int ( *PLC_FPTR_create)(runtimeConfEntry *conf, char **name, int container_slot, char **uds_dir);

typedef int ( *PLC_FPTR_start)(const char *name);

typedef int ( *PLC_FPTR_kill)(const char *name);

typedef int ( *PLC_FPTR_inspect)(const char *name, char **element, plcInspectionMode type);

typedef int ( *PLC_FPTR_wait)(const char *name);

typedef int ( *PLC_FPTR_delete)(const char *name);

struct PLC_FunctionEntriesData {
	PLC_FPTR_create create_backend;
	PLC_FPTR_start start_backend;
	PLC_FPTR_kill kill_backend;
	PLC_FPTR_inspect inspect_backend;
	PLC_FPTR_wait wait_backend;
	PLC_FPTR_delete delete_backend;
};

typedef struct PLC_FunctionEntriesData PLC_FunctionEntriesData;

enum PLC_BACKEND_TYPE {
	BACKEND_DOCKER = 0,
	BACKEND_GARDEN,   /* not implemented yet*/
	BACKEND_PROCESS,  /* not implemented yet*/
	UNIMPLEMENT_TYPE
};

extern enum PLC_BACKEND_TYPE CurrentBackendType;

void plc_backend_prepareImplementation(enum PLC_BACKEND_TYPE imptype);

/* interfaces for plc backend. */

int plc_backend_create(runtimeConfEntry *conf, char **name, int container_slot, char **uds_dir) __attribute__((warn_unused_result));

int plc_backend_start(const char *name) __attribute__((warn_unused_result));

int plc_backend_kill(const char *name) __attribute__((warn_unused_result));

int plc_backend_inspect(const char *name, char **element, plcInspectionMode type) __attribute__((warn_unused_result));

int plc_backend_wait(const char *name) __attribute__((warn_unused_result));

int plc_backend_delete(const char *name) __attribute__((warn_unused_result));

#endif /* PLC_BACKEND_API_H */
