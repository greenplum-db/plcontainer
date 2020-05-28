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
Datum plcontainer_inline_handler(PG_FUNCTION_ARGS);

char *plc_top_strdup(const char *str);
void *top_palloc(size_t bytes);

extern bool is_plcontainer_for_k8s;

#endif /* PLC_PLCONTAINER_H */
