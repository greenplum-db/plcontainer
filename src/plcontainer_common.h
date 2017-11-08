/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016, Pivotal.
 *
 *------------------------------------------------------------------------------
 */
#ifndef PLC_PLCONTAINER_COMMON_H_
#define PLC_PLCONTAINER_COMMON_H_

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

extern void * plcontainer_malloc(size_t bytes);
extern void plcontainer_free(void *ptr);



#endif /* PLC_PLCONTAINER_COMMON_H_ */
