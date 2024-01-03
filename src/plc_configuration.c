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
#ifndef PLC_PG
  #include "commands/resgroupcmds.h"
#else
  #include "catalog/pg_type.h"
  #include "access/sysattr.h"
  #include "miscadmin.h"
#endif
#include "utils/builtins.h"
#include "utils/guc.h"
#include "libpq/libpq-be.h"
#include "utils/acl.h"

#ifdef PLC_PG
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include "funcapi.h"
#ifdef PLC_PG
#pragma GCC diagnostic pop
#endif

#include "cdb/cdbvars.h"

#include "common/comm_utils.h"
#include "common/comm_connectivity.h"
#if PG_VERSION_NUM >= 120000 // PG12 or GP7
  #include "access/table.h"
  #include "common/hashfn.h"
  #include "utils/varlena.h"
  #include "catalog/pg_resgroupcapability.h"
#endif
#include "plcontainer.h"
#include "plc_backend_api.h"
#include "plc_docker_api.h"
#include "plc_configuration.h"

#define pfree_null(x) do { if (x) pfree(x); } while(0)

// we just want to avoid cleanup process to remove previous domain
// socket file, so int32 is sufficient
static int domain_socket_no = 0;

static void init_runtime_configurations();

static void parse_runtime_configuration(xmlNode *node);
static void parse_runtime_backend_configuration(xmlNode *node);

static void get_runtime_configurations(xmlNode *node);

static void free_runtime_conf_entry(runtimeConfEntry *conf);
static void free_runtime_backend_entry(plcBackend *entry, bool release_self);

static void print_runtime_configurations();

PG_FUNCTION_INFO_V1(refresh_plcontainer_config);

PG_FUNCTION_INFO_V1(show_plcontainer_config);

static HTAB *runtime_conf_table;

// runtime_backend_table is a session level in-process static variable
// [0] is the default runtime backend with name 'default' fill in by parse_runtime_backend_configuration()
static plcBackend *runtime_backend_table = NULL;
static int nruntime_backend_table = 0;

static char *plc_top_strdup_null(const char*v) {
	if (v == NULL)
		return NULL;
	return plc_top_strdup(v);
}

/*
 * init runtime conf hash table.
 */
static void init_runtime_configurations() {
    /* create the runtime conf hash table*/
	HASHCTL		hash_ctl;

	/* destroy runtime configuration first if exists */
	if (runtime_conf_table != NULL) {
		HASH_SEQ_STATUS hash_status;
		runtimeConfEntry *entry;

		hash_seq_init(&hash_status, runtime_conf_table);

		while ((entry = (runtimeConfEntry *) hash_seq_search(&hash_status)) != NULL)
		{
			free_runtime_conf_entry(entry);
		}
		hash_destroy(runtime_conf_table);
	}

	/* destroy backend configuration first if exists */
	if (nruntime_backend_table != 0) {
		for (int i = 0; i < nruntime_backend_table; i++) {
			free_runtime_backend_entry(&runtime_backend_table[i], false);
		}
		pfree_null(runtime_backend_table);
		nruntime_backend_table = 0;
		runtime_backend_table = NULL;
	}

	MemSet(&hash_ctl, 0, sizeof(hash_ctl));
	hash_ctl.keysize = RUNTIME_ID_MAX_LENGTH;
	hash_ctl.entrysize = sizeof(runtimeConfEntry);
	hash_ctl.hash = string_hash;
	/*
	 * Key and value of hash table share the same storage of entry.
	 * the first keysize bytes of the entry is the hash key, and the
	 * value if the entry itself.
	 * For string key, we use string_hash to caculate hash key and
	 * use strlcpy to copy key(when string_hash is set as hash function).
	 */
	runtime_conf_table = hash_create("runtime configuration hash",
								MAX_EXPECTED_RUNTIME_NUM,
								&hash_ctl,
								HASH_ELEM | HASH_FUNCTION);

	if (runtime_conf_table == NULL) {
		plc_elog(ERROR, "Error: could not create runtime conf hash table. Check your memory usage.");
	}

	return ;
}

#define XC(c) ((const xmlChar*)(c))

#define XML_READ_TEXT(to, from) \
			do { \
				xmlChar *__x = xmlNodeGetContent(from); \
				to = plc_top_strdup((const char*) __x); \
				xmlFree(__x); \
			} while(0)

#define XML_READ_INT(to, from) \
			do { \
				xmlChar *__x = xmlNodeGetContent(from); \
				to = atoi((const char*)__x); \
				xmlFree(__x); \
			} while(0)

#define XML_TYPE_text          10001
#define XML_TYPE_integer       10002
#define XML_TYPE_floating      10003
#define XML_TYPE_boolean       10004
#define XML_TYPE_object        10005

int XML_TYPE_SIGNLE_ELEMENT = -1;

typedef struct XML_FIELD {
	// the type of this field
	int tag;

	// the name of this field. eg:
	//   <foo name="bar">  or <foo><name /></foo>
	//        ^^^^                  ^^^^
	const xmlChar *name;

	// the number of element in input array. start from 0
	// -1 means input is signal element, not a array
	int *N;

	// sizeof(type), optional if is basic type
	size_t mem_size;

	// if true, the field must exists
	bool notnull;

	// a pointer to the output
	// if XML_TYPE_SIGNLE_ELEMENT, the prt is a value
	// if not XML_TYPE_SIGNLE_ELEMENT, the prt is a value list
	void *ptr;

	// parse function for read object
	void (*parse)(void *ctx, size_t off, xmlNode *node);
} XML_FIELD;

static void __XML_READ_BASIC_ELEMENT(int n, XML_FIELD *field, xmlChar *v,
									 void (*parser)(void *ctx, size_t off, xmlChar *value)) {
	// single text element
	if (*field->N == -1) {
		parser(field->ptr, 0, v);
		return;
	}

	void *source_mem = *(void**)field->ptr;
	int source_size = *field->N * field->mem_size;

	// an array of text. and freespace is enough to add one more item
	if (source_mem != NULL && *field->N > n) {
		parser(source_mem, n, v);
		(*field->N)++;
		return;
	}

	// freespace is not enough
	int new_size = ((*field->N) == 0 ? 1 : *field->N) * 2 * field->mem_size;
	void *new_mem = MemoryContextAllocZero(TopMemoryContext, new_size);

	if (source_size > 0) {
		memcpy(new_mem, source_mem, source_size);
		pfree_null(source_mem);
	}

	*(void**)field->ptr = new_mem;
	*field->N = new_size / field->mem_size;

	return __XML_READ_BASIC_ELEMENT(n, field, v, parser);
}

static void __XML_READ_OBJECT(int n, XML_FIELD *field, xmlNode *node) {
	if (*field->N == -1) {
		field->parse(field->ptr, 0, node);
		return;
	}

	void *source_mem = *(void**)field->ptr;
	int source_size = *field->N * field->mem_size;

	// an array of text. and freespace is enough to add one more element
	if (source_mem != NULL && *field->N > n) {
		field->parse(source_mem, n, node);
		(*field->N)++;
		return;
	}

	// freespace is not enough
	int new_size = ((*field->N) == 0 ? 1 : *field->N) * 2 * field->mem_size;
	void *new_mem = MemoryContextAllocZero(TopMemoryContext, new_size);

	// copy old item to new memory
	if (source_size > 0) {
		memcpy(new_mem, source_mem, source_size);
		pfree_null(source_mem);
	}

	*(void**)field->ptr = new_mem;
	*field->N = new_size/ field->mem_size;

	return __XML_READ_OBJECT(n, field, node);
}

static void XML_READ_PROPERTY(xmlNode *node, size_t nfields, XML_FIELD fields[]) {
	for (int i = 0; i < nfields; i++) {
		XML_FIELD *field = &fields[i];
		xmlChar *v = NULL;

		if (xmlHasProp(node, field->name) == NULL) {
			if (field->notnull) {
				plc_elog(ERROR, "in <%s>: property \"%s\" is not optional", node->name, field->name);
			}

			continue;
		}

		switch (field->tag) {
			case XML_TYPE_text:
				v = xmlGetProp(node, field->name);
				*(char**)field->ptr = plc_top_strdup((const char*)v);
				break;
			case XML_TYPE_floating:
				v = xmlGetProp(node, field->name);
				*(double*)field->ptr = strtod((const char*)v, NULL);
				break;
			case XML_TYPE_integer:
				v = xmlGetProp(node, field->name);
				*(int64_t*)field->ptr = atoll((const char*)v);
				break;
			case XML_TYPE_boolean:
				*(bool*)field->ptr = (xmlHasProp(node, field->name) != NULL);
				break;
			default:
				Assert(!"unreachable");
				break;
		}

		if (v != NULL) {
			xmlFree(v);
		}
	}
}


static void xml_atob(void *out, size_t off, xmlChar *v) {
	int a = atoi((const char*)v);

	bool *o = (bool*)out;
	o[off] = (a != 0);
}

static void xml_strdup(void *out, size_t off, xmlChar *v) {
	char **o = (char**)out;
	o[off] = plc_top_strdup((const char*)v);
}

static void xml_atoll(void *out, size_t off, xmlChar *v) {
	int64_t *o = (int64_t*)out;
	o[off] = atoll((const char *)v);
}

static void xml_atof(void *out, size_t off, xmlChar *v) {
	double *o = (double*)out;
	o[off] = atof((const char*)v);
}

static void XML_READ_CONTENT(xmlNode *node, int nfields, XML_FIELD fields[]) {
	for (int i = 0; i < nfields; i++) {
		XML_FIELD *field = &fields[i];
		size_t N = 0;

		for (xmlNode *sub = node->children; sub != NULL; sub = sub->next) {
			if (sub->type != XML_ELEMENT_NODE) {
				continue;
			}

			if (xmlStrcmp(sub->name, field->name) != 0) {
				continue;
			}

			if (*field->N == XML_TYPE_SIGNLE_ELEMENT && N != 0) {
				plc_elog(ERROR, "in <%s>: duplicate <%s> found", node->name, sub->name);
				Assert(!"unreachable");
			}

			// do the read here
			xmlChar *v = xmlNodeGetContent(sub);
			switch (field->tag) {
				case XML_TYPE_text:
					field->mem_size = sizeof(char*);
					__XML_READ_BASIC_ELEMENT(N, field, v, xml_strdup);
					break;
				case XML_TYPE_integer:
					field->mem_size = sizeof(int64_t);
					__XML_READ_BASIC_ELEMENT(N, field, v, xml_atoll);
					break;
				case XML_TYPE_floating:
					field->mem_size = sizeof(double);
					__XML_READ_BASIC_ELEMENT(N, field, v, xml_atof);
					break;
				case XML_TYPE_boolean:
					field->mem_size = sizeof(bool);
					__XML_READ_BASIC_ELEMENT(N, field, v, xml_atob);
					break;
				case XML_TYPE_object:
					Assert(field->mem_size != 0);
					__XML_READ_OBJECT(N, field, sub);
					break;
				default:
					Assert(!"unreachable");
					break;
			}

			N++;
		}

		if (*field->N != XML_TYPE_SIGNLE_ELEMENT)
			*field->N = N;

		if (field->notnull && N == 0) {
			plc_elog(ERROR, "in <%s>: <%s /> is not optional", node->name, field->name);
		}
	}
}

/* Function parses the container XML definition and fills the passed
 * plcContainerConf structure that should be already allocated */
static void parse_runtime_configuration(xmlNode *node) {
	xmlNode *cur_node = NULL;

	// we add some cleanups after longjmp. longjmp may clobber the registers,
	// so we need to add volatile qualifier to pointer. If the pointee is read
	// after longjmp, the pointee also need to include volatile qualifier.
	xmlChar* volatile value = NULL;
	volatile char* volatile runtime_id = NULL;
	int volatile id_num, image_num, command_num, num_shared_dirs;
	int volatile num_device_request = 0;

	runtimeConfEntry *conf_entry = NULL;
	bool		foundPtr;

	if (runtime_conf_table == NULL) {
		plc_elog(ERROR, "Runtime configuration table is not initialized.");
	}

	PG_TRY();
	{
		id_num = 0;
		image_num = 0;
		command_num = 0;
		num_shared_dirs = 0;
		/* Find the hash key (runtime id) firstly.*/
		for (cur_node = node->children; cur_node; cur_node = cur_node->next) {
			if (cur_node->type == XML_ELEMENT_NODE &&
					xmlStrcmp(cur_node->name, (const xmlChar *) "id") == 0) {
				if (id_num++ > 0) {
					plc_elog(ERROR, "tag <id> must be specified only once in configuartion");
				}
				value = xmlNodeGetContent(cur_node);

				runtime_id = plc_top_strdup((char *) value);
				if (value) {
					xmlFree((void *) value);
					value = NULL;
				}
			}
		}

		if (id_num == 0) {
			plc_elog(ERROR, "tag <id> must be specified in configuartion");
		}
		if ( strlen((char *)runtime_id) > RUNTIME_ID_MAX_LENGTH - 1) {
			plc_elog(ERROR, "runtime id should not be longer than 63 bytes.");
		}
		/* find the corresponding runtime config*/
		conf_entry = (runtimeConfEntry *) hash_search(runtime_conf_table,  (const void *) runtime_id, HASH_ENTER, &foundPtr);

		/*check if runtime id already exists in hash table.*/
		if (foundPtr) {
			plc_elog(ERROR, "Detecting duplicated runtime id %s in configuration file", runtime_id);
		}

		/* First iteration - parse name, container_id and memory_mb and count the
		 * number of shared directories for later allocation of related structure */

		/*runtime_id will be freed with conf_entry*/
		conf_entry->memoryMb = 1024;
		conf_entry->cpuShare = 1024;
		conf_entry->useContainerLogging = false;
		conf_entry->enableNetwork = false;
		conf_entry->resgroupOid = InvalidOid;
		conf_entry->useUserControl = false;
		conf_entry->roles = NULL;
		conf_entry->backend = NULL;

		for (cur_node = node->children; cur_node; cur_node = cur_node->next) {
			if (cur_node->type == XML_ELEMENT_NODE) {
				int processed = 0;
				value = NULL;

				if (xmlStrcmp(cur_node->name, (const xmlChar *) "id") == 0) {
					processed = 1;
				}

				if (xmlStrcmp(cur_node->name, (const xmlChar *) "image") == 0) {
					processed = 1;
					image_num++;
					value = xmlNodeGetContent(cur_node);
					conf_entry->image = plc_top_strdup((char *) value);
					if (value) {
						xmlFree((void *) value);
						value = NULL;
					}
				}

				if (xmlStrcmp(cur_node->name, (const xmlChar *) "command") == 0) {
					processed = 1;
					command_num++;
					value = xmlNodeGetContent(cur_node);
					conf_entry->command = plc_top_strdup((char *) value);
					if (value) {
						xmlFree((void *) value);
						value = NULL;
					}
				}

				if (xmlStrcmp(cur_node->name, (const xmlChar *) "setting") == 0) {
					bool validSetting = false;
					processed = 1;
					value = xmlGetProp(cur_node, (const xmlChar *) "use_container_logging");
					if (value != NULL) {
						validSetting = true;
						if (strcasecmp((char *) value, "yes") == 0) {
							conf_entry->useContainerLogging = true;
						} else if (strcasecmp((char *) value, "no") == 0) {
							conf_entry->useContainerLogging = false;
						} else {
							plc_elog(ERROR, "SETTING element <use_container_logging> only accepted \"yes\" or"
								"\"no\" only, current string is %s", value);
						}
						xmlFree((void *) value);
						value = NULL;
					}
					value = xmlGetProp(cur_node, (const xmlChar *) "memory_mb");
					if (value != NULL) {
						long memorySize = pg_atoi((char *) value, sizeof(int), 0);
						validSetting = true;

						if (memorySize <= 0) {
							plc_elog(ERROR, "container memory size couldn't be equal or less than 0, current string is %s", value);
						} else {
							conf_entry->memoryMb = memorySize;
						}
						xmlFree((void *) value);
						value = NULL;
					}
					value = xmlGetProp(cur_node, (const xmlChar *) "cpu_share");
					if (value != NULL) {
						long cpuShare = pg_atoi((char *) value, sizeof(int), 0);
						validSetting = true;

						if (cpuShare <= 0) {
							plc_elog(ERROR, "container cpu share couldn't be equal or less than 0, current string is %s", value);
						} else {
							conf_entry->cpuShare = cpuShare;
						}
						xmlFree((void *) value);
						value = NULL;
					}

					// enable or disable network in container. default is no
					value = xmlGetProp(cur_node, (const xmlChar *) "enable_network");
					if (value != NULL) {
						validSetting = true;
						if (strcasecmp((char *) value, "yes") == 0) {
							conf_entry->enableNetwork = true;
						} else if (strcasecmp((char *) value, "no") == 0) {
							conf_entry->enableNetwork = false;
						} else {
							plc_elog(ERROR, "SETTING element <enable_network> only accepted \"yes\" or "
								"\"no\" only, current string is %s", value);
						}

						xmlFree((void *) value);
						value = NULL;

						// network will a new attack surface. enable strict permission check
						if (conf_entry->enableNetwork == true) {
							conf_entry->useUserControl = true;
						}
					}

					value = xmlGetProp(cur_node, (const xmlChar *) "resource_group_id");
					if (value != NULL) {
						Oid resgroupOid;
						validSetting = true;
						if (strlen((char *) value) == 0) {
							plc_elog(ERROR, "SETTING length of element <resource_group_id> is zero");
						}
						resgroupOid = (Oid) pg_atoi((char *) value, sizeof(int), 0);

#ifndef PLC_PG
						if (resgroupOid == InvalidOid || GetResGroupNameForId(resgroupOid) == NULL) {
							plc_elog(ERROR, "SETTING element <resource_group_id> must be a resource group id in greenplum. " "Current setting is: %s", (char * ) value);
						}
#if PG_VERSION_NUM < 120000 // gpdb7 removed RESGROUP_MEMORY_AUDITOR_CGROUP
						int32 memAuditor = GetResGroupMemAuditorForId(resgroupOid, AccessShareLock);
						if (memAuditor != RESGROUP_MEMORY_AUDITOR_CGROUP) {
							plc_elog(ERROR, "SETTING element <resource_group_id> must be a resource group with memory_auditor type cgroup.");
						}
#endif
#endif
						conf_entry->resgroupOid = resgroupOid;
						xmlFree((void *) value);
						value = NULL;
					}

					value = xmlGetProp(cur_node, (const xmlChar *) "roles");
					if (value != NULL) {
						validSetting = true;
						if (strlen((char *) value) == 0) {
							plc_elog(ERROR, "SETTING length of element <roles> is zero");
						}
						conf_entry->roles = plc_top_strdup((char *) value);
						conf_entry->useUserControl = true;
						xmlFree((void *) value);
						value = NULL;
					}

					if (!validSetting) {
						plc_elog(ERROR, "Unrecognized setting options, please check the configuration file: %s", conf_entry->runtimeid);
					}

				}

				if (xmlStrcmp(cur_node->name, (const xmlChar *) "shared_directory") == 0) {
					num_shared_dirs++;
					processed = 1;
				}

				if (xmlStrcmp(cur_node->name, (const xmlChar *) "device_request") == 0) {
					num_device_request++;
					processed = 1;

					if (xmlHasProp(cur_node, XC("type")) == NULL) {
						plc_elog(ERROR, "SETTING <device_request> need \"type\" property");
					}
				}

				if (xmlStrcmp(cur_node->name, XC("backend")) == 0) {
					value = xmlGetProp(cur_node, XC("name"));

					if (value == NULL) {
						plc_elog(ERROR, "runtime \"%s\": SETTING <backend> need \"name\" property", conf_entry->runtimeid);
					}

					conf_entry->backend = take_plcbackend_byname((const char*)value);
					if (conf_entry->backend == NULL) {
						plc_elog(ERROR, "runtime \"%s\": Can not find any backend named with \"%s\"",
								conf_entry->runtimeid, value);
					}

					if (conf_entry->enableNetwork == false && conf_entry->backend->tag == PLC_BACKEND_REMOTE_DOCKER) {
						plc_elog(ERROR, "runtime \"%s\": <enable_network> must be true when using remote_docker as backend." , conf_entry->runtimeid);
					}

					processed = 1;
					xmlFree(value);
				}

				/* If the tag is not known - we raise the related error */
				if (processed == 0) {
					plc_elog(ERROR, "Unrecognized element '%s' inside of container specification",
						 cur_node->name);
				}
			}
		}

		if (image_num > 1) {
			plc_elog(ERROR, "There are more than one 'image' subelement in a runtime element %s", conf_entry->runtimeid);
		}
		else if (image_num < 1) {
			plc_elog(ERROR, "Lack of 'image' subelement in a runtime element %s", conf_entry->runtimeid);
		}

		if (command_num > 1) {
			plc_elog(ERROR, "There are more than one 'command' subelement in a runtime element %s", conf_entry->runtimeid);
		}
		else if (command_num < 1) {
			plc_elog(ERROR, "Lack of 'command' subelement in a runtime element %s", conf_entry->runtimeid);
		}

		/* Process the shared directories */
		conf_entry->nSharedDirs = num_shared_dirs;
		conf_entry->sharedDirs = NULL;
		if (num_shared_dirs > 0) {
			int i = 0;
			int j = 0;

			/* Allocate in top context as it should live between function calls */
			conf_entry->sharedDirs = PLy_malloc(num_shared_dirs * sizeof(plcSharedDir));
			for (cur_node = node->children; cur_node; cur_node = cur_node->next) {
				if (cur_node->type == XML_ELEMENT_NODE &&
					xmlStrcmp(cur_node->name, (const xmlChar *) "shared_directory") == 0) {

					value = xmlGetProp(cur_node, (const xmlChar *) "host");
					if (value == NULL) {
						plc_elog(ERROR, "Configuration tag 'shared_directory' has a mandatory element"
							" 'host' that is not found: %s", conf_entry->runtimeid);
					}
					conf_entry->sharedDirs[i].host = plc_top_strdup((char *) value);
					xmlFree((void *) value);
					value = xmlGetProp(cur_node, (const xmlChar *) "container");
					if (value == NULL) {
						plc_elog(ERROR, "Configuration tag 'shared_directory' has a mandatory element"
							" 'container' that is not found: %s", conf_entry->runtimeid);
					}
					/* Shared folders will not be created a lot, so using array to search duplicated
					 * container path is enough.
					 * */
					for (j =0; j< i; j++) {
						if (strcasecmp((char *) value, conf_entry->sharedDirs[j].container) == 0) {
							plc_elog(ERROR, "Container path cannot be the same in 'shared_directory' element "
									"in the runtime %s", conf_entry->runtimeid);
						}
					}
					conf_entry->sharedDirs[i].container = plc_top_strdup((char *) value);
					xmlFree((void *) value);
					value = xmlGetProp(cur_node, (const xmlChar *) "access");
					if (value == NULL) {
						plc_elog(ERROR, "Configuration tag 'shared_directory' has a mandatory element"
							" 'access' that is not found: %s", conf_entry->runtimeid);
					} else if (strcmp((char *) value, "ro") == 0) {
						conf_entry->sharedDirs[i].mode = PLC_ACCESS_READONLY;
					} else if (strcmp((char *) value, "rw") == 0) {
						conf_entry->sharedDirs[i].mode = PLC_ACCESS_READWRITE;
					} else {
						plc_elog(ERROR, "Directory access mode should be either 'ro' or 'rw', but passed value is '%s': %s", value, conf_entry->runtimeid);
					}
					xmlFree((void *) value);
					value = NULL;
					i += 1;
				}
			}
		}

		conf_entry->ndevicerequests = num_device_request;
		conf_entry->devicerequests = NULL;
		if (conf_entry->ndevicerequests > 0) {
			int devicerequests_index = 0;
			conf_entry->devicerequests = PLy_malloc(conf_entry->ndevicerequests * sizeof(plcDeviceRequest));
			memset(conf_entry->devicerequests, 0, conf_entry->ndevicerequests * sizeof(plcDeviceRequest));

			for (cur_node = node->children; cur_node != NULL; cur_node = cur_node->next) {
				if (cur_node->type != XML_ELEMENT_NODE ||
					xmlStrcmp(cur_node->name, (const xmlChar*)"device_request") != 0) {
					// skip all not '<device_request />' node
					continue;
				}

				plcDeviceRequest *req = &conf_entry->devicerequests[devicerequests_index];
				devicerequests_index++;

				for (xmlNode *sub = cur_node->children; sub != NULL; sub = sub->next) {
					if (sub->type != XML_ELEMENT_NODE)
						continue;

					if (xmlStrcmp(sub->name, (const xmlChar*)"deviceid") == 0)
						req->ndeviceid++;
					else if (xmlStrcmp(sub->name, (const xmlChar*)"capacity") == 0)
						req->ncapabilities++;
				}

				req->deviceid = PLy_malloc(sizeof(char*) * req->ndeviceid);
				memset(req->deviceid, 0, sizeof(char*) * req->ndeviceid);

				req->ncapabilities += 1; // add one more default capacity
				req->capabilities = PLy_malloc(sizeof(char*) * req->ncapabilities);
				memset(req->capabilities, 0, sizeof(char*) * req->ncapabilities);
				{ // the default capacity
					value = xmlGetProp(cur_node, (const xmlChar*)"type");
					req->capabilities[req->ncapabilities - 1] = plc_top_strdup((const char*)value);
					xmlFree(value);
					value = NULL;
				}

				int c_i = 0, dev_i = 0;
				for (xmlNode *sub = cur_node->children; sub != NULL; sub = sub->next) {
					if (sub->type != XML_ELEMENT_NODE)
						continue;

					if (xmlStrcmp(sub->name, (const xmlChar*)"deviceid") == 0) {
						XML_READ_TEXT(req->deviceid[dev_i++], sub);
					} else if (xmlStrcmp(sub->name, (const xmlChar*)"capacity") == 0) {
						XML_READ_TEXT(req->capabilities[c_i++], sub);
					} else if (xmlStrcmp(sub->name, (const xmlChar*)"driver") == 0) {
						XML_READ_TEXT(req->driver, sub);
					}
				}

				// <device_request all="true" /> or empty device list means enable all device
				// set ndeviceid to -1 means enable all device
				if (req->ndeviceid == 0 || xmlHasProp(cur_node, (const xmlChar *)"all") != NULL) {
					for (int i  = 0; i < req->ndeviceid; i ++) {
						pfree(req->deviceid[i]);
					}
					pfree(req->deviceid);

					req->ndeviceid = -1;
					req->deviceid = NULL;
				}

				// when assign a GPU, may need extra permissions. enable strict permission check
				if (req->ndeviceid != 0) {
					conf_entry->useUserControl = true;
				}
			}
		}

		// apply default backend
		if (conf_entry->backend == NULL) {
			for (int i = 0; i < nruntime_backend_table; i++) {
				if (strcmp(runtime_backend_table[i].name, "default") == 0)
					conf_entry->backend = &runtime_backend_table[i];
			}
		}
		Assert(conf_entry->backend != NULL);
	}
	PG_CATCH();
	{
		if (value != NULL) {
			xmlFree((void *) value);
			value = NULL;
		}

		if (runtime_conf_table != NULL && runtime_id != NULL) {
			/* remove the broken runtime config entry in hash table*/
			hash_search(runtime_conf_table,  (const void *) runtime_id, HASH_REMOVE, NULL);
		}

		PG_RE_THROW();
	}
	PG_END_TRY();

	return ;
}

/* Function returns an array of plcContainerConf structures based on the contents
 * of passed XML document tree. Returns NULL on failure */
static void get_runtime_configurations(xmlNode *node) {
	xmlNode *cur_node = NULL;

	/* Validation that the root node matches the expected specification */
	if (xmlStrcmp(node->name, (const xmlChar *) "configuration") != 0) {
		plc_elog(ERROR, "Wrong XML configuration provided. Expected 'configuration'"
			" as root element, got '%s' instead", node->name);
	}

	parse_runtime_backend_configuration(node);

	/* Iterating through the list of containers to parse them */
	for (cur_node = node->children; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE &&
		    xmlStrcmp(cur_node->name, (const xmlChar *) "runtime") == 0) {
			parse_runtime_configuration(cur_node);
		}
	}

	/* If no container definitions found - error */
	if (hash_get_num_entries(runtime_conf_table) == 0) {
		plc_elog(ERROR, "Did not find a single 'runtime' declaration in configuration");
	}

	return ;
}

/* Safe way to deallocate container configuration list structure */
static void free_runtime_conf_entry(runtimeConfEntry *entry) {
	if (entry->image)
		pfree(entry->image);
	if (entry->command)
		pfree(entry->command);
	if (entry->roles)
		pfree(entry->roles);

	for (int i = 0; i < entry->nSharedDirs; i++) {
		pfree_null(entry->sharedDirs[i].host);
		pfree_null(entry->sharedDirs[i].container);
	}

	pfree_null(entry->sharedDirs);

	for (int i = 0; i < entry->ndevicerequests; i++) {
		plcDeviceRequest *d = &entry->devicerequests[i];

		pfree_null(d->driver);

		for (int j = 0; j < d->ndeviceid; j++)
			pfree(d->deviceid[j]);

		pfree_null(d->deviceid);

		for (int j = 0; j < d->ncapabilities; j++)
			pfree(d->capabilities[j]);

		pfree_null(d->capabilities);
	}

	pfree_null(entry->devicerequests);
}


static void free_runtime_backend_entry(plcBackend *entry, bool release_self) {
	if (!entry)
		return;

	pfree_null(entry->name);

	switch(entry->tag) {
		case PLC_BACKEND_REMOTE_DOCKER:
			pfree_null(entry->remotedocker.address);
			pfree_null(entry->remotedocker.username);
			pfree_null(entry->remotedocker.password);
			break;
		case PLC_BACKEND_DOCKER:
			pfree_null(entry->localdocker.uds_address);
			break;
		case PLC_BACKEND_PROCESS:
		case PLC_BACKEND_UNIMPLEMENT:
			Assert(!"BUG: not implement");
	}

	if (release_self)
		pfree(entry);
}

static void print_runtime_configurations() {
	if (runtime_conf_table == NULL) {
		return;
	}

	for (int i = 0; i < nruntime_backend_table; i++) {
		plcBackend *b = &runtime_backend_table[i];
		plc_elog(INFO, "Backend '%s' configuration", b->name);
		plc_elog(INFO, "    type = '%s'", PLC_BACKEND_TYPE_TO_STRING(b->tag));
		if (b->tag == PLC_BACKEND_DOCKER) {
			plc_elog(INFO, "    uds_address = '%s'", b->localdocker.uds_address);
		} else if (b->tag == PLC_BACKEND_REMOTE_DOCKER) {
			plc_elog(INFO, "    address = '%s'", b->remotedocker.address);
			plc_elog(INFO, "    port = '%ld'", b->remotedocker.port);
			plc_elog(INFO, "    add segindex = '%s'", b->remotedocker.add_segment_index ? "true":"false");
			if (b->remotedocker.username != NULL) {
				plc_elog(INFO, "    username = '%s'", b->remotedocker.username);
			}
			if (b->remotedocker.password != NULL) {
				plc_elog(INFO, "    password = '*****'");
			}
		}
	}

	HASH_SEQ_STATUS hash_status;
	hash_seq_init(&hash_status, runtime_conf_table);

	runtimeConfEntry *conf_entry;
	while ((conf_entry = (runtimeConfEntry *) hash_seq_search(&hash_status)) != NULL)
	{
		plc_elog(INFO, "Container '%s' configuration", conf_entry->runtimeid);
		plc_elog(INFO, "    image = '%s'", conf_entry->image);
		plc_elog(INFO, "    command = '%s'", conf_entry->command);
		plc_elog(INFO, "    memory_mb = '%d'", conf_entry->memoryMb);
		plc_elog(INFO, "    cpu_share = '%d'", conf_entry->cpuShare);
		plc_elog(INFO, "    use_container_logging = '%s'", conf_entry->useContainerLogging ? "yes" : "no");
		plc_elog(INFO, "    enable_network = '%s'", conf_entry->enableNetwork ? "yes" : "no");
		plc_elog(INFO, "    backend = '%s'", conf_entry->backend->name);
		if (conf_entry->ndevicerequests != 0) {
			for (int i = 0; i < conf_entry->ndevicerequests; i++) {
				plcDeviceRequest *req = &conf_entry->devicerequests[i];
				if (req->driver != NULL) {
					plc_elog(INFO, "    device_request[%d].driver = '%s'", i, req->driver);
				}
				if (req->ndeviceid == -1) {
					plc_elog(INFO, "    device_request[%d].deviceid = 'all'", i);
				}
				for (int j = 0; j < req->ndeviceid; j++) {
					plc_elog(INFO, "    device_request[%d].deviceid[%d] = '%s'", i, j, req->deviceid[j]);
				}
				for (int j = 0; j < req->ncapabilities; j++) {
					plc_elog(INFO, "    device_request[%d].capabilities[%d] = '%s'", i, j, req->capabilities[j]);
				}
			}
		}
		if (conf_entry->useUserControl) {
			plc_elog(INFO, "    useUserControl = 'true'");
		}
		if (conf_entry->roles != NULL) {
			plc_elog(INFO, "    allowed roles list = '%s'", conf_entry->roles);
		}
		if (conf_entry->resgroupOid != InvalidOid) {
			plc_elog(INFO, "    resource group id = '%u'", conf_entry->resgroupOid);
		}
		for (int j = 0; j < conf_entry->nSharedDirs; j++) {
			plc_elog(INFO, "    shared directory from host '%s' to container '%s'",
				 conf_entry->sharedDirs[j].host,
				 conf_entry->sharedDirs[j].container);
			if (conf_entry->sharedDirs[j].mode == PLC_ACCESS_READONLY) {
				plc_elog(INFO, "        access = readonly");
			} else {
				plc_elog(INFO, "        access = readwrite");
			}
		}
	}
}

static int plc_refresh_container_config(bool verbose) {
	xmlDoc* volatile doc = NULL;
	char filename[1024];
 #ifdef PLC_PG
    char data_directory[1024];
	char *env_str;
 #endif
	init_runtime_configurations();
	/*
	 * this initialize the library and check potential ABI mismatches
	 * between the version it was compiled for and the actual shared
	 * library used.
	 */
	LIBXML_TEST_VERSION

 #ifdef PLC_PG
    if ((env_str = getenv("PGDATA")) == NULL)
        plc_elog (ERROR, "PGDATA is not set");
	snprintf(data_directory, sizeof(data_directory), "%s", env_str );
 #endif
	/* Parse the file and get the DOM */
	sprintf(filename, "%s/%s", DataDir, PLC_PROPERTIES_FILE);

	PG_TRY();
	{
		doc = xmlReadFile(filename, NULL, 0);
		if (doc == NULL) {
			ereport(ERROR,
				(errmsg("Error: could not parse file %s, wrongly formatted XML or missing configuration file\n", filename),
				 errhint("run plcontainer runtime-edit to check the configuration file"))
			);
			return -1;
		}

		get_runtime_configurations(xmlDocGetRootElement(doc));
	}
	PG_CATCH();
	{
		if (doc != NULL) {
			xmlFreeDoc(doc);
		}

		PG_RE_THROW();
	}
	PG_END_TRY();


	/* Free the document */
	xmlFreeDoc(doc);

	/* Free the global variables that may have been allocated by the parser */
	xmlCleanupParser();

	if (hash_get_num_entries(runtime_conf_table) == 0) {
		return -1;
	}

	if (verbose) {
		print_runtime_configurations();
	}

	return 0;
}

static int plc_show_container_config() {
	int res = 0;

	if (runtime_conf_table == NULL) {
		res = plc_refresh_container_config(false);
		if (res != 0)
			return -1;
	}

	if (runtime_conf_table == NULL || hash_get_num_entries(runtime_conf_table) == 0) {
		return -1;
	}

	print_runtime_configurations();
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

runtimeConfEntry *plc_get_runtime_configuration(char *runtime_id) {
	int res = 0;
	runtimeConfEntry *entry = NULL;

	if (runtime_conf_table == NULL || hash_get_num_entries(runtime_conf_table) == 0) {
		res = plc_refresh_container_config(0);
		if (res < 0) {
			return NULL;
		}
	}

	/* find the corresponding runtime config*/
	entry = (runtimeConfEntry *) hash_search(runtime_conf_table,  (const void *) runtime_id, HASH_FIND, NULL);

	return entry;
}


backendConnectionInfo *runtime_conf_copy_backend_connection_info(const backendConnectionInfo *a) {
	backendConnectionInfo *r = PLy_malloc(sizeof(backendConnectionInfo));

	r->tag = a->tag;
	switch(a->tag) {
		case PLC_BACKEND_DOCKER:
			r->plcBackendLocalDocker.uds_address = plc_top_strdup(a->plcBackendLocalDocker.uds_address);
			break;
		case PLC_BACKEND_REMOTE_DOCKER:
			r->plcBackendRemoteDocker.hostname = plc_top_strdup(a->plcBackendRemoteDocker.hostname);
			r->plcBackendRemoteDocker.port = a->plcBackendRemoteDocker.port;
			r->plcBackendRemoteDocker.username = plc_top_strdup_null(a->plcBackendRemoteDocker.username);
			r->plcBackendRemoteDocker.password = plc_top_strdup_null(a->plcBackendRemoteDocker.password);
			break;
		case PLC_BACKEND_PROCESS:
			break;
		case PLC_BACKEND_UNIMPLEMENT:
			Assert(!"not implement");
			break;
	}
	return r;
}

backendConnectionInfo *runtime_conf_get_backend_connection_info(const plcBackend *backend) {
	backendConnectionInfo *info = palloc0(sizeof(backendConnectionInfo));

	switch(backend->tag) {
		case PLC_BACKEND_DOCKER: {
			char *address = "/var/run/docker.sock";
			if (backend->localdocker.uds_address != NULL) {
				address = backend->localdocker.uds_address;
			}

			info->tag = PLC_BACKEND_DOCKER;
			info->plcBackendLocalDocker.uds_address = plc_top_strdup(address);

			break;
		}
		case PLC_BACKEND_REMOTE_DOCKER: {
			char *address = "localhost";
			if (backend->remotedocker.address != NULL) {
				address = backend->remotedocker.address;
			}

			int sz = strlen(address) + sizeof("2147483647") + 1;
			char *hostname = palloc(sizeof(char) * sz);

			if (backend->remotedocker.add_segment_index) {
				// prevent "-1" in address. 0 -> coordinater. 1 -> segment 0.
				uint32_t segindex = GpIdentity.segindex + 1;
				snprintf(hostname, sz, "%d.%s", segindex, address);
			} else {
				snprintf(hostname, sz, "%s", address);
			}

			info->tag = PLC_BACKEND_REMOTE_DOCKER;
			info->plcBackendRemoteDocker.hostname = hostname;
			info->plcBackendRemoteDocker.port = backend->remotedocker.port;
			info->plcBackendRemoteDocker.username = plc_top_strdup_null(backend->remotedocker.username);
			info->plcBackendRemoteDocker.password = plc_top_strdup_null(backend->remotedocker.password);

			break;
		}
		case PLC_BACKEND_PROCESS:
			info->tag = PLC_BACKEND_PROCESS;

			break;
		case PLC_BACKEND_UNIMPLEMENT:
			plc_elog(ERROR, "%s: BUG! unimplemented", __func__);
			break;
	}

	return info;
}


void runtime_conf_free_backend_connection_info(backendConnectionInfo *info) {
	if (info == NULL)
		return;

	switch(info->tag) {
		case PLC_BACKEND_DOCKER:
			pfree(info->plcBackendLocalDocker.uds_address);
			break;
		case PLC_BACKEND_REMOTE_DOCKER:
			pfree(info->plcBackendRemoteDocker.hostname);
			pfree_null(info->plcBackendRemoteDocker.username);
			pfree_null(info->plcBackendRemoteDocker.password);
			break;
		case PLC_BACKEND_PROCESS:
			break;
		case PLC_BACKEND_UNIMPLEMENT:
			break;
	}

	pfree(info);
}


runtimeConnectionInfo* runtime_conf_copy_runtime_connection_info(const runtimeConnectionInfo *a) {
	runtimeConnectionInfo *r = PLy_malloc(sizeof(runtimeConnectionInfo));

	r->tag = a->tag;
	r->identity = plc_top_strdup(a->identity);

	switch(r->tag) {
		case PLC_RUNTIME_CONNECTION_TCP:
			r->connection_tcp.hostname = plc_top_strdup(a->connection_tcp.hostname);
			r->connection_tcp.port = a->connection_tcp.port;
			break;
		case PLC_RUNTIME_CONNECTION_UDS:
			r->connection_uds.uds_address = plc_top_strdup(a->connection_uds.uds_address);
			break;
		case PLC_RUNTIME_CONNECTION_UNKNOWN:
			Assert(!"unimplement");
	}

	return r;
}

runtimeConnectionInfo* runtime_conf_get_runtime_connection_info(const backendConnectionInfo *a){
	runtimeConnectionInfo *r = palloc0(sizeof(runtimeConnectionInfo));

	switch(a->tag) {
		case PLC_BACKEND_REMOTE_DOCKER:
			r->tag = PLC_RUNTIME_CONNECTION_TCP;
			// container and the docker host shared the same address
			r->connection_tcp.hostname = plc_top_strdup(a->plcBackendRemoteDocker.hostname);
			r->connection_tcp.port = 0;
			break;
		case PLC_BACKEND_DOCKER:
		case PLC_BACKEND_PROCESS:
			r->tag = PLC_RUNTIME_CONNECTION_UDS;
			r->connection_uds.uds_address = NULL;
			break;
		case PLC_BACKEND_UNIMPLEMENT:
			Assert(!"unimplement");
	}

	return r;
}

void runtime_conf_free_runtime_connection_info(runtimeConnectionInfo *info) {
	if (info == NULL)
		return;

	pfree(info->identity);

	switch(info->tag) {
		case PLC_RUNTIME_CONNECTION_TCP:
			pfree(info->connection_tcp.hostname);
			break;
		case PLC_RUNTIME_CONNECTION_UDS:
			pfree(info->connection_uds.uds_address);
			break;
		case PLC_RUNTIME_CONNECTION_UNKNOWN:
			break;
	}

	pfree(info);
}

int generate_sharing_options_and_uds_address(const runtimeConfEntry *conf, const backendConnectionInfo *backend,
											 const int container_slot, runtimeConnectionInfo *connection,
											 char **docekr_sharing_options) {
	// generate sharing options for more backend
	(void)backend;

	StringInfoData buffer = {};
	initStringInfo(&buffer);

	// the format '(\s+):(\s+):[ro|rw]'.join(',')
	//              ^^^   ^^^
	//             host   container
	int has_previous = false;

	for (int i = 0; i < conf->nSharedDirs; i++) {
		const plcSharedDir *dir = &conf->sharedDirs[i];

		if (has_previous) {
			appendStringInfo(&buffer, ",");
		}
		has_previous = true;

		switch (dir->mode) {
			case PLC_ACCESS_READONLY:
				appendStringInfo(&buffer, "\"%s:%s:ro\"", dir->host, dir->container);
				break;
			case PLC_ACCESS_READWRITE:
				appendStringInfo(&buffer, "\"%s:%s:rw\"", dir->host, dir->container);
				break;
			default:
				snprintf(backend_error_message, sizeof(backend_error_message),
						"cannot determine directory sharing mode: %d",
						conf->sharedDirs[i].mode);
				goto err;
		}
	}

	// append a unix domain socket to shared dir for sending message to pl client
	if (connection->tag == PLC_RUNTIME_CONNECTION_UDS) {
		if (has_previous) {
			appendStringInfo(&buffer, ",");
		}
		has_previous = true;

		int uds_count = domain_socket_no++;
		size_t _old = buffer.len;

		// directory on host:
		// IPC_GPDB_BASE_DIR + "." + PID + "." + counter + "." + container_slot
		// eg: /tmp/plcontainer.1.0.0
		appendStringInfo(&buffer,"\"%s.%d.%d.%d:%s:rw\"",
				IPC_GPDB_BASE_DIR, getpid(), uds_count, container_slot,
				IPC_CLIENT_DIR);

		size_t sz = buffer.len - _old + sizeof(UDS_SHARED_FILE);

		// do the format again to create the directory
		char *_dirname = palloc0(sz);
		int n = snprintf(_dirname, sz, "%s.%d.%d.%d", IPC_GPDB_BASE_DIR, getpid(), uds_count, container_slot);
		Assert(n < sz);

		if (mkdir(_dirname, S_IRWXU) < 0 && errno != EEXIST) {
			snprintf(backend_error_message, sizeof(backend_error_message),
			         "cannot create directory %s: %s",
			         _dirname, strerror(errno));
			goto err;
		}

		// do the format again to get the UDS filename
		if (backend->tag == PLC_BACKEND_PROCESS) {
			n = snprintf(_dirname, sz, "%s/%s", IPC_GPDB_BASE_DIR, UDS_SHARED_FILE);
		} else {
			n = snprintf(_dirname, sz, "%s.%d.%d.%d/%s", IPC_GPDB_BASE_DIR, getpid(), uds_count, container_slot, UDS_SHARED_FILE);
		}
		Assert(n < sz);

		// free out side
		connection->connection_uds.uds_address = _dirname;
	}

	// StringInfo guarante the string terminated with '\0'
	*docekr_sharing_options = buffer.data;
	return 0;

err:
	*docekr_sharing_options = NULL;
	pfree(buffer.data);
	return -1;
}

bool plc_check_user_privilege(char *roles) {
	List *elemlist;
	ListCell *l;
	Oid currentUserOid;

	if (roles == NULL) {
		ereport(WARNING,
				(errmsg("plcontainer: To access network or physical device, user permission needs to be granted explicitly in the runtime config."),
				errhint("set user name in <setting roles=\"<user name>\" />")));

		return false;
	}

	if (!SplitIdentifierString(roles, ',', &elemlist))
	{
		list_free(elemlist);
		elog(ERROR, "Could not get role list from %s, please check it again", roles);

		return false;
	}

	currentUserOid = GetUserId();

	if (currentUserOid == InvalidOid){
		elog(ERROR, "Could not get current user Oid");
	}

	foreach(l, elemlist)
	{
		char *role = (char*) lfirst(l);
		Oid roleOid = get_role_oid(role, true);
		if (is_member_of_role(currentUserOid, roleOid)){
			return true;
		}
	}

	return false;
}

const char* PLC_BACKEND_TYPE_TO_STRING(PLC_BACKEND_TYPE b) {
	switch(b) {
		case PLC_BACKEND_DOCKER:
			return "docker";
		case PLC_BACKEND_REMOTE_DOCKER:
			return "remote docker";
		case PLC_BACKEND_PROCESS:
			return "process";
		case PLC_BACKEND_UNIMPLEMENT:
			return "unimplement";
	}

	Assert(!"unreachable");
	return "";
}

static void xml_read_backend(void *ctx, size_t off, xmlNode *node) {
	plcBackend *backend = &((plcBackend*)ctx)[off];
	char *type_str = NULL;

	XML_READ_PROPERTY(node, 2, (XML_FIELD[]){
		{
			.tag = XML_TYPE_text, .notnull = true,
			.name = XC("name"),
			.N = &XML_TYPE_SIGNLE_ELEMENT,
			.ptr = &backend->name,
		},
		{
			.name = XC("type"),
			.tag = XML_TYPE_text,
			.notnull = true,
			.N = &XML_TYPE_SIGNLE_ELEMENT,
			.ptr = &type_str,
		},
	});

	if (strcmp(type_str, "docker") == 0 ||
		strcmp(type_str, "local_docker") == 0) {
		backend->tag = PLC_BACKEND_DOCKER;
	} else if (strcmp(type_str, "remote_docker") == 0) {
		// https://docs.docker.com/engine/reference/commandline/dockerd/#bind-docker-to-another-host-port-or-a-unix-socket
		backend->tag = PLC_BACKEND_REMOTE_DOCKER;
	} else {
		plc_elog(ERROR, "<backend name=\"%s\"/> the 'type' should be: "
				"['docker', 'local_docker', 'remote_docker']. current %s", backend->name, type_str);
	}

	pfree_null(type_str);

	switch (backend->tag) {
		case PLC_BACKEND_REMOTE_DOCKER:
			XML_READ_CONTENT(node, 5, (XML_FIELD[]){
				{
					.name = XC("add_segment_index"),
					.tag = XML_TYPE_boolean,
					.N = &XML_TYPE_SIGNLE_ELEMENT,
					.ptr = &backend->remotedocker.add_segment_index,
				},
				{
					.name = XC("address"),
					.tag = XML_TYPE_text,
					.notnull = true,
					.N = &XML_TYPE_SIGNLE_ELEMENT,
					.ptr = &backend->remotedocker.address,
				},
				{
					.name = XC("port"),
					.tag = XML_TYPE_integer,
					.notnull = true,
					.N = &XML_TYPE_SIGNLE_ELEMENT,
					.ptr = &backend->remotedocker.port,
				},
				{
					.name = XC("username"),
					.tag = XML_TYPE_text,
					.N = &XML_TYPE_SIGNLE_ELEMENT,
					.ptr = &backend->remotedocker.username,
				},
				{
					.name = XC("password"),
					.tag = XML_TYPE_text,
					.N = &XML_TYPE_SIGNLE_ELEMENT,
					.ptr = &backend->remotedocker.password,
				},
			});
			break;
		case PLC_BACKEND_DOCKER:
			XML_READ_CONTENT(node, 1, (XML_FIELD[]){
				{
					.name = XC("address"),
					.tag = XML_TYPE_text,
					.notnull = true,
					.N = &XML_TYPE_SIGNLE_ELEMENT,
					.ptr = &backend->localdocker.uds_address,
				},
			});
			break;
		case PLC_BACKEND_PROCESS:
			break;
		case PLC_BACKEND_UNIMPLEMENT:
			Assert(!"unreachable");
			break;
	}
}

static void parse_runtime_backend_configuration(xmlNode *root) {
	// <configuration> <- current root
	//   <backend />
	//   <backend />
	// </configuration>
	XML_READ_CONTENT(root, 1, (XML_FIELD[]){
		{
			.name = XC("backend"),
			.tag = XML_TYPE_object,
			.mem_size = sizeof(plcBackend),
			.notnull = false,
			.N = &nruntime_backend_table,
			.ptr = (void*)&runtime_backend_table,
			.parse = xml_read_backend,
		},
	});

	// add the default backend
	const int old = nruntime_backend_table;

	plcBackend *new = MemoryContextAllocZero(TopMemoryContext, sizeof(plcBackend) * (old + 1));
	new[0].name = plc_top_strdup("default");
	new[0].tag = PLC_BACKEND_DOCKER;
	if (plc_backend_type_string && strcmp(plc_backend_type_string, "process") == 0)
		new[0].tag = PLC_BACKEND_PROCESS;
	new[0].localdocker.uds_address = plc_top_strdup("/var/run/docker.sock");

	memcpy(&new[1], runtime_backend_table, old * sizeof(plcBackend));

	runtime_backend_table = new;
	nruntime_backend_table += 1;
}

const plcBackend *take_plcbackend_byname(const char *name) {
	if (runtime_backend_table == NULL)
		plc_refresh_container_config(false);

	for (int i = 0; i < nruntime_backend_table; i++) {
		const plcBackend *b = &runtime_backend_table[i];
		if (strcmp(b->name, name) == 0)
			return b;
	}

	return NULL;
}
