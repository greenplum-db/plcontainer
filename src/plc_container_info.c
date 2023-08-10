#include <libxml/tree.h>
#include <libxml/parser.h>
#include <unistd.h>
#include <sys/stat.h>

#include "postgres.h"
#include "commands/resgroupcmds.h"

#include "cdb/cdbdisp_query.h"
#include "cdb/cdbdispatchresult.h"
#include "cdb/cdbvars.h"

#include "utils/builtins.h"
#include "utils/guc.h"
#include "libpq/libpq-be.h"
#include "libpq-fe.h"
#include "utils/acl.h"
#include "storage/ipc.h"
#include "storage/lwlock.h"
#include "storage/shmem.h"

#include "funcapi.h"

#include "common/comm_utils.h"
#include "common/comm_connectivity.h"
#include "plcontainer.h"
#include "plc_backend_api.h"
#include "plc_docker_api.h"
#include "plc_container_info.h"
#include "plc_configuration.h"

#if PG_VERSION_NUM >= 120000
#include "common/hashfn.h"
#else
#include "catalog/gp_segment_config.h"
#endif


PG_FUNCTION_INFO_V1(list_running_containers);

static HTAB *udf_container_id_map = NULL;
LWLock	   *plc_lw_lock;
static shmem_startup_hook_type prev_shmem_startup_hook = NULL;
static Size PLContainerShmemSize(void);
static UdfContainerIdMap* find_containerid_entry(const char *dockerid);
#define MAX_UDF_ENTRIES 256
#define PREFIX_CONTAINER_ID_LENGTH 32

Datum
list_running_containers(pg_attribute_unused() PG_FUNCTION_ARGS) {
	FuncCallContext *funcctx;
	int call_cntr;
	int max_calls;
	int res;
	TupleDesc tupdesc;
	AttInMetadata *attinmeta;
	struct jsonc_json_object *container_list = NULL;
	char *json_result;
	bool isFirstCall = true;

	char* backend_name = "default";
	if (PG_NARGS() == 1) {
		backend_name = text_to_cstring(PG_GETARG_TEXT_PP(0));
	}
	const plcBackend *backend = take_plcbackend_byname(backend_name);

	/* Init the container list in the first call and get the results back */
	if (SRF_IS_FIRSTCALL()) {
		MemoryContext oldcontext;
		int arraylen;
		int dbid;

		/* create a function context for cross-call persistence */
		funcctx = SRF_FIRSTCALL_INIT();

		/* switch to memory context appropriate for multiple function calls */
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		dbid = GpIdentity.segindex;

		res = plc_docker_list_container(&json_result, dbid, backend);
		if (res < 0) {
			plc_elog(ERROR, "Docker container list error: %s", backend_error_message);
		}

		/* no container running */
		if (strcmp(json_result, "[]") == 0) {
			funcctx->max_calls = 0;
		}

		container_list = json_tokener_parse(json_result);

		if (container_list == NULL) {
			plc_elog(ERROR, "Parse JSON object error, cannot get the containers summary");
		}

		arraylen = json_object_array_length(container_list);

		/* total number of containers to be returned, each array contains one container */
		funcctx->max_calls = (uint32) arraylen;

		/*
		 * prepare attribute metadata for next calls that generate the tuple
		 */
#if PG_VERSION_NUM >= 120000
		tupdesc = CreateTemplateTupleDesc(7);
#else
		tupdesc = CreateTemplateTupleDesc(7, false);
#endif
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "SEGMENT_ID",
		                   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "CONTAINER_ID",
		                   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "UP_TIME",
		                   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "OWNER",
		                   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 5, "MEMORY_USAGE(KB)",
		                   TEXTOID, -1, 0);
        TupleDescInitEntry(tupdesc, (AttrNumber) 6, "CPU_USAGE",
		                   TEXTOID, -1, 0);
        TupleDescInitEntry(tupdesc, (AttrNumber) 7, "Current Running Function",
		                   TEXTOID, -1, 0);

		attinmeta = TupleDescGetAttInMetadata(tupdesc);
		funcctx->attinmeta = attinmeta;

		MemoryContextSwitchTo(oldcontext);
	} else {
		isFirstCall = false;
	}

	funcctx = SRF_PERCALL_SETUP();

	call_cntr = funcctx->call_cntr;
	max_calls = funcctx->max_calls;
	attinmeta = funcctx->attinmeta;

	if (isFirstCall) {
		funcctx->user_fctx = (void *) container_list;
	} else {
		container_list = (struct jsonc_json_object *) funcctx->user_fctx;
	}
	/*if a record is not suitable, skip it and scan next record*/
	while (1) {
		/* send one tuple */
		if (call_cntr < max_calls) {
			char **values;
			HeapTuple tuple;
			Datum result;
			int res;
			char *containerState = NULL;
			struct jsonc_json_object *containerObj = NULL;
			struct jsonc_json_object *containerStateObj = NULL;

			/* Memory Usage */
			struct jsonc_json_object *memoryObj = NULL;
			struct jsonc_json_object *memoryUsageObj = NULL;
			struct jsonc_json_object *memoryStateObj = NULL;
			struct jsonc_json_object *memoryStateCacheObj = NULL;
			int64_t containerMemoryTotal = 0;
			int64_t containerMemoryCache = 0;
			int64_t containerMemoryUsage = 0;

			/* CPU Usage */
			struct jsonc_json_object *cpuObj = NULL;
			struct jsonc_json_object *preCpuObj = NULL;
			struct jsonc_json_object *cpuStatusObj = NULL;
			struct jsonc_json_object *preCpuStatusObj = NULL;
			struct jsonc_json_object *cpuStatusUsageObj = NULL;
			struct jsonc_json_object *preCpuStatusUsageObj = NULL;
			struct jsonc_json_object *cpuSystemUsageObj = NULL;
			struct jsonc_json_object *preCpuSystemUsageObj = NULL;
			struct jsonc_json_object *numberCpusObj = NULL;
			int64_t containerCPUDelta = 0;
			int64_t containerpreCPUDelta = 0;
			int64_t containerCPUSystemDelta = 0;
			int64_t containerpreCPUSystemDelta = 0;
			int containerNumberCPU = 0;
			double containerCPUUsage = 0;

			struct jsonc_json_object *statusObj = NULL;
			const char *statusStr;
			struct jsonc_json_object *labelObj = NULL;
			struct jsonc_json_object *ownerObj = NULL;
			const char *ownerStr;
			const char *username;
			struct jsonc_json_object *dbidObj = NULL;
			const char *dbidStr;
			struct jsonc_json_object *idObj = NULL;
			const char *idStr;

            UdfContainerIdMap *cid = NULL;
			/*
			 * Process json object by its key, and then get value
			 */

			containerObj = json_object_array_get_idx(container_list, call_cntr);
			if (containerObj == NULL) {
				plc_elog(ERROR, "Not a valid container.");
			}

			if (!json_object_object_get_ex(containerObj, "Status", &statusObj)) {
				plc_elog(ERROR, "failed to get json \"Status\" field.");
			}
			statusStr = json_object_get_string(statusObj);
			if (!json_object_object_get_ex(containerObj, "Labels", &labelObj)) {
				plc_elog(ERROR, "failed to get json \"Labels\" field.");
			}
			if (!json_object_object_get_ex(labelObj, "owner", &ownerObj)) {
				funcctx->call_cntr++;
				call_cntr++;
				plc_elog(LOG, "failed to get json \"owner\" field. Maybe this container is not started by PL/Container");
				continue;
			}
			ownerStr = json_object_get_string(ownerObj);
#if PG_VERSION_NUM >= 120000
			username = GetUserNameFromId(GetUserId(), /*noerr*/ false);
#else
			username = GetUserNameFromId(GetUserId());
#endif
			if (strcmp(ownerStr, username) != 0 && superuser() == false) {
				funcctx->call_cntr++;
				call_cntr++;
				plc_elog(DEBUG1, "Current username %s (not super user) is not match conatiner owner %s, skip",
					 username, ownerStr);
				continue;
			}


			if (!json_object_object_get_ex(labelObj, "dbid", &dbidObj)) {
				funcctx->call_cntr++;
				call_cntr++;
				plc_elog(LOG, "failed to get json \"dbid\" field. Maybe this container is not started by PL/Container");
				continue;
			}
			dbidStr = json_object_get_string(dbidObj);

			if (!json_object_object_get_ex(containerObj, "Id", &idObj)) {
				plc_elog(ERROR, "failed to get json \"Id\" field.");
			}
			idStr = json_object_get_string(idObj);

			res = plc_docker_get_container_state(&containerState, idStr, backend);
			if (res < 0) {
				plc_elog(ERROR, "Fail to get docker container state: %s", backend_error_message);
			}

			containerStateObj = json_tokener_parse(containerState);

            /* Parse memory info */
			if (!json_object_object_get_ex(containerStateObj, "memory_stats", &memoryObj)) {
				plc_elog(ERROR, "failed to get json \"memory_stats\" field.");
			}
			if (!json_object_object_get_ex(memoryObj, "usage", &memoryUsageObj)) {
				plc_elog(LOG, "failed to get json \"usage\" field.");
			} else {
				containerMemoryTotal = json_object_get_int64(memoryUsageObj) / 1024;
			}

            if (!json_object_object_get_ex(memoryStateObj, "stats", &memoryUsageObj)) {
				plc_elog(LOG, "failed to get json \"stats\" field.");
			}

            if (!json_object_object_get_ex(memoryStateCacheObj, "cache", &memoryStateObj)) {
				plc_elog(LOG, "failed to get json \"stats.cache\" field.");
			} else {
                containerMemoryCache = json_object_get_int64(memoryStateCacheObj) / 1024;
            }

            containerMemoryUsage = containerMemoryTotal - containerMemoryCache;

            /* Parse CPU info */
            if (!json_object_object_get_ex(containerStateObj, "cpu_stats", &cpuObj)) {
				plc_elog(ERROR, "failed to get json \"cpu_stats\" field.");
			}

            if (!json_object_object_get_ex(containerStateObj, "precpu_stats", &preCpuObj)) {
				plc_elog(ERROR, "failed to get json \"precpu_stats\" field.");
			}

            if (!json_object_object_get_ex(cpuObj, "cpu_usage", &cpuStatusUsageObj)) {
				plc_elog(ERROR, "failed to get json \"cpu_usage\" field.");
			}

            if (!json_object_object_get_ex(preCpuObj, "cpu_usage", &preCpuStatusUsageObj)) {
				plc_elog(ERROR, "failed to get json \"cpu_usage\" field.");
			}

			if (!json_object_object_get_ex(cpuStatusUsageObj, "total_usage", &cpuStatusObj)) {
				plc_elog(LOG, "failed to get json \"cpu.usage\" field.");
			} else {
                containerCPUDelta = json_object_get_int64(cpuStatusObj);
            }

            if (!json_object_object_get_ex(preCpuStatusUsageObj, "total_usage", &preCpuStatusObj)) {
				plc_elog(LOG, "failed to get json \"precpu.usage\" field.");
			} else {
                containerpreCPUDelta = json_object_get_int64(preCpuStatusObj);
            }

            if (!json_object_object_get_ex(cpuObj, "system_cpu_usage", &cpuSystemUsageObj)) {
				plc_elog(LOG, "failed to get json \"cpu.systemusage\" field.");
			} else {
                containerCPUSystemDelta = json_object_get_int64(cpuSystemUsageObj);
            }

            if (!json_object_object_get_ex(preCpuObj, "system_cpu_usage", &preCpuSystemUsageObj)) {
				plc_elog(LOG, "failed to get json \"precpu.system.suage\" field.");
			} else {
                containerpreCPUSystemDelta = json_object_get_int64(preCpuSystemUsageObj);
            }

            if (!json_object_object_get_ex(cpuObj, "online_cpus", &numberCpusObj)) {
				plc_elog(LOG, "failed to get json \"cpu.online_cpus\" field.");
			} else {
                containerNumberCPU = json_object_get_int(numberCpusObj);
            }

            containerCPUUsage = (((containerCPUDelta - containerpreCPUDelta) * 1.0)
                                / ((containerCPUSystemDelta - containerpreCPUSystemDelta) * 1.0))
                                * containerNumberCPU * 100.0;
            /* Then found UDF name from shm */
            cid = find_containerid_entry(idStr);

            if (cid == NULL){
                cid = (UdfContainerIdMap*) palloc(sizeof(UdfContainerIdMap));
                strcpy(cid->udf_name, "No Infomation");
            }

			values = (char **) palloc(7 * sizeof(char *));
			values[0] = (char *) palloc(8 * sizeof(char));
			values[1] = (char *) palloc(80 * sizeof(char));
			values[2] = (char *) palloc(64 * sizeof(char));
			values[3] = (char *) palloc(64 * sizeof(char));
			values[4] = (char *) palloc(64 * sizeof(char));
			values[5] = (char *) palloc(32 * sizeof(char));
			values[6] = (char *) palloc(225 * sizeof(char));

			snprintf(values[0], 8, "%s", dbidStr);
			snprintf(values[1], 80, "%s", idStr);
			snprintf(values[2], 64, "%s", statusStr);
			snprintf(values[3], 64, "%s", ownerStr);
			snprintf(values[4], 64, "%" PRId64 "KB", containerMemoryUsage);
			snprintf(values[5], 32, "%lf%%", containerCPUUsage);
			snprintf(values[6], 224, "%s", cid->udf_name);

			/* build a tuple */
			tuple = BuildTupleFromCStrings(attinmeta, values);

			/* make the tuple into a datum */
			result = HeapTupleGetDatum(tuple);

			SRF_RETURN_NEXT(funcctx, result);
		} else {
			if (container_list != NULL) {
				//json_object_put(container_list);
			}
			SRF_RETURN_DONE(funcctx);
		}
	}

}

PG_FUNCTION_INFO_V1(containers_summary);

Datum
containers_summary(pg_attribute_unused() PG_FUNCTION_ARGS) {
	FuncCallContext *funcctx;
	int call_cntr;
	int max_calls;
	int res;
	TupleDesc tupdesc;
	AttInMetadata *attinmeta;
	struct jsonc_json_object *container_list = NULL;
	char *json_result;
	bool isFirstCall = true;

	char* backend_name = "default";
	if (PG_NARGS() == 1) {
		backend_name = text_to_cstring(PG_GETARG_TEXT_PP(0));
	}
	const plcBackend *backend = take_plcbackend_byname(backend_name);

	/* Init the container list in the first call and get the results back */
	if (SRF_IS_FIRSTCALL()) {
		MemoryContext oldcontext;
		int arraylen;

		/* create a function context for cross-call persistence */
		funcctx = SRF_FIRSTCALL_INIT();

		/* switch to memory context appropriate for multiple function calls */
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		res = plc_docker_list_container(&json_result, GpIdentity.segindex, backend);
		if (res < 0) {
			plc_elog(ERROR, "Docker container list error: %s", backend_error_message);
		}

		/* no container running */
		if (strcmp(json_result, "[]") == 0) {
			funcctx->max_calls = 0;
		}

		container_list = json_tokener_parse(json_result);

		if (container_list == NULL) {
			plc_elog(ERROR, "Parse JSON object error, cannot get the containers summary");
		}

		arraylen = json_object_array_length(container_list);

		/* total number of containers to be returned, each array contains one container */
		funcctx->max_calls = (uint32) arraylen;

		/*
		 * prepare attribute metadata for next calls that generate the tuple
		 */
#if PG_VERSION_NUM >= 120000
		tupdesc = CreateTemplateTupleDesc(5);
#else
		tupdesc = CreateTemplateTupleDesc(5, false);
#endif
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "SEGMENT_ID",
		                   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "CONTAINER_ID",
		                   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "UP_TIME",
		                   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "OWNER",
		                   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 5, "MEMORY_USAGE(KB)",
		                   TEXTOID, -1, 0);

		attinmeta = TupleDescGetAttInMetadata(tupdesc);
		funcctx->attinmeta = attinmeta;

		MemoryContextSwitchTo(oldcontext);
	} else {
		isFirstCall = false;
	}

	funcctx = SRF_PERCALL_SETUP();

	call_cntr = funcctx->call_cntr;
	max_calls = funcctx->max_calls;
	attinmeta = funcctx->attinmeta;

	if (isFirstCall) {
		funcctx->user_fctx = (void *) container_list;
	} else {
		container_list = (json_object *) funcctx->user_fctx;
	}
	/*if a record is not suitable, skip it and scan next record*/
	while (1) {
		/* send one tuple */
		if (call_cntr < max_calls) {
			char **values;
			HeapTuple tuple;
			Datum result;
			int res;
			char *containerState = NULL;
			struct jsonc_json_object *containerObj = NULL;
			struct jsonc_json_object *containerStateObj = NULL;
			int64 containerMemoryUsage = 0;

			struct jsonc_json_object *statusObj = NULL;
			const char *statusStr;
			struct jsonc_json_object *labelObj = NULL;
			struct jsonc_json_object *ownerObj = NULL;
			const char *ownerStr;
			const char *username;
			struct jsonc_json_object *dbidObj = NULL;
			const char *dbidStr;
			struct jsonc_json_object *idObj = NULL;
			const char *idStr;
			struct jsonc_json_object *memoryObj = NULL;
			struct jsonc_json_object *memoryUsageObj = NULL;


			/*
			 * Process json object by its key, and then get value
			 */

			containerObj = json_object_array_get_idx(container_list, call_cntr);
			if (containerObj == NULL) {
				plc_elog(ERROR, "Not a valid container.");
			}

			if (!json_object_object_get_ex(containerObj, "Status", &statusObj)) {
				plc_elog(ERROR, "failed to get json \"Status\" field.");
			}
			statusStr = json_object_get_string(statusObj);
			if (!json_object_object_get_ex(containerObj, "Labels", &labelObj)) {
				plc_elog(ERROR, "failed to get json \"Labels\" field.");
			}
			if (!json_object_object_get_ex(labelObj, "owner", &ownerObj)) {
				funcctx->call_cntr++;
				call_cntr++;
				plc_elog(LOG, "failed to get json \"owner\" field. Maybe this container is not started by PL/Container");
				continue;
			}
			ownerStr = json_object_get_string(ownerObj);
#if PG_VERSION_NUM >= 120000
			username = GetUserNameFromId(GetUserId(), /*noerr*/ false);
#else
			username = GetUserNameFromId(GetUserId());
#endif
			if (strcmp(ownerStr, username) != 0 && superuser() == false) {
				funcctx->call_cntr++;
				call_cntr++;
				plc_elog(DEBUG1, "Current username %s (not super user) is not match conatiner owner %s, skip",
					 username, ownerStr);
				continue;
			}


			if (!json_object_object_get_ex(labelObj, "dbid", &dbidObj)) {
				funcctx->call_cntr++;
				call_cntr++;
				plc_elog(LOG, "failed to get json \"dbid\" field. Maybe this container is not started by PL/Container");
				continue;
			}
			dbidStr = json_object_get_string(dbidObj);

			if (!json_object_object_get_ex(containerObj, "Id", &idObj)) {
				plc_elog(ERROR, "failed to get json \"Id\" field.");
			}
			idStr = json_object_get_string(idObj);

			res = plc_docker_get_container_state(&containerState, idStr, backend);
			if (res < 0) {
				if (res != PLC_DOCKER_API_RES_NOT_FOUND) {
					plc_elog(ERROR, "Fail to get docker container state: %s", backend_error_message);
				}

				statusStr = psprintf("Has been removed");
			} else {
				containerStateObj = json_tokener_parse(containerState);
				if (!json_object_object_get_ex(containerStateObj, "memory_stats", &memoryObj)) {
					plc_elog(ERROR, "failed to get json \"memory_stats\" field.");
				}
				if (!json_object_object_get_ex(memoryObj, "usage", &memoryUsageObj)) {
					plc_elog(LOG, "failed to get json \"usage\" field.");
				} else {
					containerMemoryUsage = json_object_get_int64(memoryUsageObj) / 1024;
				}
			}

			values = (char **) palloc(5 * sizeof(char *));
			values[0] = (char *) palloc(8 * sizeof(char));
			values[1] = (char *) palloc(80 * sizeof(char));
			values[2] = (char *) palloc(64 * sizeof(char));
			values[3] = (char *) palloc(64 * sizeof(char));
			values[4] = (char *) palloc(32 * sizeof(char));

			snprintf(values[0], 8, "%s", dbidStr);
			snprintf(values[1], 80, "%s", idStr);
			snprintf(values[2], 64, "%s", statusStr);
			snprintf(values[3], 64, "%s", ownerStr);
			snprintf(values[4], 32, "%ld", containerMemoryUsage);

			/* build a tuple */
			tuple = BuildTupleFromCStrings(attinmeta, values);

			/* make the tuple into a datum */
			result = HeapTupleGetDatum(tuple);

			SRF_RETURN_NEXT(funcctx, result);
		} else {
			if (container_list != NULL) {
				json_object_put(container_list);
			}
			SRF_RETURN_DONE(funcctx);
		}
	}

}

static Size
PLContainerShmemSize(void)
{
	Size		size = 0;

	size = add_size(size, hash_estimate_size(MAX_UDF_ENTRIES, sizeof(UdfContainerIdMap)));
	return size;
}

/*
 * 	Allocate and initialize plcontainer-related shared memory
 */
void
plcontainer_shmem_startup(void)
{
	HASHCTL		hash_ctl;

	if (prev_shmem_startup_hook)
		(*prev_shmem_startup_hook)();

	udf_container_id_map = NULL;

	LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);
#if PG_VERSION_NUM >= 120000
	LWLockInitialize(plc_lw_lock, LWLockNewTrancheId());
#else
	plc_lw_lock = LWLockAssign();
#endif

	memset(&hash_ctl, 0, sizeof(hash_ctl));
	hash_ctl.keysize = PREFIX_CONTAINER_ID_LENGTH;
	hash_ctl.entrysize =  sizeof(UdfContainerIdMap);
	hash_ctl.hash = string_hash;

	udf_container_id_map = ShmemInitHash("plcontainer map entries between container id and udf name",
									MAX_UDF_ENTRIES,
									MAX_UDF_ENTRIES,
									&hash_ctl,
									HASH_ELEM | HASH_FUNCTION);

	LWLockRelease(AddinShmemInitLock);
}

void
init_plcontainer_shmem(void)
{
	/*
	 * Request additional shared resources.  (These are no-ops if we're not in
	 * the postmaster process.)  We'll allocate or attach to the shared
	 * resources in pgss_shmem_startup().
	 */
	RequestAddinShmemSpace(PLContainerShmemSize());
#if PG_VERSION_NUM < 90600
	RequestAddinLWLocks(1);
#endif

	/*
	 * Install startup hook to initialize our shared memory.
	 */
	prev_shmem_startup_hook = shmem_startup_hook;
	shmem_startup_hook = plcontainer_shmem_startup;
}

void
add_containerid_entry(char *dockerid, char *udf)
{
    bool found;
    UdfContainerIdMap *cidentry;
    char cid[PREFIX_CONTAINER_ID_LENGTH];

    if (udf_container_id_map == NULL)
    {
        return;
    }
    /* Copy the data into hashmap entry */
    strncpy(cid, dockerid, PREFIX_CONTAINER_ID_LENGTH - 1);
    cid[31] = '\0';

    LWLockAcquire(plc_lw_lock, LW_EXCLUSIVE);

    cidentry = hash_search(udf_container_id_map, (void *)cid, HASH_ENTER, &found);
    strncpy(cidentry->udf_name, udf, 224 - 1);
    cidentry->udf_name[223] = '\0';
    LWLockRelease(plc_lw_lock);
}

void
replace_containerid_entry(char *dockerid, char *udf)
{
    bool found;
    UdfContainerIdMap *cidentry;
    char cid[PREFIX_CONTAINER_ID_LENGTH];

    if (udf_container_id_map == NULL)
    {
        return;
    }
    /* Copy the data into hashmap entry */
    strncpy(cid, dockerid, PREFIX_CONTAINER_ID_LENGTH - 1);
    cid[31] = '\0';

    LWLockAcquire(plc_lw_lock, LW_EXCLUSIVE);

    cidentry = hash_search(udf_container_id_map, (void *)cid, HASH_FIND, &found);
    strncpy(cidentry->udf_name, udf, 224 - 1);
    cidentry->udf_name[223] = '\0';
    LWLockRelease(plc_lw_lock);
}

void del_containerid_entry(char *dockerid)
{
    bool found;
    char cid[PREFIX_CONTAINER_ID_LENGTH];

    /* Copy the data into hashmap entry */
    strncpy(cid, dockerid, PREFIX_CONTAINER_ID_LENGTH - 1);
    cid[31] = '\0';

    if (udf_container_id_map == NULL)
    {
        return;
    }

    LWLockAcquire(plc_lw_lock, LW_EXCLUSIVE);

    hash_search(udf_container_id_map, (void *)cid, HASH_REMOVE, &found);

    LWLockRelease(plc_lw_lock);
}

static UdfContainerIdMap*
find_containerid_entry(const char *dockerid)
{
    bool found;
    char cid[PREFIX_CONTAINER_ID_LENGTH];
    UdfContainerIdMap *cidentry = NULL;

    /* Copy the data into hashmap entry */
    strncpy(cid, dockerid, PREFIX_CONTAINER_ID_LENGTH - 1);
    cid[31] = '\0';

    if (udf_container_id_map == NULL)
    {
        return NULL;
    }

    cidentry = hash_search(udf_container_id_map, (void *)cid, HASH_FIND, &found);

    if (found)
    {
        return cidentry;
    }

    return NULL;
}

PG_FUNCTION_INFO_V1(collect_running_containers);

Datum
collect_running_containers(pg_attribute_unused() PG_FUNCTION_ARGS)
{
    char **values;
	TupleDesc tupdesc;
	AttInMetadata *attinmeta;
    CdbPgResults cdb_pgresults = {};
    int	i,j;
    char  *sql = NULL;
    Tuplestorestate	   *tupstore;
	MemoryContext	oldcontext;
    ReturnSetInfo  *rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;

    /* Prepare Tuplestore */
	oldcontext = MemoryContextSwitchTo(rsinfo->econtext->ecxt_per_query_memory);
#if PG_VERSION_NUM >= 120000
    tupdesc = CreateTemplateTupleDesc(7);
#else
    tupdesc = CreateTemplateTupleDesc(7, false);
#endif
	TupleDescInitEntry(tupdesc, (AttrNumber) 1, "SEGMENT_ID",
	                   TEXTOID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 2, "CONTAINER_ID",
	                   TEXTOID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 3, "UP_TIME",
	                   TEXTOID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 4, "OWNER",
	                   TEXTOID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 5, "MEMORY_USAGE(KB)",
	                   TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber) 6, "CPU_USAGE",
	                   TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber) 7, "Current Running Function",
	                   TEXTOID, -1, 0);

	attinmeta = TupleDescGetAttInMetadata(tupdesc);
    rsinfo->returnMode = SFRM_Materialize;

    /* initialize our tuplestore */
	tupstore = tuplestore_begin_heap(true, false, work_mem);

    /* first get all oid of tables which are active table on any segment */
	sql = "select * from plcontainer_containers_info()";

	/* any errors will be catch in upper level */
	CdbDispatchCommand(sql, DF_NONE, &cdb_pgresults);

	for (i = 0; i < cdb_pgresults.numResults; i++)
	{

	    struct pg_result *pgresult = cdb_pgresults.pg_results[i];
	    HeapTuple tuple;

	    if (PQresultStatus(pgresult) != PGRES_TUPLES_OK)
	    {
		    cdbdisp_clearCdbPgResults(&cdb_pgresults);
		    ereport(ERROR,
				(errmsg("Unable to get container information from segment: %d",
						PQresultStatus(pgresult))));
	    }

	    if (PQntuples(pgresult) == 0)
	    {
		    break;
	    }


        values = (char **) palloc(7 * sizeof(char *));

		for (j = 0; j < PQntuples(pgresult); j++)
		{
			values[0] = PQgetvalue(pgresult, j, 0);
			values[1] = PQgetvalue(pgresult, j, 1);
			values[2] = PQgetvalue(pgresult, j, 2);
			values[3] = PQgetvalue(pgresult, j, 3);
			values[4] = PQgetvalue(pgresult, j, 4);
			values[5] = PQgetvalue(pgresult, j, 5);
			values[6] = PQgetvalue(pgresult, j, 6);
        		tuple = BuildTupleFromCStrings(attinmeta, values);
        		tuplestore_puttuple(tupstore, tuple);
		}
        pfree(values);
	}
    cdbdisp_clearCdbPgResults(&cdb_pgresults);
    rsinfo->setDesc = tupdesc;
    rsinfo->setResult = tupstore;
	MemoryContextSwitchTo(oldcontext);

	return (Datum) 0;
}
