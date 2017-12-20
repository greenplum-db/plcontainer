/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */


#include <libxml/tree.h>
#include <libxml/parser.h>
#include <unistd.h>
#include <sys/stat.h>

#include "postgres.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "libpq/libpq-be.h"
#include "funcapi.h"

#include "common/comm_utils.h"
#include "common/comm_connectivity.h"
#include "plcontainer.h"
#include "plc_backend_api.h"
#include "plc_configuration.h"

static runtimeConf *plcContConf = NULL;
static int plcNumContainers = 0;
// we just want to avoid cleanup process to remove previous domain
// socket file, so int32 is sufficient
static int domain_socket_no = 0;

static int parse_container(xmlNode *node, runtimeConf *conf);

static runtimeConf *get_containers(xmlNode *node, int *size);

static void free_containers(runtimeConf *conf, int size);

static void print_containers(runtimeConf *conf, int size);

PG_FUNCTION_INFO_V1(refresh_plcontainer_config);

PG_FUNCTION_INFO_V1(show_plcontainer_config);

/* Function parses the container XML definition and fills the passed
 * plcContainerConf structure that should be already allocated */
static int parse_container(xmlNode *node, runtimeConf *conf) {
	xmlNode *cur_node = NULL;
	xmlChar *value = NULL;
	int has_image = 0;
	int has_id = 0;
	int has_command = 0;
	int num_shared_dirs = 0;

	/* First iteration - parse name, container_id and memory_mb and count the
	 * number of shared directories for later allocation of related structure */
	memset((void *) conf, 0, sizeof(runtimeConf));
	conf->memoryMb = 1024;
	conf->enable_log = false;
	conf->isNetworkConnection = false;
	for (cur_node = node->children; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			int processed = 0;
			value = NULL;

			if (xmlStrcmp(cur_node->name, (const xmlChar *) "id") == 0) {
				processed = 1;
				has_id = 1;
				value = xmlNodeGetContent(cur_node);
				conf->runtimeid = plc_top_strdup((char *) value);
			}

			if (xmlStrcmp(cur_node->name, (const xmlChar *) "image") == 0) {
				processed = 1;
				has_image = 1;
				value = xmlNodeGetContent(cur_node);
				conf->image = plc_top_strdup((char *) value);
			}

			if (xmlStrcmp(cur_node->name, (const xmlChar *) "command") == 0) {
				processed = 1;
				has_command = 1;
				value = xmlNodeGetContent(cur_node);
				conf->command = plc_top_strdup((char *) value);
			}

			if (xmlStrcmp(cur_node->name, (const xmlChar *) "setting") == 0) {
				bool validSetting = false;
				processed = 1;
				value = xmlGetProp(cur_node, (const xmlChar *) "logs");
				if (value != NULL) {
					validSetting = true;
					if (strcasecmp((char *) value, "enable") == 0) {
						conf->enable_log = true;
					} else if (strcasecmp((char *) value, "disable") == 0) {
						conf->enable_log = false;
					} else {
						elog(ERROR, "SETTING element <log> only accepted \"enable\" or"
							"\"disable\" only, current string is %s", value);
					}
				}
				value = xmlGetProp(cur_node, (const xmlChar *) "memory_mb");
				if (value != NULL) {
					validSetting = true;
					long memorySize = pg_atoi((char *) value, sizeof(int), 0);
					if (memorySize <= 0) {
						elog(ERROR, "container memory size could not less 0, current string is %s", value);
					} else {
						conf->memoryMb = conf->memoryMb;
					}
				}
				value = xmlGetProp(cur_node, (const xmlChar *) "use_network");
				if (value != NULL) {
					validSetting = true;
					if (strcasecmp((char *) value, "false") == 0 ||
					    strcasecmp((char *) value, "no") == 0) {
						conf->isNetworkConnection = false;
					} else if (strcasecmp((char *) value, "true") == 0 ||
					         strcasecmp((char *) value, "yes") == 0) {
						conf->isNetworkConnection = true;
					} else {
						elog(WARNING, "SETTING element <use_network> only accepted \"yes\"|\"true\" or"
							"\"no\"|\"false\" only, current string is %s", value);

					}
				}
				if (!validSetting) {
					elog(ERROR, "Unrecognized setting options, please check the configuration file: %s", conf->runtimeid);
				}

			}

			if (xmlStrcmp(cur_node->name, (const xmlChar *) "shared_directory") == 0) {
				num_shared_dirs += 1;
				processed = 1;
			}

			/* If the tag is not known - we raise the related error */
			if (processed == 0) {
				elog(ERROR, "Unrecognized element '%s' inside of container specification",
				     cur_node->name);
				return -1;
			}

			/* Free the temp value if we have allocated it */
			if (value) {
				xmlFree(value);
			}
		}
	}

	if (has_id == 0) {
		elog(ERROR, "tag <id> must be specified in configuartion");
		return -1;
	}

	if (has_image == 0) {
		elog(ERROR, "tag <image> must be specified in configuration: %s", conf->runtimeid);
		return -1;
	}

	if (has_command == 0) {
		elog(ERROR, "tag <command> must be specified in configuration: %s", conf->runtimeid);
		return -1;
	}

	/* Process the shared directories */
	conf->nSharedDirs = num_shared_dirs;
	conf->sharedDirs = NULL;
	if (num_shared_dirs > 0) {
		int i = 0;

		/* Allocate in top context as it should live between function calls */
		conf->sharedDirs = plc_top_alloc(num_shared_dirs * sizeof(plcSharedDir));
		for (cur_node = node->children; cur_node; cur_node = cur_node->next) {
			if (cur_node->type == XML_ELEMENT_NODE &&
			    xmlStrcmp(cur_node->name, (const xmlChar *) "shared_directory") == 0) {

				value = xmlGetProp(cur_node, (const xmlChar *) "host");
				if (value == NULL) {
					elog(ERROR, "Configuration tag 'shared_directory' has a mandatory element"
						" 'host' that is not found: %s", conf->runtimeid);
					return -1;
				}
				conf->sharedDirs[i].host = plc_top_strdup((char *) value);
				xmlFree(value);

				value = xmlGetProp(cur_node, (const xmlChar *) "container");
				if (value == NULL) {
					elog(ERROR, "Configuration tag 'shared_directory' has a mandatory element"
						" 'container' that is not found: %s", conf->runtimeid);
					return -1;
				}
				conf->sharedDirs[i].container = plc_top_strdup((char *) value);
				xmlFree(value);

				value = xmlGetProp(cur_node, (const xmlChar *) "access");
				if (value == NULL) {
					elog(ERROR, "Configuration tag 'shared_directory' has a mandatory element"
						" 'access' that is not found: %s", conf->runtimeid);
					return -1;
				} else if (strcmp((char *) value, "ro") == 0) {
					conf->sharedDirs[i].mode = PLC_ACCESS_READONLY;
				} else if (strcmp((char *) value, "rw") == 0) {
					conf->sharedDirs[i].mode = PLC_ACCESS_READWRITE;
				} else {
					elog(ERROR, "Directory access mode should be either 'ro' or 'rw', passed value is '%s': %s", value, conf->runtimeid);
					return -1;
				}
				xmlFree(value);

				i += 1;
			}
		}
	}

	return 0;
}

/* Function returns an array of plcContainerConf structures based on the contents
 * of passed XML document tree. Returns NULL on failure */
static runtimeConf *get_containers(xmlNode *node, int *size) {
	xmlNode *cur_node = NULL;
	int nContainers = 0;
	int i = 0;
	int res = 0;
	runtimeConf *result = NULL;

	/* Validation that the root node matches the expected specification */
	if (xmlStrcmp(node->name, (const xmlChar *) "configuration") != 0) {
		elog(ERROR, "Wrong XML configuration provided. Expected 'configuration'"
			" as root element, got '%s' instead", node->name);
		return result;
	}

	/* Iterating through the list of containers to get the count */
	for (cur_node = node->children; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE &&
		    xmlStrcmp(cur_node->name, (const xmlChar *) "runtime") == 0) {
			nContainers += 1;
		}
	}

	/* If no container definitions found - error */
	if (nContainers == 0) {
		elog(ERROR, "Did not find a single 'runtime' declaration in configuration");
		return result;
	}

	result = plc_top_alloc(nContainers * sizeof(runtimeConf));

	/* Iterating through the list of containers to parse them into plcContainerConf */
	i = 0;
	res = 0;
	for (cur_node = node->children; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE &&
		    xmlStrcmp(cur_node->name, (const xmlChar *) "runtime") == 0) {
			res |= parse_container(cur_node, &result[i]);
			i += 1;
		}
	}

	/* If error occurred during parsing - return NULL */
	if (res != 0) {
		free_containers(result, nContainers);
		result = NULL;
	}

	*size = nContainers;
	return result;
}

/* Safe way to deallocate container configuration list structure */
static void free_containers(runtimeConf *conf, int size) {
	int i;
	for (i = 0; i < size; i++) {
		if (conf[i].nSharedDirs > 0 && conf[i].sharedDirs != NULL) {
			pfree(conf[i].sharedDirs);
		}
	}
	pfree(conf);
}

static void print_containers(runtimeConf *conf, int size) {
	int i, j;
	for (i = 0; i < size; i++) {
		elog(INFO, "Container '%s' configuration", conf[i].runtimeid);
		elog(INFO, "    image = '%s'", conf[i].image);
		elog(INFO, "    memory_mb = '%d'", conf[i].memoryMb);
		elog(INFO, "    use network = '%s'", conf[i].isNetworkConnection ? "yes" : "no");
		elog(INFO, "    enable log  = '%s'", conf[i].enable_log ? "yes" : "no");
		for (j = 0; j < conf[i].nSharedDirs; j++) {
			elog(INFO, "    shared directory from host '%s' to container '%s'",
			     conf[i].sharedDirs[j].host,
			     conf[i].sharedDirs[j].container);
			if (conf[i].sharedDirs[j].mode == PLC_ACCESS_READONLY) {
				elog(INFO, "        access = readonly");
			} else {
				elog(INFO, "        access = readwrite");
			}
		}
	}
}

static int plc_refresh_container_config(bool verbose) {
	xmlDoc *doc = NULL;
	char filename[1024];

	/*
	 * this initialize the library and check potential ABI mismatches
	 * between the version it was compiled for and the actual shared
	 * library used.
	 */
	LIBXML_TEST_VERSION

	/* Parse the file and get the DOM */
	sprintf(filename, "%s/plcontainer_configuration.xml", data_directory);
	doc = xmlReadFile(filename, NULL, 0);
	if (doc == NULL) {
		elog(ERROR, "Error: could not parse file %s, wrongly formatted XML or missing configuration file\n", filename);
		return -1;
	}

	/* Read the configuration */
	if (plcContConf != NULL) {
		free_containers(plcContConf, plcNumContainers);
		plcContConf = NULL;
		plcNumContainers = 0;
	}

	plcContConf = get_containers(xmlDocGetRootElement(doc), &plcNumContainers);

	/* Free the document */
	xmlFreeDoc(doc);

	/* Free the global variables that may have been allocated by the parser */
	xmlCleanupParser();

	if (plcContConf == NULL) {
		return -1;
	}

	if (verbose) {
		print_containers(plcContConf, plcNumContainers);
	}

	return 0;
}

static int plc_show_container_config() {
	int res = 0;

	if (plcContConf == NULL) {
		res = plc_refresh_container_config(false);
		if (res != 0)
			return -1;
	}

	if (plcContConf == NULL) {
		return -1;
	}

	print_containers(plcContConf, plcNumContainers);
	return 0;
}

/* Function referenced from Postgres that can update configuration on
 * specific GPDB segment */
Datum
refresh_plcontainer_config(PG_FUNCTION_ARGS) {
	int res = plc_refresh_container_config(PG_GETARG_BOOL(0));
	if (res == 0) {
		PG_RETURN_TEXT_P(cstring_to_text("ok"));
	} else {
		PG_RETURN_TEXT_P(cstring_to_text("error"));
	}
}

/*
 * Function referenced from Postgres that can update configuration on
 * specific GPDB segment
 */
Datum
show_plcontainer_config(pg_attribute_unused() PG_FUNCTION_ARGS) {
	int res = plc_show_container_config();
	if (res == 0) {
		PG_RETURN_TEXT_P(cstring_to_text("ok"));
	} else {
		PG_RETURN_TEXT_P(cstring_to_text("error"));
	}
}

runtimeConf *plc_get_runtime_configuration(char *runtime_id) {
	int res = 0;
	int i = 0;
	runtimeConf *result = NULL;

	if (plcContConf == NULL || plcNumContainers == 0) {
		res = plc_refresh_container_config(0);
		if (res < 0) {
			return NULL;
		}
	}

	for (i = 0; i < plcNumContainers; i++) {
		if (strcmp(runtime_id, plcContConf[i].runtimeid) == 0) {
			result = &plcContConf[i];
			break;
		}
	}

	return result;
}

char *get_sharing_options(runtimeConf *conf, int container_slot, bool *has_error, char **uds_dir) {
	char *res = NULL;

	*has_error = false;

	if (conf->nSharedDirs >= 0) {
		char **volumes = NULL;
		int totallen = 0;
		char *pos;
		int i;
		char comma = ' ';

		volumes = palloc((conf->nSharedDirs + 1) * sizeof(char *));
		for (i = 0; i < conf->nSharedDirs; i++) {
			volumes[i] = palloc(10 + strlen(conf->sharedDirs[i].host) +
			                    strlen(conf->sharedDirs[i].container));
			if (i > 0)
				comma = ',';
			if (conf->sharedDirs[i].mode == PLC_ACCESS_READONLY) {
				sprintf(volumes[i], " %c\"%s:%s:ro\"", comma, conf->sharedDirs[i].host,
				        conf->sharedDirs[i].container);
			} else if (conf->sharedDirs[i].mode == PLC_ACCESS_READWRITE) {
				sprintf(volumes[i], " %c\"%s:%s:rw\"", comma, conf->sharedDirs[i].host,
				        conf->sharedDirs[i].container);
			} else {
				snprintf(api_error_message, sizeof(api_error_message),
				         "Cannot determine directory sharing mode: %d",
				         conf->sharedDirs[i].mode);
				*has_error = true;
				return NULL;
			}
			totallen += strlen(volumes[i]);
		}

		if (!conf->isNetworkConnection) {
			if (i > 0)
				comma = ',';
			/* Directory for QE : IPC_GPDB_BASE_DIR + "." + PID + "." + container_slot */
			int gpdb_dir_sz;

			gpdb_dir_sz = strlen(IPC_GPDB_BASE_DIR) + 1 + 16 + 1 + 16 + 1 + 4 + 1;
			*uds_dir = pmalloc(gpdb_dir_sz);
			sprintf(*uds_dir, "%s.%d.%d.%d", IPC_GPDB_BASE_DIR, getpid(), domain_socket_no++, container_slot);
			volumes[i] = pmalloc(10 + gpdb_dir_sz + strlen(IPC_CLIENT_DIR));
			sprintf(volumes[i], " %c\"%s:%s:rw\"", comma, *uds_dir, IPC_CLIENT_DIR);
			totallen += strlen(volumes[i]);

			/* Create the directory. */
			if (mkdir(*uds_dir, S_IRWXU) < 0 && errno != EEXIST) {
				snprintf(api_error_message, sizeof(api_error_message),
				         "Cannot create directory %s: %s",
				         *uds_dir, strerror(errno));
				*has_error = true;
				return NULL;
			}
		}

		res = palloc(totallen + conf->nSharedDirs + 1 + 1);
		pos = res;
		for (i = 0; i < (conf->isNetworkConnection ? conf->nSharedDirs : conf->nSharedDirs + 1); i++) {
			memcpy(pos, volumes[i], strlen(volumes[i]));
			pos += strlen(volumes[i]);
			*pos = ' ';
			pos++;
			pfree(volumes[i]);
		}
		*pos = '\0';
		pfree(volumes);
	}

	return res;
}

PG_FUNCTION_INFO_V1(containers_summary);

Datum
containers_summary(pg_attribute_unused() PG_FUNCTION_ARGS) {

	FuncCallContext *funcctx;
	int call_cntr;
	int max_calls;
	TupleDesc tupdesc;
	AttInMetadata *attinmeta;
	struct json_object *container_list = NULL;
	char *json_result;
	bool isFirstCall = true;


	/* Init the container list in the first call and get the results back */
	if (SRF_IS_FIRSTCALL()) {
		MemoryContext oldcontext;
		int arraylen;

		/* create a function context for cross-call persistence */
		funcctx = SRF_FIRSTCALL_INIT();

		/* switch to memory context appropriate for multiple function calls */
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		plc_docker_list_container(&json_result);

		/* no container running */
		if (strcmp(json_result, "[]") == 0) {
			funcctx->max_calls = 0;
		}

		container_list = json_tokener_parse(json_result);

		if (container_list == NULL) {
			elog(ERROR, "Parse JSON object error, cannot get the containers summary");
		}

		arraylen = json_object_array_length(container_list);

		/* total number of containers to be returned, each array contains one container */
		funcctx->max_calls = (uint32) arraylen;

		/*
		 * prepare attribute metadata for next calls that generate the tuple
		 */

		tupdesc = CreateTemplateTupleDesc(5, false);
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
			char *containerState = NULL;
			struct json_object *containerObj = NULL;
			struct json_object *containerStateObj = NULL;
			int64 containerMemoryUsage = 0;

			/*
			 * Process json object by its key, and then get value
			 */

			containerObj = json_object_array_get_idx(container_list, call_cntr);
			if (containerObj == NULL) {
				elog(ERROR, "Not a valid container.");
			}

			struct json_object *statusObj = NULL;
			if (!json_object_object_get_ex(containerObj, "Status", &statusObj)) {
				elog(ERROR, "failed to get json \"Status\" field.");
			}
			const char *statusStr = json_object_get_string(statusObj);
			struct json_object *labelObj = NULL;
			if (!json_object_object_get_ex(containerObj, "Labels", &labelObj)) {
				elog(ERROR, "failed to get json \"Labels\" field.");
			}
			struct json_object *ownerObj = NULL;
			if (!json_object_object_get_ex(labelObj, "owner", &ownerObj)) {
				funcctx->call_cntr++;
				call_cntr++;
				elog(LOG, "failed to get json \"owner\" field. Maybe this container is not started by PL/Container");
				continue;
			}
			const char *ownerStr = json_object_get_string(ownerObj);
			const char *username = GetUserNameFromId(GetUserId());
			if (strcmp(ownerStr, username) != 0 && superuser() == false) {
				funcctx->call_cntr++;
				call_cntr++;
				elog(DEBUG1, "Current username %s (not super user) is not match conatiner owner %s, skip",
					 username, ownerStr);
				continue;
			}

			struct json_object *dbidObj = NULL;
			if (!json_object_object_get_ex(labelObj, "dbid", &dbidObj)) {
				funcctx->call_cntr++;
				call_cntr++;
				elog(LOG, "failed to get json \"dbid\" field. Maybe this container is not started by PL/Container");
				continue;
			}
			const char *dbidStr = json_object_get_string(dbidObj);
			struct json_object *idObj = NULL;
			if (!json_object_object_get_ex(containerObj, "Id", &idObj)) {
				elog(ERROR, "failed to get json \"Id\" field.");
			}
			const char *idStr = json_object_get_string(idObj);

			plc_docker_get_container_state(idStr, &containerState);
			containerStateObj = json_tokener_parse(containerState);
			struct json_object *memoryObj = NULL;
			if (!json_object_object_get_ex(containerStateObj, "memory_stats", &memoryObj)) {
				elog(ERROR, "failed to get json \"memory_stats\" field.");
			}
			struct json_object *memoryUsageObj = NULL;
			if (!json_object_object_get_ex(memoryObj, "usage", &memoryUsageObj)) {
				elog(LOG, "failed to get json \"usage\" field.");
			} else {
				containerMemoryUsage = json_object_get_int64(memoryUsageObj) / 1024;
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
