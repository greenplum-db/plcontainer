/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016, Pivotal.
 *
 *------------------------------------------------------------------------------
 */

#ifndef PLC_PYERROR_H
#define PLC_PYERROR_H

#include "common/comm_connectivity.h"

// Raising exception to the backend
void raise_execution_error(const char *format, ...);

void plc_raise_delayed_error(void);

void *plc_error_callback(void);

#endif /* PLC_PYERROR_H */
