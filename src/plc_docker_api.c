/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */


#include "postgres.h"
#include "utils/guc.h"
#include "libpq/libpq.h"
#include "miscadmin.h"
#include "libpq/libpq-be.h"
#include "common/comm_utils.h"
#include "plc_docker_api.h"
#include "plc_backend_api.h"
#ifndef PLC_PG
  #include "cdb/cdbvars.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <json-c/json.h>

// URL prefix specifies Docker API version
static char *plc_docker_version_127 = "http:/v1.27";
// GPU basic support after moby v19.03 (2019-7) API version v1.40
// GPU with out privilege support when using NVIDIA/libnvidia-container v1.10 (2020.5) with API version v1.40
// need to use NVIDIA/libnvidia-container to enable non-privilege container
// NVIDIA/container-config (2019-11~2021-11) or something before libnvidia-container does not support non-privilege
static char *plc_docker_version_140 = "http:/v1.40";

static char *default_log_dirver = "journald";

/* Static functions of the Docker API module */
static plcCurlBuffer *plcCurlBufferInit();

static void plcCurlBufferFree(plcCurlBuffer *buf);

static size_t plcCurlCallback(void *contents, size_t size, size_t nmemb, void *userp);

static plcCurlBuffer *plcCurlRESTAPICall(plcCurlCallType cType, const char *version, char *url, char *body);

static int docker_inspect_string(char *buf, char **element, plcInspectionMode type);

static const inline char* _get_docker_socket_path() {
	// Default location of the Docker API unix socket
	static char *plc_docker_socket = "/var/run/docker.sock";

	static char *path = NULL;

	if (path == NULL) {
		path = getenv("PLC_DOCKER_SOCKET_PATH");
	}

	if (path == NULL) {
		path = plc_docker_socket;
	}

	return path;
}

/* Initialize Curl response receiving buffer */
static plcCurlBuffer *plcCurlBufferInit() {
	plcCurlBuffer *buf = palloc(sizeof(plcCurlBuffer));
	buf->data = palloc0(CURL_BUFFER_SIZE);   /* will be grown as needed by the realloc above */
	buf->bufsize = CURL_BUFFER_SIZE;        /* initial size of the buffer */
	buf->size = 0;              /* amount of data in this buffer */
	buf->status = 0;            /* status of the Curl call */
	return buf;
}

/* Free Curl response receiving buffer */
static void plcCurlBufferFree(plcCurlBuffer *buf) {
	if (buf != NULL) {
		if (buf->data)
			pfree(buf->data);
		pfree(buf);
	}
}

/* Curl callback for receiving a chunk of data into buffer */
static size_t plcCurlCallback(void *contents, size_t size, size_t nmemb, void *userp) {
	size_t realsize = size * nmemb;
	plcCurlBuffer *mem = (plcCurlBuffer *) userp;

	if (mem->size + realsize + 1 > mem->bufsize) {
		/*  repalloc will preserve the content of the memory block,
		 *  up to the lesser of the new and old sizes
		 */
		mem->data = repalloc(mem->data, 3 * (mem->size + realsize + 1) / 2);
		mem->bufsize = 3 * (mem->size + realsize + 1) / 2;
	}

	memcpy(&(mem->data[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->data[mem->size] = 0;

	return realsize;
}

/* Function for calling Docker REST API using Curl */
static plcCurlBuffer *plcCurlRESTAPICall(plcCurlCallType cType,
                                         const char *version_prefix,
                                         char *url,
                                         char *body) {
	CURL *curl;
	CURLcode res;
	plcCurlBuffer *buffer = plcCurlBufferInit();
	char errbuf[CURL_ERROR_SIZE];

	/* Calling the API */
	struct timeval start_time, end_time;
	uint64_t elapsed_us;

	memset(errbuf, 0, CURL_ERROR_SIZE);

	curl = curl_easy_init();

	if (curl) {
		char *fullurl;
		struct curl_slist *headers = NULL;

		curl_easy_reset(curl);
		if (log_min_messages <= DEBUG1)
			curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

		/* Setting Docker API endpoint */
		curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, _get_docker_socket_path());

		/* Setting up request URL */
		if (cType == PLC_HTTP_GET && body != NULL)
		{
			char *param = NULL;
			param = curl_easy_escape(curl, body ,strlen(body));
			fullurl = palloc(strlen(version_prefix) + strlen(url) + strlen(param) + 2);
			sprintf(fullurl, "%s%s%s", plc_docker_version_127, url, param);
			curl_easy_setopt(curl, CURLOPT_URL, fullurl);
			curl_free(param);
		} else {
			fullurl = palloc(strlen(version_prefix) + strlen(url) + 2);
			sprintf(fullurl, "%s%s", version_prefix, url);
			curl_easy_setopt(curl, CURLOPT_URL, fullurl);
		}

		/* Providing a buffer to store errors in */
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

		/* FIXME: Need GUCs for timeout parameter settings? */

		/* Setting timeout for connecting. */
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

		/* Setting timeout for connecting. */
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

		/* Choosing the right request type */
		switch (cType) {
			case PLC_HTTP_GET:
				curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
				break;
			case PLC_HTTP_POST:
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
			case PLC_HTTP_DELETE:
				curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
				break;
			default:
				snprintf(backend_error_message, sizeof(backend_error_message),
				         "Unsupported call type for PL/Container Docker Curl API: %d", cType);
				buffer->status = -1;
				goto cleanup;
		}

		/* Setting up response receive callback */
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, plcCurlCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) buffer);

		gettimeofday(&start_time, NULL);
		res = curl_easy_perform(curl);
		if (res != CURLE_OK) {
			size_t len = strlen(errbuf);

			gettimeofday(&end_time, NULL);
			elapsed_us =
				((uint64) end_time.tv_sec) * 1000000 + end_time.tv_usec -
				((uint64) start_time.tv_sec) * 1000000 - start_time.tv_usec;

			snprintf(backend_error_message, sizeof(backend_error_message),
			         "PL/Container libcurl returns code %d, error '%s'", res,
			         (len > 0) ? errbuf : curl_easy_strerror(res));
			buffer->status = -1;

			backend_log(LOG, "Curl Request with type: %d, url: %s", cType, fullurl);
			backend_log(LOG, "Curl Request with http body: %s\n", body);
			backend_log(LOG, "Curl Request costs "
				UINT64_FORMAT
				"ms", elapsed_us / 1000);

			goto cleanup;
		} else {
			long http_code = 0;

			curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
			buffer->status = (int) http_code;
			backend_log(DEBUG1, "CURL response code is %ld. CURL response message is %s", http_code, buffer->data);
		}

cleanup:
		pfree(fullurl);
		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);
	} else {
		snprintf(backend_error_message, sizeof(backend_error_message),
		         "Failed to start a curl session for unknown reason");
		buffer->status = -1;
	}

	return buffer;
}

#define __JSON_APPEND_STRING_ARRARY(__bf, __n, __array) \
	do { \
		appendStringInfo(__bf, "["); \
		int __it = 0; \
		for (; __it < __n; __it++) { \
			appendStringInfo(__bf, "\"%s\"", __array[__it]); \
			if (__it != __n -1) { \
				appendStringInfo(__bf, ","); \
			} \
		} \
		appendStringInfo(__bf, "]"); \
	} while(0)

#define __JSON_APPEND_OBJECT_ARRAY(__bf, __n, __array, __out_fn) \
	do { \
		appendStringInfo(__bf, "["); \
		int __it = 0; \
		for (; __it < __n; __it++) { \
			__out_fn(__bf, &(__array)[__it]); \
			if (__it != __n -1) { \
				appendStringInfo(__bf, ","); \
			} \
		} \
		appendStringInfo(__bf, "]"); \
	} while(0)

#define __JSON_APPEND_WITH_NAME(__bf, __name, __end, __out_fn) \
	do { \
		appendStringInfo(__bf, "\""__name"\":"); \
		__out_fn; \
		if (!__end) { \
			appendStringInfo(__bf, ","); \
		} \
	} while (0)

static void _req_serialize_devicerequest(StringInfo b, const plcDeviceRequest *req) {
	appendStringInfo(b, "{");

	__JSON_APPEND_WITH_NAME(b, "Count", false, appendStringInfo(b, "%d", req->_count));

	if (req->driver != NULL) {
		__JSON_APPEND_WITH_NAME(b, "Driver", false, appendStringInfo(b, "%s", req->driver));
	}

	if (req->ndeviceid > 0) {
		__JSON_APPEND_WITH_NAME(b, "DeviceIDs", false, __JSON_APPEND_STRING_ARRARY(b, req->ndeviceid, req->deviceid));
	}

	if (req->ncapabilities > 0) {
		__JSON_APPEND_WITH_NAME(b, "Capabilities", true,
			{
				appendStringInfo(b, "[");
				__JSON_APPEND_STRING_ARRARY(b, req->ncapabilities, req->capabilities);
				appendStringInfo(b, "]");
			}
		);
	}

	appendStringInfo(b, "}");
}

int plc_docker_create_container(runtimeConfEntry *conf, char **name, int container_id, char **uds_dir) {
	char *createRequest =
		"{\n"
			"    \"AttachStdin\": false,\n"
			"    \"AttachStdout\": %s,\n"
			"    \"AttachStderr\": %s,\n"
			"    \"Tty\": false,\n"
			"    \"Cmd\": [\"%s\"],\n"
			"    \"Env\": [\"EXECUTOR_UID=%d\",\n"
			"              \"EXECUTOR_GID=%d\",\n"
			"              \"DB_USER_NAME=%s\",\n"
			"              \"DB_NAME=%s\",\n"
			"              \"DB_QE_PID=%d\",\n"
			"              \"USE_CONTAINER_NETWORK=%s\"],\n"
			"    \"NetworkDisabled\": %s,\n"
			"    \"Image\": \"%s\",\n"
			"    \"HostConfig\": {\n"
			"        \"Binds\": [%s],\n"
			"        \"CgroupParent\": \"%s\",\n"
			"        \"Memory\": %lld,\n"
			"        \"CpuShares\": %lld,\n"
			"        \"PublishAllPorts\": true,"
			"        %s\n"
			"        \"LogConfig\":{\"Type\": \"%s\"}\n"
			"    },\n"
			"    \"Labels\": {\n"
			"        \"owner\": \"%s\",\n"
			"        \"dbid\": \"%d\",\n"
			"        \"databaseid\": \"%d\",\n"
			"        \"segindex\": \"%d\"\n"
			"    }\n"
			"}\n";
	bool has_error;
	char *volumeShare = get_sharing_options(conf, container_id, &has_error, uds_dir);

	char *messageBody = NULL;
	plcCurlBuffer *response = NULL;
	int res = 0;
	int createStringSize = 0;

	const char *username;
	const char *dbname;
	char cgroupParent[RES_GROUP_PATH_MAX_LENGTH] = "";

	int16 dbid = 0;
	int16 database_id = 0, segindex = 0;
#ifndef PLC_PG
	dbid = GpIdentity.segindex;
	database_id = MyDatabaseId;
	segindex = GpIdentity.segindex;
#endif

	StringInfoData requestBuffer; // TODO refactor the json serialize with this requestBuffer
	initStringInfo(&requestBuffer);

	/*
	 *  no shared volumes should not be treated as an error, so we use has_error to
	 *  identifier whether there is an error when parse sharing options.
	 */
	if (has_error == true) {
		return -1;
	}

	username = GetUserNameFromId(GetUserId());
	dbname = MyProcPort->database_name;


	/*
	 * We run container processes with the uid/gid of user "nobody" on host.
	 * We might want to allow to use uid/gid set in runtimeConfEntry but since
	 * this is important (security concern) we simply use "nobody" by now.
	 * Note this is used for IPC only at this momement.
	 * Note: We stop to use user "nobody" currently due to different os has
	 * different user id for "nobody". Hence simpliy pass the host OS "nobody"
	 * user id will have some risks.
	 */

	/*
	 * generate cgroup parent path when using resource group feature
	 * set cgroupParent to empty string when resgroupOid is not valid
	 * and cgroup parent of containers will be 'docker' as default.
	 * Note that this feature is only for gpdb with resource group enable.
	 */

	if (conf->resgroupOid != InvalidOid) {
		snprintf(cgroupParent, RES_GROUP_PATH_MAX_LENGTH, "/gpdb/%d",conf->resgroupOid);
	}

	if (conf->ndevicerequests > 0) {
		StringInfo b = &requestBuffer;

		int n = conf->ndevicerequests;
		plcDeviceRequest *req = conf->devicerequests;

		req->_count = req->ndeviceid;
		if (req->ndeviceid != -1)
			req->_count = 0; // docker: cannot set both Count and DeviceIDs on device request

		appendStringInfo(b, "\n");

		__JSON_APPEND_WITH_NAME(
			b, "DeviceRequests", false,
			__JSON_APPEND_OBJECT_ARRAY(b, n, req, _req_serialize_devicerequest)
		);
	}

	/* Get Docket API "create" call JSON message body */
	createStringSize = 100 + strlen(createRequest) + strlen(conf->command)
	                   + strlen(conf->image) + strlen(volumeShare) + strlen(username) * 2
	                   + strlen(dbname) + requestBuffer.len;
	messageBody = (char *) palloc(createStringSize * sizeof(char));
	snprintf(messageBody,
	         createStringSize,
	         createRequest,
	         conf->useContainerLogging ? "true" : "false",
	         conf->useContainerLogging ? "true" : "false",
	         conf->command,
	         getuid(),
	         getgid(),
	         username,
	         dbname,
	         MyProcPid,
	         conf->useContainerNetwork ? "true" : "false", // .Env.useContainerNetwork
	         conf->enableNetwork ? "false" : "true", // .NetworkDisabled
	         conf->image,
	         volumeShare,
	         cgroupParent,
	         ((long long) conf->memoryMb) * 1024 * 1024,
	         ((long long) conf->cpuShare),
	         requestBuffer.data, // .DeviceRequests
	         conf->useContainerLogging ? default_log_dirver : "none",
	         username,
	         dbid,
	         database_id,
	         segindex);

	// to use devicerequests, need docker api version >= 1.40.
	// resolve version dynamically to compatible with old docker install
	const char* version_prefix = conf->devicerequests == NULL ? plc_docker_version_127 : plc_docker_version_140;

	response = plcCurlRESTAPICall(PLC_HTTP_POST, version_prefix, "/containers/create", messageBody);

	res = response->status;

	if (res == 201) {
		res = 0;
	} else if (res >= 0) {
		backend_log(LOG, "Docker fails to create container, response: %s", response->data);
		snprintf(backend_error_message, sizeof(backend_error_message),
		         "Failed to create container, return code: %d, detail: %s", res, response->data);
		res = -1;
	}

	/* Free up intermediate data */
	pfree(volumeShare);
	pfree(messageBody);

	if (res < 0) {
		goto cleanup;
	}
	res = docker_inspect_string(response->data, name, PLC_INSPECT_NAME);

	if (res < 0) {
		backend_log(DEBUG1, "Error parsing container ID during creating container with errno %d.", res);
		snprintf(backend_error_message, sizeof(backend_error_message),
		         "Error parsing container ID during creating container");
		goto cleanup;
	}

cleanup:
	pfree(requestBuffer.data);
	plcCurlBufferFree(response);

	return res;
}

int plc_docker_start_container(const char *name) {
	plcCurlBuffer *response = NULL;
	char *method = "/containers/%s/start";
	char *url = NULL;
	int res = 0;

	url = palloc(strlen(method) + strlen(name) + 2);
	sprintf(url, method, name);

	response = plcCurlRESTAPICall(PLC_HTTP_POST, plc_docker_version_127, url, NULL);
	res = response->status;

	if (res == 204 || res == 304) {
		res = 0;
	} else if (res >= 0) {
		backend_log(DEBUG1, "start docker container %s failed with errno %d.", name, res);
		snprintf(backend_error_message, sizeof(backend_error_message),
		         "Failed to start container %s, return code: %d, detail: %s", name, res, response->data);
		res = -1;
	}

	plcCurlBufferFree(response);
	pfree(url);

	return res;
}

int plc_docker_kill_container(const char *name) {
	plcCurlBuffer *response = NULL;
	char *method = "/containers/%s/kill?signal=KILL";
	char *url = NULL;
	int res = 0;

	backend_log(FATAL, "Not implemented yet. Do not call it.");

	url = palloc(strlen(method) + strlen(name) + 2);
	sprintf(url, method, name);

	response = plcCurlRESTAPICall(PLC_HTTP_POST, plc_docker_version_127, url, NULL);
	res = response->status;

	plcCurlBufferFree(response);

	pfree(url);

	return res;
}

int plc_docker_inspect_container(const char *name, char **element, plcInspectionMode type) {
	plcCurlBuffer *response = NULL;
	char *method = "/containers/%s/json";
	char *url = NULL;
	int res = 0;

	url = palloc(strlen(method) + strlen(name) + 2);
	sprintf(url, method, name);

	response = plcCurlRESTAPICall(PLC_HTTP_GET, plc_docker_version_127, url, NULL);
	res = response->status;

	/* We will need to handle the "no such container" case specially. */
	if (res == 404 && type == PLC_INSPECT_STATUS) {
		*element = pstrdup("unexist");
		res = 0;
		goto cleanup;
	}

	if (res != 200) {
		backend_log(LOG, "Docker cannot inspect container, response: %s", response->data);
		snprintf(backend_error_message, sizeof(backend_error_message),
		         "Docker inspect api returns http code %d on container %s, detail: %s", res, name, response->data);
		res = -1;
		goto cleanup;
	}

	res = docker_inspect_string(response->data, element, type);
	if (res < 0) {
		snprintf(backend_error_message, sizeof(backend_error_message),
		         "Failed to inspect the container.");
		goto cleanup;
	}

cleanup:
	plcCurlBufferFree(response);
	pfree(url);

	return res;
}

int plc_docker_wait_container(const char *name) {
	plcCurlBuffer *response = NULL;
	char *method = "/containers/%s/wait";
	char *url = NULL;
	int res = 0;

	backend_log(FATAL, "Not implemented yet. Do not call it.");

	url = palloc(strlen(method) + strlen(name) + 2);
	sprintf(url, method, name);

	response = plcCurlRESTAPICall(PLC_HTTP_POST, plc_docker_version_127, url, NULL);
	res = response->status;

	plcCurlBufferFree(response);

	pfree(url);

	return res;
}

int plc_docker_delete_container(const char *name) {
	plcCurlBuffer *response = NULL;
	char *method = "/containers/%s?v=1&force=1";
	char *url = NULL;
	int res = 0;

	url = palloc(strlen(method) + strlen(name) + 2);
	sprintf(url, method, name);

	response = plcCurlRESTAPICall(PLC_HTTP_DELETE, plc_docker_version_127, url, NULL);
	res = response->status;

	/* 204 = deleted success, 404 = container not found, both are OK for delete */
	if (res == 204 || res == 404) {
		res = 0;
	} else if (res >= 0) {
		snprintf(backend_error_message, sizeof(backend_error_message),
		         "Failed to delete container %s, return code: %d, detail: %s", name, res, response->data);
		res = -1;
	}

	plcCurlBufferFree(response);
	pfree(url);

	return res;
}

int plc_docker_list_container(char **result, int dbid) {
	plcCurlBuffer *response = NULL;
	char *url = "/containers/json?all=1&filters=";
	char *param = "{\"label\":[\"dbid=%d\"]}";
	char *body = NULL;
	int res = 0;

	body = (char *) palloc((strlen(param) + 12) * sizeof(char));
	sprintf(body, param, dbid);
	response = plcCurlRESTAPICall(PLC_HTTP_GET, plc_docker_version_127, url, body);
	res = response->status;

	if (res == 200) {
		res = 0;
	} else if (res >= 0) {
		snprintf(backend_error_message, sizeof(backend_error_message),
		         "Failed to list containers, return code: %d, detail: %s, dbid is %d", res, response->data, dbid);
		res = -1;
	}
	*result = pstrdup(response->data);

	pfree(body);

	return res;
}

int plc_docker_get_container_state(const char *name, char **result) {
	plcCurlBuffer *response = NULL;
	char *method = "/containers/%s/stats?stream=false";
	char *url = NULL;
	int res = 0;

	url = palloc(strlen(method) + strlen(name) + 2);
	sprintf(url, method, name);
	response = plcCurlRESTAPICall(PLC_HTTP_GET, plc_docker_version_127, url, NULL);
	res = response->status;

	if (res == 200) {
		res = 0;
	} else if (res >= 0) {
		snprintf(backend_error_message, sizeof(backend_error_message),
		         "Failed to get container %s state, return code: %d, detail: %s", name, res, response->data);
		res = -1;
	}

	*result = pstrdup(response->data);

	pfree(url);

	return res;
}

static int docker_inspect_string(char *buf, char **element, plcInspectionMode type) {
	int i;
	struct json_object *response = NULL;

	backend_log(DEBUG1, "plcontainer: docker_inspect_string:%s", buf);
	response = json_tokener_parse(buf);
	if (response == NULL)
		return -1;
	if (type == PLC_INSPECT_NAME) {
		struct json_object *nameidObj = NULL;
		const char *namestr;

		if (!json_object_object_get_ex(response, "Id", &nameidObj)) {
			backend_log(WARNING, "failed to get json \"Id\" field.");
			return -1;
		}
		namestr = json_object_get_string(nameidObj);
		*element = pstrdup(namestr);
		return 0;
	} else if (type == PLC_INSPECT_PORT) {
		struct json_object *NetworkSettingsObj = NULL;
		struct json_object *PortsObj = NULL;
		struct json_object *HostPortArray = NULL;
		int arraylen;

		if (!json_object_object_get_ex(response, "NetworkSettings", &NetworkSettingsObj)) {
			backend_log(WARNING, "failed to get json \"NetworkSettings\" field.");
			return -1;
		}
		if (!json_object_object_get_ex(NetworkSettingsObj, "Ports", &PortsObj)) {
			backend_log(WARNING, "failed to get json \"Ports\" field.");
			return -1;
		}
		if (!json_object_object_get_ex(PortsObj, "8080/tcp", &HostPortArray)) {
			backend_log(WARNING, "failed to get json \"HostPortArray\" field.");
			return -1;
		}
		if (json_object_get_type(HostPortArray) != json_type_array) {
			backend_log(WARNING, "no element found in json \"HostPortArray\" field.");
			return -1;
		}
		arraylen = json_object_array_length(HostPortArray);
		for (i = 0; i < arraylen; i++) {
			struct json_object *PortBindingObj = NULL;
			struct json_object *HostPortObj = NULL;
			const char *HostPortStr;

			PortBindingObj = json_object_array_get_idx(HostPortArray, i);
			if (PortBindingObj == NULL) {
				backend_log(WARNING, "failed to get json \"PortBinding\" field.");
				return -1;
			}
			if (!json_object_object_get_ex(PortBindingObj, "HostPort", &HostPortObj)) {
				backend_log(WARNING, "failed to get json \"HostPort\" field.");
				return -1;
			}
			HostPortStr = json_object_get_string(HostPortObj);
			*element = pstrdup(HostPortStr);
			return 0;
		}
	} else if (type == PLC_INSPECT_STATUS) {
		struct json_object *StateObj = NULL;

		if (!json_object_object_get_ex(response, "State", &StateObj)) {
			backend_log(WARNING, "failed to get json \"State\" field.");
			return -1;
		}
		struct json_object *StatusObj = NULL;
		if (!json_object_object_get_ex(StateObj, "Status", &StatusObj)) {
			backend_log(WARNING, "failed to get json \"Status\" field.");
			return -1;
		}
		const char *StatusStr = json_object_get_string(StatusObj);
		*element = pstrdup(StatusStr);
		return 0;
	} else if (type == PLC_INSPECT_OOM) {
		struct json_object *StateObj = NULL;
		struct json_object *OOMKillObj = NULL;
		const char *OOMKillStr;
		if (!json_object_object_get_ex(response, "State", &StateObj)) {
			backend_log(WARNING, "failed to get json \"State\" field.");
			return -1;
		}

		if (!json_object_object_get_ex(StateObj, "OOMKilled", &OOMKillObj)) {
			backend_log(WARNING, "failed to get json \"OOMKilled\" field.");
			return -1;
		}
		OOMKillStr = json_object_get_string(OOMKillObj);
		*element = pstrdup(OOMKillStr);
		return 0;
	} else {
		backend_log(LOG, "Error PLC inspection mode, unacceptable inpsection type %d", type);
		return -1;
	}

	return -1;
}
