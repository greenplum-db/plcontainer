/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016, Pivotal.
 *
 *------------------------------------------------------------------------------
 */

#ifndef PLC_PLCONTAINER_H
#define PLC_PLCONTAINER_H

#include "fmgr.h"

/* entrypoint for all plcontainer procedures */
Datum plcontainer_call_handler(PG_FUNCTION_ARGS);

#endif /* PLC_PLCONTAINER_H */
