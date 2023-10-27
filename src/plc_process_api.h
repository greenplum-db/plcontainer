/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */


#ifndef PLC_PROCESS_API_H
#define PLC_PROCESS_API_H

#include "plc_configuration.h"

int plc_process_create_container(runtimeConfEntry *conf, char **name, int container_slot, char **uds_dir);

int plc_process_start_container(const char *name);

int plc_process_kill_container(const char *name);

int plc_process_inspect_container(const char *name, char **element, plcInspectionMode type);

int plc_process_wait_container(const char *name);

int plc_process_delete_container(const char *name);

#endif /* PLC_PROCESS_API_H */
