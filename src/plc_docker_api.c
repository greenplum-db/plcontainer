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
#include <curl/curl.h>
#include <json-c/json.h>

// Default location of the Docker API unix socket
static char *plc_docker_socket = "/var/run/docker.sock";

// URL prefix specifies Docker API version
#ifdef DOCKER_API_LOW
static char *plc_docker_url_prefix = "http:/v1.19";
static char *default_log_dirver = "syslog";
#else
static char *plc_docker_url_prefix = "http:/v1.27";
static char *default_log_dirver = "journald";
#endif

/* Static functions of the Docker API module */
static plcCurlBuffer *plcCurlBufferInit();

static void plcCurlBufferFree(plcCurlBuffer *buf);

static size_t plcCurlCallback(void *contents, size_t size, size_t nmemb, void *userp);

static plcCurlBuffer *plcCurlRESTAPICall(plcCurlCallType cType, char *url, char *body);

static int docker_inspect_string(char *buf, char **element, plcInspectionMode type);

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
                                         char *url,
                                         char *body) {
	CURL *curl;
	CURLcode res;
	plcCurlBuffer *buffer = plcCurlBufferInit();
	char errbuf[CURL_ERROR_SIZE];

	memset(errbuf, 0, CURL_ERROR_SIZE);

	curl = curl_easy_init();

	if (curl) {
		char *fullurl;
		struct curl_slist *headers = NULL;

		curl_easy_reset(curl);
		if (log_min_messages <= DEBUG1)
			curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

		/* Setting Docker API endpoint */
		curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, plc_docker_socket);

		/* Setting up request URL */
		fullurl = palloc(strlen(plc_docker_url_prefix) + strlen(url) + 2);
		sprintf(fullurl, "%s%s", plc_docker_url_prefix, url);
		curl_easy_setopt(curl, CURLOPT_URL, fullurl);

		/* Providing a buffer to store errors in */
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

		/* FIXME: Need GUCs for timeout parameter settings? */

		/* Setting timeout for connecting. */
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

		/* Setting timeout for connecting. */
#ifdef DOCKER_API_LOW
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 180L);
#else
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
#endif

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

		/* Calling the API */
		struct timeval start_time, end_time;
		uint64_t elapsed_us;

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
			buffer->status = -2;

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
			"              \"CLIENT_UID=%d\",\n"
			"              \"CLIENT_GID=%d\",\n"
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
			"        \"CpuShares\": %lld, \n"
			"        \"PublishAllPorts\": true,\n"
			"        \"LogConfig\":{\"Type\": \"%s\"}\n"
			"    },\n"
			"    \"Labels\": {\n"
			"        \"owner\": \"%s\",\n"
			"        \"dbid\": \"%d\"\n"
			"    }\n"
			"}\n";
	bool has_error;
	char *volumeShare = get_sharing_options(conf, container_id, &has_error, uds_dir);

	int16 dbid = 0;
#ifndef PLC_PG
	dbid = GpIdentity.dbid;
#endif

	/*
	 *  no shared volumes should not be treated as an error, so we use has_error to
	 *  identifier whether there is an error when parse sharing options.
	 */
	if (has_error == true) {
		return -1;
	}
	char *messageBody = NULL;
	plcCurlBuffer *response = NULL;
	int res = 0;
	int createStringSize = 0;
#ifdef PLC_PG
	const char *username = GetUserNameFromId(GetUserId(),false);
#else	
	const char *username = GetUserNameFromId(GetUserId());
#endif	
	const char *dbname = MyProcPort->database_name;
	struct passwd *pwd;


	/*
	 * We run container processes with the uid/gid of user "nobody" on host.
	 * We might want to allow to use uid/gid set in runtimeConfEntry but since
	 * this is important (security concern) we simply use "nobody" by now.
	 * Note this is used for IPC only at this momement.
	 */
	errno = 0;
	pwd = getpwnam("nobody");
	if (pwd == NULL) {
		snprintf(backend_error_message, sizeof(backend_error_message),
		         "Failed to get passwd info for user 'nobody': %d", errno);
		return -1;
	}

	/*
	 * generate cgroup parent path when using resource group feature
	 * set cgroupParent to empty string when resgroupOid is not valid
	 * and cgroup parent of containers will be 'docker' as default.
	 * Note that this feature is only for gpdb with resource group enable.
	 */
	char cgroupParent[RES_GROUP_PATH_MAX_LENGTH] = "";
	if (conf->resgroupOid != InvalidOid) {
		snprintf(cgroupParent,RES_GROUP_PATH_MAX_LENGTH,"/gpdb/%d",conf->resgroupOid);
	}
	/* Get Docket API "create" call JSON message body */
	createStringSize = 100 + strlen(createRequest) + strlen(conf->command)
	                   + strlen(conf->image) + strlen(volumeShare) + strlen(username) * 2
	                   + strlen(dbname);
	messageBody = (char *) palloc(createStringSize * sizeof(char));
	snprintf(messageBody,
	         createStringSize,
	         createRequest,
	         conf->useContainerLogging ? "true" : "false",
	         conf->useContainerLogging ? "true" : "false",
	         conf->command,
	         getuid(),
	         getgid(),
	         pwd->pw_uid,
	         pwd->pw_gid,
	         username,
	         dbname,
	         MyProcPid,
	         conf->useContainerNetwork ? "true" : "false",
	         conf->useContainerNetwork ? "false" : "true",
	         conf->image,
	         volumeShare,
	       	 cgroupParent,
	         ((long long) conf->memoryMb) * 1024 * 1024,
			 ((long long) conf->cpuShare),
	         conf->useContainerLogging ? default_log_dirver : "none",
	         username,
	         dbid);

	/* Make a call */
	response = plcCurlRESTAPICall(PLC_HTTP_POST, "/containers/create", messageBody);
	/* Free up intermediate data */
	pfree(messageBody);
	pfree(volumeShare);
	res = response->status;

	if (res == 201) {
		res = 0;
	} else if (res >= 0) {
		backend_log(LOG, "Docker fails to create container, response: %s", response->data);
		snprintf(backend_error_message, sizeof(backend_error_message),
		         "Failed to create container (return code: %d).", res);
		res = -1;
	} else {
		snprintf(backend_error_message, sizeof(backend_error_message),
		         "Failed to create container (return code: %d).", res);
	}

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

	response = plcCurlRESTAPICall(PLC_HTTP_POST, url, NULL);
	res = response->status;
	plcCurlBufferFree(response);

	if (res == 204 || res == 304) {
		res = 0;
	} else if (res >= 0) {
		backend_log(DEBUG1, "start docker container %s failed with errno %d.", name, res);
		snprintf(backend_error_message, sizeof(backend_error_message),
		         "Failed to start container %s (return code: %d)", name, res);
		res = -1;
	} else {
		snprintf(backend_error_message, sizeof(backend_error_message),
		         "Failed to start container %s (return code: %d)", name, res);
	}

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

	response = plcCurlRESTAPICall(PLC_HTTP_POST, url, NULL);
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

	response = plcCurlRESTAPICall(PLC_HTTP_GET, url, NULL);
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
		         "Docker inspect api returns http code %d on container %s", res, name);
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

	response = plcCurlRESTAPICall(PLC_HTTP_POST, url, NULL);
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

	response = plcCurlRESTAPICall(PLC_HTTP_DELETE, url, NULL);
	res = response->status;
	plcCurlBufferFree(response);

	/* 204 = deleted success, 404 = container not found, both are OK for delete */
	if (res == 204 || res == 404) {
		res = 0;
	} else if (res >= 0) {
		snprintf(backend_error_message, sizeof(backend_error_message),
		         "Failed to delete container %s (return code: %d)", name, res);
		res = -1;
	} else {
		snprintf(backend_error_message, sizeof(backend_error_message),
		         "Failed to delete container %s (return code: %d)", name, res);
	}

	pfree(url);

	return res;
}

int plc_docker_list_container(char **result) {
	plcCurlBuffer *response = NULL;
	char *method = "/containers/json?all=1&label=\"dbid=%d\"";
	char *url = NULL;
	int res = 0;
	url = (char *) palloc((strlen(method) + 12) * sizeof(char));
	int16 dbid = 0;
#ifndef PLC_PG
	dbid = GpIdentity.dbid;
#endif			 

	sprintf(url, method, dbid); 
	response = plcCurlRESTAPICall(PLC_HTTP_GET, url, NULL);
	res = response->status;

	if (res == 200) {
		res = 0;
	} else if (res >= 0) {
		snprintf(backend_error_message, sizeof(backend_error_message),
		         "Failed to list containers (return code: %d), dbid is %d", res, dbid);
		res = -1;
	} else {
		snprintf(backend_error_message, sizeof(backend_error_message),
		         "Failed to list containers (return code: %d), dbid is %d", res, dbid);
	}
	*result = pstrdup(response->data);

	pfree(url);

	return res;
}

int plc_docker_get_container_state(const char *name, char **result) {
	plcCurlBuffer *response = NULL;
	char *method = "/containers/%s/stats?stream=false";
	char *url = NULL;
	int res = 0;

	url = palloc(strlen(method) + strlen(name) + 2);
	sprintf(url, method, name);
	response = plcCurlRESTAPICall(PLC_HTTP_GET, url, NULL);
	res = response->status;

	if (res == 200) {
		res = 0;
	} else if (res >= 0) {
		snprintf(backend_error_message, sizeof(backend_error_message),
		         "Failed to get container %s state (return code: %d)", name, res);
		res = -1;
	} else {
		snprintf(backend_error_message, sizeof(backend_error_message),
		         "Failed to get container %s state (return code: %d)", name, res);
	}

	*result = pstrdup(response->data);

	pfree(url);

	return res;
}

static int docker_inspect_string(char *buf, char **element, plcInspectionMode type) {
	int i;
	backend_log(DEBUG1, "plcontainer: docker_inspect_string:%s", buf);
	struct json_object *response = json_tokener_parse(buf);
	if (response == NULL)
		return -1;
	if (type == PLC_INSPECT_NAME) {
		struct json_object *nameidObj = NULL;
		if (!json_object_object_get_ex(response, "Id", &nameidObj)) {
			backend_log(WARNING, "failed to get json \"Id\" field.");
			return -1;
		}
		const char *namestr = json_object_get_string(nameidObj);
		*element = pstrdup(namestr);
		return 0;
	} else if (type == PLC_INSPECT_PORT) {
		struct json_object *NetworkSettingsObj = NULL;
		if (!json_object_object_get_ex(response, "NetworkSettings", &NetworkSettingsObj)) {
			backend_log(WARNING, "failed to get json \"NetworkSettings\" field.");
			return -1;
		}
		struct json_object *PortsObj = NULL;
		if (!json_object_object_get_ex(NetworkSettingsObj, "Ports", &PortsObj)) {
			backend_log(WARNING, "failed to get json \"Ports\" field.");
			return -1;
		}
		struct json_object *HostPortArray = NULL;
		if (!json_object_object_get_ex(PortsObj, "8080/tcp", &HostPortArray)) {
			backend_log(WARNING, "failed to get json \"HostPortArray\" field.");
			return -1;
		}
		if (json_object_get_type(HostPortArray) != json_type_array) {
			backend_log(WARNING, "no element found in json \"HostPortArray\" field.");
			return -1;
		}
		int arraylen = json_object_array_length(HostPortArray);
		for (i = 0; i < arraylen; i++) {
			struct json_object *PortBindingObj = NULL;
			PortBindingObj = json_object_array_get_idx(HostPortArray, i);
			if (PortBindingObj == NULL) {
				backend_log(WARNING, "failed to get json \"PortBinding\" field.");
				return -1;
			}
			struct json_object *HostPortObj = NULL;
			if (!json_object_object_get_ex(PortBindingObj, "HostPort", &HostPortObj)) {
				backend_log(WARNING, "failed to get json \"HostPort\" field.");
				return -1;
			}
			const char *HostPortStr = json_object_get_string(HostPortObj);
			*element = pstrdup(HostPortStr);
			return 0;
		}
	} else if (type == PLC_INSPECT_STATUS) {
		struct json_object *StateObj = NULL;
		if (!json_object_object_get_ex(response, "State", &StateObj)) {
			backend_log(WARNING, "failed to get json \"State\" field.");
			return -1;
		}
#ifdef DOCKER_API_LOW
		struct json_object *RunningObj = NULL;
		if (!json_object_object_get_ex(StateObj, "Running", &RunningObj)) {
			backend_log(WARNING, "failed to get json \"Running\" field.");
			return -1;
		}
		const char *RunningStr = json_object_get_string(RunningObj);
		*element = pstrdup(RunningStr);
		return 0;
#else
		struct json_object *StatusObj = NULL;
		if (!json_object_object_get_ex(StateObj, "Status", &StatusObj)) {
			backend_log(WARNING, "failed to get json \"Status\" field.");
			return -1;
		}
		const char *StatusStr = json_object_get_string(StatusObj);
		*element = pstrdup(StatusStr);
		return 0;
#endif
	} else if (type == PLC_INSPECT_OOM) {
		struct json_object *StateObj = NULL;
		if (!json_object_object_get_ex(response, "State", &StateObj)) {
			backend_log(WARNING, "failed to get json \"State\" field.");
			return -1;
		}
		struct json_object *OOMKillObj = NULL;
		if (!json_object_object_get_ex(StateObj, "OOMKilled", &OOMKillObj)) {
			backend_log(WARNING, "failed to get json \"OOMKilled\" field.");
			return -1;
		}
		const char *OOMKillStr = json_object_get_string(OOMKillObj);
		*element = pstrdup(OOMKillStr);
		return 0;
	} else {
		backend_log(LOG, "Error PLC inspection mode, unacceptable inpsection type %d", type);
		return -1;
	}

	return -1;
}
