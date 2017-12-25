/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */
#ifndef PLC_PLCONTAINER_COMMON_H_
#define PLC_PLCONTAINER_COMMON_H_

#include "fmgr.h"
#include "utils/hsearch.h"
#include "utils/resowner.h"

extern HTAB *rumtime_conf_table;
extern MemoryContext pl_container_caller_context;

/* list of explicit subtransaction data */
List *explicit_subtransactions;

/* explicit subtransaction data */
typedef struct PLySubtransactionData {
	MemoryContext oldcontext;
	ResourceOwner oldowner;
} PLySubtransactionData;

extern void *plcontainer_malloc(size_t bytes);

extern void plcontainer_free(void *ptr);


#endif /* PLC_PLCONTAINER_COMMON_H_ */
