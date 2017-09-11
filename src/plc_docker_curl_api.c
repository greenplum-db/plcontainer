/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */


#ifdef CURL_DOCKER_API

#include "postgres.h"
#include "regex/regex.h"

#include "plc_docker_curl_api.h"
#include "plc_configuration.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

// Default location of the Docker API unix socket
static char *plc_docker_socket = "/var/run/docker.sock";

// URL prefix specifies Docker API version
#ifdef DOCKER_API_LOW
    static char *plc_docker_url_prefix = "http:/v1.19";
#else
    static char *plc_docker_url_prefix = "http:/v1.21";
#endif
/* Static functions of the Docker API module */
static plcCurlBuffer *plcCurlBufferInit();
static void plcCurlBufferFree(plcCurlBuffer *buf);
static size_t plcCurlCallback(void *contents, size_t size, size_t nmemb, void *userp);
static plcCurlBuffer *plcCurlRESTAPICall(plcCurlCallType cType,
                                         char *url,
                                         char *body,
                                         long expectedReturn,
                                         bool silent);
static int docker_parse_container_id(char *response, char **name);
static int docker_parse_port_mapping(char *response, int *port);

/* Initialize Curl response receiving buffer */
static plcCurlBuffer *plcCurlBufferInit() {
    plcCurlBuffer *buf = palloc(sizeof(plcCurlBuffer));
    buf->data = palloc(8192);   /* will be grown as needed by the realloc above */ 
    memset(buf->data, 0, 8192); /* set to zeros to avoid errors */
    buf->bufsize = 8192;        /* initial size of the buffer */
    buf->size = 0;              /* amount of data in this buffer */ 
    buf->status = 0;            /* status of the Curl call */
    return buf;
}

/* Free Curl response receiving buffer */
static void plcCurlBufferFree(plcCurlBuffer *buf) {
    pfree(buf->data);
    pfree(buf);
}

/* Curl callback for receiving a chunk of data into buffer */
static size_t plcCurlCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    plcCurlBuffer *mem = (plcCurlBuffer*)userp;

    if (mem->size + realsize + 1 > mem->bufsize) {
        mem->data = repalloc(mem->data, 3 * (mem->size + realsize + 1) / 2);
        mem->bufsize = 3 * (mem->size + realsize + 1) / 2;
        if (mem->data == NULL) {
            /* out of memory! */ 
            elog(ERROR, "not enough memory (realloc returned NULL)");
            return 0;
        }
    }

    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;

    return realsize;
}

/* Function for calling Docker REST API using Curl */
static plcCurlBuffer *plcCurlRESTAPICall(plcCurlCallType cType,
                                         char *url,
                                         char *body,
                                         long expectedReturn,
                                         bool silent) {
    CURL *curl;
    CURLcode res;
    plcCurlBuffer *buffer = plcCurlBufferInit();
    char errbuf[CURL_ERROR_SIZE];

    memset(errbuf, 0, CURL_ERROR_SIZE);

    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    if (curl) {
        char *fullurl;
        struct curl_slist *headers = NULL; // init to NULL is important

        /* Setting Docker API endpoint */
        curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, plc_docker_socket);

        /* Setting up request URL */
        fullurl = palloc(strlen(plc_docker_url_prefix) + strlen(url) + 2);
        sprintf(fullurl, "%s%s", plc_docker_url_prefix, url);
        curl_easy_setopt(curl, CURLOPT_URL, fullurl);

        /* Providing a buffer to store errors in */
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

        /* Choosing the right request type */
        switch (cType) {
            case PLC_CALL_HTTPGET:
                curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
                break;
            case PLC_CALL_POST:
                curl_easy_setopt(curl, CURLOPT_POST, 1);
                /* If the body is set - we are sending JSON, else - plain text */
                if (body != NULL) {
                    headers = curl_slist_append(headers, "Content-Type: application/json");
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
                } else {
                    headers = curl_slist_append(headers, "Content-Type: text/plain");
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
                }
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                break;
            case PLC_CALL_DELETE:
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE"); 
                break;
            default:
                elog(ERROR, "Unsupported call type for PL/Container Docker Curl API: %d", cType);
                buffer->status = -1;
                break;
        }

        /* Setting up response receive callback */
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, plcCurlCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)buffer);

        /* Calling the API */
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            size_t len = strlen(errbuf);
            if (!silent) {
                elog(ERROR, "PL/Container libcurl return code %d, error '%s'", res,
                    (len > 0) ? errbuf : curl_easy_strerror(res));
            }
            buffer->status = -2;
        } else {
            long http_code = 0;

            curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
            if (http_code == expectedReturn) {
                if (!silent) {
                    elog(DEBUG1, "Call '%s' succeeded\n", fullurl);
                    elog(DEBUG1, "Returned data: %s\n", buffer->data);
                }
                buffer->status = 0;
            } else {
                if (!silent) {
                    elog(ERROR, "Curl call to '%s' returned error code %ld, error '%s'\n",
                           fullurl, http_code, buffer->data);
                }
                buffer->status = -3;
            }
        }

        /* Freeing up full URL */
        pfree(fullurl);
    }

    /* cleanup curl stuff */ 
    curl_easy_cleanup(curl);

    return buffer;
}

/* Parse container ID out of JSON response */
static int docker_parse_container_id(char* response, char **name) {
    // Regular expression for parsing "create" call JSON response
    char *plc_docker_containerid_regex =
            "\\{\\s*\"[Ii][Dd]\\s*\"\\:\\s*\"(\\w+)\"\\s*,\\s*\"[Ww]arnings\"\\s*\\:([^\\}]*)\\s*\\}";
    regex_t     preg;
    regmatch_t  pmatch[3];
    int         res = 0;
    int         len = 0;
    int         wmasklen, masklen;
    pg_wchar   *mask;
    int         wdatalen, datalen;
    pg_wchar   *data;

    masklen = strlen(plc_docker_containerid_regex);
    mask = (pg_wchar *) palloc((masklen + 1) * sizeof(pg_wchar));
    wmasklen = pg_mb2wchar_with_len(plc_docker_containerid_regex, mask, masklen);

    res = pg_regcomp(&preg, mask, wmasklen, REG_ADVANCED);
    pfree(mask);
    if (res < 0) {
        elog(ERROR, "Cannot compile Postgres regular expression: '%s'", strerror(errno));
        return -1;
    }

    datalen = strlen(response);
    data = (pg_wchar *) palloc((datalen + 1) * sizeof(pg_wchar));
    wdatalen = pg_mb2wchar_with_len(response, data, datalen);

    res = pg_regexec(&preg,
                     data,
                     wdatalen,
                     0,
                     NULL,
                     3,
                     pmatch,
                     0);
    pfree(data);
    if (res == REG_NOMATCH) {
        elog(ERROR, "Docker API response does not match regular expression: '%s'", response);
        return -1;
    }

    if (pmatch[1].rm_so == -1) {
        elog(ERROR, "Postgres regex failed to extract created container name from Docker API response: '%s'", response);
        return -1;
    }

    len = pmatch[1].rm_eo - pmatch[1].rm_so;
    *name = palloc(len + 1);
    memcpy(*name, response + pmatch[1].rm_so, len);
    (*name)[len] = '\0';

    if (pmatch[2].rm_so != -1 && pmatch[2].rm_eo - pmatch[2].rm_so > 10) {
        char *err = NULL;
        len = pmatch[2].rm_eo - pmatch[2].rm_so;
        err = palloc(len+1);
        memcpy(err, response + pmatch[2].rm_so, len);
        err[len] = '\0';
        elog(WARNING, "Docker API 'create' call returned warning message: '%s'", err);
    }

    pg_regfree(&preg);
    return 0;
}

/* Parse host port out of JSON response */
static int docker_parse_port_mapping(char* response, int *port) {
    // Regular expression for parsing container inspection JSON response
    char *plc_docker_port_regex =
            "\"8080\\/tcp\"\\s*\\:\\s*\\[.*\"HostPort\"\\s*\\:\\s*\"([0-9]*)\".*\\]";
    regex_t     preg;
    regmatch_t  pmatch[2];
    int         res = 0;
    int         len = 0;
    int         wmasklen, masklen;
    pg_wchar   *mask;
    int         wdatalen, datalen;
    pg_wchar   *data;
    char       *portstr;

    masklen = strlen(plc_docker_port_regex);
    mask = (pg_wchar *) palloc((masklen + 1) * sizeof(pg_wchar));
    wmasklen = pg_mb2wchar_with_len(plc_docker_port_regex, mask, masklen);

    res = pg_regcomp(&preg, mask, wmasklen, REG_ADVANCED);
    pfree(mask);
    if (res < 0) {
        elog(ERROR, "Cannot compile Postgres regular expression: '%s'", strerror(errno));
        return -1;
    }

    datalen = strlen(response);
    data = (pg_wchar *) palloc((datalen + 1) * sizeof(pg_wchar));
    wdatalen = pg_mb2wchar_with_len(response, data, datalen);

    res = pg_regexec(&preg,
                     data,
                     wdatalen,
                     0,
                     NULL,
                     2,
                     pmatch,
                     0);
    pfree(data);
    if(res == REG_NOMATCH) {
        return -1;
    }

    if (pmatch[1].rm_so == -1) {
        return -1;
    }

    len = pmatch[1].rm_eo - pmatch[1].rm_so;
    portstr = malloc(len + 1);
    memcpy(portstr, response + pmatch[1].rm_so, len);
    portstr[len] = '\0';

    *port = strtol(portstr, NULL, 10);
    return 0;
}

/* Not used in Curl API */
int plc_docker_connect() {
    return 8080;
}

int plc_docker_create_container(pg_attribute_unused() int sockfd, plcContainerConf *conf, char **name, int container_id) {
    char *createRequest =
            "{\n"
            "    \"AttachStdin\": false,\n"
            "    \"AttachStdout\": false,\n"
            "    \"AttachStderr\": false,\n"
            "    \"Tty\": false,\n"
            "    \"Cmd\": [\"%s\"],\n"
            "    \"Env\": [\"USE_NETWORK=%s\"],\n"
            "    \"NetworkDisabled\": %s,\n"
            "    \"Image\": \"%s\",\n"
            "    \"HostConfig\": {\n"
            "        \"Binds\": [%s],\n"
            "        \"Memory\": %lld,\n"
            "        \"PublishAllPorts\": true\n"
            "    }\n"
            "}\n";
    char *volumeShare = get_sharing_options(conf, container_id);
    char *messageBody = NULL;
    plcCurlBuffer *response = NULL;
    int res = 0;

    /* Get Docket API "create" call JSON message body */
    messageBody = palloc(40 + strlen(createRequest) + strlen(conf->command)
                            + strlen(conf->dockerid) + strlen(volumeShare));
    sprintf(messageBody,
            createRequest,
            conf->command,
            conf->isNetworkConnection ? "true" : "false",
            conf->isNetworkConnection ? "false" : "true",
            conf->dockerid,
            volumeShare,
            ((long long)conf->memoryMb) * 1024 * 1024);

    /* Make a call */
    response = plcCurlRESTAPICall(PLC_CALL_POST, "/containers/create", messageBody, 201, false);
    res = response->status;

    /* Free up intermediate data */
    pfree(messageBody);
    pfree(volumeShare);

    if (res == 0) {
        res = docker_parse_container_id(response->data, name);
        if (res < 0) {
            elog(ERROR, "Error parsing container ID");
        }
    }

    plcCurlBufferFree(response);

    return res;
}

int plc_docker_start_container(pg_attribute_unused() int sockfd, char *name) {
    plcCurlBuffer *response = NULL;
    char *method = "/containers/%s/start";
    char *url = NULL;
    int res = 0;

    url = palloc(strlen(method) + strlen(name) + 2);
    sprintf(url, method, name);

    response = plcCurlRESTAPICall(PLC_CALL_POST, url, NULL, 204, false);
    res = response->status;

    plcCurlBufferFree(response);

    return res;
}

int plc_docker_kill_container(pg_attribute_unused() int sockfd, char *name) {
    plcCurlBuffer *response = NULL;
    char *method = "/containers/%s/kill?signal=KILL";
    char *url = NULL;
    int res = 0;

    url = palloc(strlen(method) + strlen(name) + 2);
    sprintf(url, method, name);

    response = plcCurlRESTAPICall(PLC_CALL_POST, url, NULL, 204, false);
    res = response->status;

    plcCurlBufferFree(response);

    return res;
}

int plc_docker_inspect_container(pg_attribute_unused() int sockfd, char *name, int *port) {
    plcCurlBuffer *response = NULL;
    char *method = "/containers/%s/json";
    char *url = NULL;
    int res = 0;

    url = palloc(strlen(method) + strlen(name) + 2);
    sprintf(url, method, name);

    response = plcCurlRESTAPICall(PLC_CALL_HTTPGET, url, NULL, 200, false);
    res = response->status;

    if (res == 0) {
        res = docker_parse_port_mapping(response->data, port);
        if (res < 0) {
            elog(ERROR, "Error finding container port mapping in response '%s'", response->data);
        }
    }

    plcCurlBufferFree(response);

    return res;
}

int plc_docker_wait_container(pg_attribute_unused() int sockfd, char *name) {
    plcCurlBuffer *response = NULL;
    char *method = "/containers/%s/wait";
    char *url = NULL;
    int res = 0;

    url = palloc(strlen(method) + strlen(name) + 2);
    sprintf(url, method, name);

    response = plcCurlRESTAPICall(PLC_CALL_POST, url, NULL, 200, true);
    res = response->status;

    plcCurlBufferFree(response);

    return res;
}

int plc_docker_delete_container(pg_attribute_unused() int sockfd, char *name) {
    plcCurlBuffer *response = NULL;
    char *method = "/containers/%s?v=1&force=1";
    char *url = NULL;
    int res = 0;

    url = palloc(strlen(method) + strlen(name) + 2);
    sprintf(url, method, name);

    response = plcCurlRESTAPICall(PLC_CALL_DELETE, url, NULL, 204, true);
    res = response->status;

    plcCurlBufferFree(response);

    return res;
}

/* Not used in Curl API */
int plc_docker_disconnect(pg_attribute_unused() int sockfd) {
    return 0;
}

#endif
