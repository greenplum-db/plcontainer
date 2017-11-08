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
#include "utils/resowner.h"

MemoryContext pl_container_caller_context;

/* list of explicit subtransaction data */
List *explicit_subtransactions;

/* explicit subtransaction data */
typedef struct PLySubtransactionData
{
	MemoryContext oldcontext;
	ResourceOwner oldowner;
} PLySubtransactionData;


/* entrypoint for all plcontainer procedures */
Datum plcontainer_call_handler(PG_FUNCTION_ARGS);

#endif /* PLC_PLCONTAINER_H */
