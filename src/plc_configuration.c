/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */


#include <libxml/tree.h>
#include <libxml/parser.h>

#include "postgres.h"
#include "utils/builtins.h"
#include "utils/guc.h"

#include "common/comm_utils.h"
#include "plcontainer.h"
#include "plc_configuration.h"

static plcContainerConf *plcContConf = NULL;
static int plcNumContainers = 0;

static int parse_container(xmlNode *node, plcContainerConf *conf);
static plcContainerConf *get_containers(xmlNode *node, int *size);
static void free_containers(plcContainerConf *conf, int size);
static void print_containers(plcContainerConf *conf, int size);

PG_FUNCTION_INFO_V1(refresh_plcontainer_config);
PG_FUNCTION_INFO_V1(show_plcontainer_config);

/* Function parses the container XML definition and fills the passed
 * plcContainerConf structure that should be already allocated */
static int parse_container(xmlNode *node, plcContainerConf *conf) {
    xmlNode *cur_node = NULL;
    xmlChar *value = NULL;
    int has_name = 0;
    int has_id = 0;
    int has_command = 0;
    int num_shared_dirs = 0;

    /* First iteration - parse name, container_id and memory_mb and count the
     * number of shared directories for later allocation of related structure */
    conf->memoryMb = -1;
    for (cur_node = node->children; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            int processed = 0;
            value = NULL;

            if (xmlStrcmp(cur_node->name, (const xmlChar *)"name") == 0) {
                processed = 1;
                has_name = 1;
                value = xmlNodeGetContent(cur_node);
                conf->name = plc_top_strdup((char*)value);
            }

            if (xmlStrcmp(cur_node->name, (const xmlChar *)"image") == 0) {
                processed = 1;
                has_id = 1;
                value = xmlNodeGetContent(cur_node);
                conf->dockerid = plc_top_strdup((char*)value);
            }

            if (xmlStrcmp(cur_node->name, (const xmlChar *)"command") == 0) {
                processed = 1;
                has_command = 1;
                value = xmlNodeGetContent(cur_node);
                conf->command = plc_top_strdup((char*)value);
            }

            if (xmlStrcmp(cur_node->name, (const xmlChar *)"memory_mb") == 0) {
                processed = 1;
                value = xmlNodeGetContent(cur_node);
                conf->memoryMb = pg_atoi((char*)value, sizeof(int), 0);
            }

            if (xmlStrcmp(cur_node->name, (const xmlChar *)"shared_directory") == 0) {
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

    if (has_name == 0) {
        elog(ERROR, "Container name in tag <name> must be specified in configuartion");
        return -1;
    }

    if (has_id == 0) {
        elog(ERROR, "Container ID in tag <image> must be specified in configuration");
        return -1;
    }

    if (has_command == 0) {
        elog(ERROR, "Container startup command in tag <command> must be specified in configuration");
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
                    xmlStrcmp(cur_node->name, (const xmlChar *)"shared_directory") == 0) {

                value  = xmlGetProp(cur_node, (const xmlChar *)"host");
                if (value == NULL) {
                    elog(ERROR, "Configuration tag 'shared_directory' has a mandatory element"
                         " 'host' that is not found");
                    return -1;
                }
                conf->sharedDirs[i].host = plc_top_strdup((char*)value);
                xmlFree(value);

                value = xmlGetProp(cur_node, (const xmlChar *)"container");
                if (value == NULL) {
                    elog(ERROR, "Configuration tag 'shared_directory' has a mandatory element"
                         " 'container' that is not found");
                    return -1;
                }
                conf->sharedDirs[i].container = plc_top_strdup((char*)value);
                xmlFree(value);

                value = xmlGetProp(cur_node, (const xmlChar *)"access");
                if (value == NULL) {
                    elog(ERROR, "Configuration tag 'shared_directory' has a mandatory element"
                         " 'access' that is not found");
                    return -1;
                } else if (strcmp((char*)value, "ro") == 0) {
                    conf->sharedDirs[i].mode = PLC_ACCESS_READONLY;
                } else if (strcmp((char*)value, "rw") == 0) {
                    conf->sharedDirs[i].mode = PLC_ACCESS_READWRITE;
                } else {
                    elog(ERROR, "Directory access mode should be either 'ro' or 'rw', passed value is '%s'", value);
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
static plcContainerConf *get_containers(xmlNode *node, int *size) {
    xmlNode *cur_node = NULL;
    int nContainers = 0;
    int i = 0;
    int res = 0;
    plcContainerConf* result = NULL;

    /* Validation that the root node matches the expected specification */
    if (xmlStrcmp(node->name, (const xmlChar *)"configuration") != 0) {
        elog(ERROR, "Wrong XML configuration provided. Expected 'configuration'"
             " as root element, got '%s' instead", node->name);
        return result;
    }

    /* Iterating through the list of containers to get the count */
    for (cur_node = node->children; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE &&
                xmlStrcmp(cur_node->name, (const xmlChar *)"container") == 0) {
            nContainers += 1;
        }
    }

    /* If no container definitions found - error */
    if (nContainers == 0) {
        elog(ERROR, "Did not find a single 'container' declaration in configuration");
        return result;
    }

    result = plc_top_alloc(nContainers * sizeof(plcContainerConf));

    /* Iterating through the list of containers to parse them into plcContainerConf */
    i = 0;
    res = 0;
    for (cur_node = node->children; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE &&
                xmlStrcmp(cur_node->name, (const xmlChar *)"container") == 0) {
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
static void free_containers(plcContainerConf *conf, int size) {
    int i;
    for (i = 0; i < size; i++) {
        if (conf[i].nSharedDirs > 0 && conf[i].sharedDirs != NULL) {
            pfree(conf[i].sharedDirs);
        }
    }
    pfree(conf);
}

static void print_containers(plcContainerConf *conf, int size) {
    int i, j;
    for (i = 0; i < size; i++) {
        elog(INFO, "Container '%s' configuration", conf[i].name);
        elog(INFO, "    container_id = '%s'", conf[i].dockerid);
        elog(INFO, "    memory_mb = '%d'", conf[i].memoryMb);
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
Datum refresh_plcontainer_config(PG_FUNCTION_ARGS) {
    int res = plc_refresh_container_config(PG_GETARG_BOOL(0));
    if (res == 0) {
        PG_RETURN_TEXT_P(cstring_to_text("ok"));
    } else {
        PG_RETURN_TEXT_P(cstring_to_text("error"));
    }
}

/* Function referenced from Postgres that can update configuration on
 * specific GPDB segment */
Datum show_plcontainer_config(pg_attribute_unused() PG_FUNCTION_ARGS) {
    int res = plc_show_container_config();
    if (res == 0) {
        PG_RETURN_TEXT_P(cstring_to_text("ok"));
    } else {
        PG_RETURN_TEXT_P(cstring_to_text("error"));
    }
}

plcContainerConf *plc_get_container_config(char *name) {
    int res = 0;
    int i = 0;
    plcContainerConf *result = NULL;

    if (plcContConf == NULL || plcNumContainers == 0) {
        res = plc_refresh_container_config(0);
        if (res < 0) {
            return NULL;
        }
    }

    for (i = 0; i < plcNumContainers; i++) {
        if (strcmp(name, plcContConf[i].name) == 0) {
            result = &plcContConf[i];
            break;
        }
    }

    return result;
}

char *get_sharing_options(plcContainerConf *conf) {
    char *res = NULL;

    if (conf->nSharedDirs > 0) {
        char **volumes = NULL;
        int totallen = 0;
        char *pos;
        int i;

        volumes = palloc(conf->nSharedDirs * sizeof(char*));
        for (i = 0; i < conf->nSharedDirs; i++) {
            volumes[i] = palloc(10 + strlen(conf->sharedDirs[i].host) +
                                 strlen(conf->sharedDirs[i].container));
            if (conf->sharedDirs[i].mode == PLC_ACCESS_READONLY) {
                sprintf(volumes[i], "\"%s:%s:ro\"", conf->sharedDirs[i].host,
                        conf->sharedDirs[i].container);
            } else if (conf->sharedDirs[i].mode == PLC_ACCESS_READWRITE) {
                sprintf(volumes[i], "\"%s:%s:rw\"", conf->sharedDirs[i].host,
                        conf->sharedDirs[i].container);
            } else {
                elog(ERROR, "Cannot determine directory sharing mode");
            }
            totallen += strlen(volumes[i]);
        }

        res = palloc(totallen + 2 * conf->nSharedDirs);
        pos = res;
        for (i = 0; i < conf->nSharedDirs; i++) {
            memcpy(pos, volumes[i], strlen(volumes[i]));
            pos += strlen(volumes[i]);
            *pos = ' ';
            pos += 1;
            pfree(volumes[i]);
        }
        *pos = '\0';
        pfree(volumes);
    } else {
        res = palloc(1);
        res[0] = '\0';
    }
    return res;
}
