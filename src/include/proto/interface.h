#ifndef __INTERFACE_H__
#define __INTERFACE_H__


#ifdef __cplusplus
extern "C"
{

#include "common/messages/messages.h"
#include "common/comm_connectivity.h"
#include "plc/message_fns.h"
#include "plc/containers.h"

#include "postgres.h"
#include "utils/builtins.h"
#include "catalog/pg_proc.h"
#include "utils/guc.h"
#include "access/transam.h"
#include "utils/array.h"
#include "utils/syscache.h"
#include "utils/array.h"

#endif

// C interface definition
Datum plcontainer_function_handler(FunctionCallInfo fcinfo, plcProcInfo *proc, MemoryContext function_cxt); 

#ifdef __cplusplus
}
#endif



#endif
