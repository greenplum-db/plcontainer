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
#include "plc_docker_api_common.h"
#include "cdb/cdbvars.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>

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

static plcCurlBuffer *plcCurlRESTAPICall(plcCurlCallType cType,
                                         char *url,
                                         char *body);

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
				snprintf(api_error_message, sizeof(api_error_message),
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

			snprintf(api_error_message, sizeof(api_error_message),
			         "PL/Container libcurl returns code %d, error '%s'", res,
			         (len > 0) ? errbuf : curl_easy_strerror(res));
			buffer->status = -2;

			elog(LOG, "Curl Request with type: %d, url: %s", cType, fullurl);
			elog(LOG, "Curl Request with http body: %s\n", body);
			elog(LOG, "Curl Request costs "
				UINT64_FORMAT
				"ms", elapsed_us / 1000);

			goto cleanup;
		} else {
			long http_code = 0;

			curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
			buffer->status = (int) http_code;
			elog(DEBUG1, "CURL response code is %ld. CURL response message is %s", http_code, buffer->data);
		}

		cleanup:
		pfree(fullurl);
		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);
	} else {
		snprintf(api_error_message, sizeof(api_error_message),
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
			"              \"USE_NETWORK=%s\"],\n"
			"    \"NetworkDisabled\": %s,\n"
			"    \"Image\": \"%s\",\n"
			"    \"HostConfig\": {\n"
			"        \"Binds\": [%s],\n"
			"        \"Memory\": %lld,\n"
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
	char *messageBody = NULL;
	plcCurlBuffer *response = NULL;
	int res = 0;
	int createStringSize = 0;
	const char *username = GetUserNameFromId(GetUserId());
	const char *dbname = MyProcPort->database_name;

	if (has_error == true) {
		return -1;
	}

	/* Get Docket API "create" call JSON message body */
	createStringSize = 100 + strlen(createRequest) + strlen(conf->command)
	                   + strlen(conf->image) + strlen(volumeShare) + strlen(username) * 2
	                   + strlen(dbname);
	messageBody = (char *) palloc(createStringSize * sizeof(char));
	snprintf(messageBody,
	         createStringSize,
	         createRequest,
	         conf->enable_log ? "true" : "false",
	         conf->enable_log ? "true" : "false",
	         conf->command,
	         getuid(),
	         getgid(),
	         getuid() + 1,
	         getgid() + 1,
	         username,
	         dbname,
	         MyProcPid,
	         conf->isNetworkConnection ? "true" : "false",
	         conf->isNetworkConnection ? "false" : "true",
	         conf->image,
	         volumeShare,
	         ((long long) conf->memoryMb) * 1024 * 1024,
	         conf->enable_log ? default_log_dirver : "none",
	         username,
	         GpIdentity.dbid);

	/* Make a call */
	response = plcCurlRESTAPICall(PLC_HTTP_POST, "/containers/create", messageBody);
	/* Free up intermediate data */
	pfree(messageBody);
	pfree(volumeShare);
	res = response->status;

	if (res == 201) {
		res = 0;
	} else if (res >= 0) {
		elog(LOG, "Docker fails to create container, response: %s", response->data);
		snprintf(api_error_message, sizeof(api_error_message),
		         "Failed to create container (return code: %d).", res);
		res = -1;
	}

	if (res < 0) {
		goto cleanup;
	}
	res = docker_inspect_string(response->data, name, PLC_INSPECT_NAME);

	if (res < 0) {
		elog(DEBUG1, "Error parsing container ID during creating container with errno %d.", res);
		snprintf(api_error_message, sizeof(api_error_message),
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
		elog(DEBUG1, "start docker container %s failed with errno %d.", name, res);
		snprintf(api_error_message, sizeof(api_error_message),
		         "Failed to start container %s (return code: %d)", name, res);
		res = -1;
	}

	return res;
}

int plc_docker_kill_container(const char *name) {
	plcCurlBuffer *response = NULL;
	char *method = "/containers/%s/kill?signal=KILL";
	char *url = NULL;
	int res = 0;

	elog(FATAL, "Not finished yet. Do not call it.");

	url = palloc(strlen(method) + strlen(name) + 2);
	sprintf(url, method, name);

	response = plcCurlRESTAPICall(PLC_HTTP_POST, url, NULL);
	res = response->status;

	plcCurlBufferFree(response);

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
		elog(LOG, "Docker cannot inspect container, response: %s", response->data);
		snprintf(api_error_message, sizeof(api_error_message),
		         "Docker inspect api returns http code %d on container %s", res, name);
		res = -1;
		goto cleanup;
	}

	res = docker_inspect_string(response->data, element, type);
	if (res < 0) {
		snprintf(api_error_message, sizeof(api_error_message),
		         "Failed to inspect the container.");
		goto cleanup;
	}

	cleanup:
	plcCurlBufferFree(response);

	return res;
}

int plc_docker_wait_container(const char *name) {
	plcCurlBuffer *response = NULL;
	char *method = "/containers/%s/wait";
	char *url = NULL;
	int res = 0;

	elog(FATAL, "Not finished yet. Do not call it.");

	url = palloc(strlen(method) + strlen(name) + 2);
	sprintf(url, method, name);

	response = plcCurlRESTAPICall(PLC_HTTP_POST, url, NULL);
	res = response->status;

	plcCurlBufferFree(response);

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

	/* 200 = deleted success, 404 = container not found, both are OK for delete */
	if (res == 204 || res == 404) {
		res = 0;
	} else if (res >= 0) {
		snprintf(api_error_message, sizeof(api_error_message),
		         "Failed to delete container %s (return code: %d)", name, res);
		res = -1;
	}

	return res;
}

int plc_docker_list_container(char **result) {
	plcCurlBuffer *response = NULL;
	char *method = "/containers/json?all=1&label=\"dbid=%d\"";
	char *url = NULL;
	int res = 0;

	url = (char *) palloc((strlen(method) + 12) * sizeof(char));
	sprintf(url, method, GpIdentity.dbid);

	response = plcCurlRESTAPICall(PLC_HTTP_GET, url, NULL);
	res = response->status;

	if (res == 200) {
		res = 0;
	} else if (res >= 0) {
		snprintf(api_error_message, sizeof(api_error_message),
		         "Failed to list containers (return code: %d), dbid is %d", res, GpIdentity.dbid);
		res = -1;
	}
	*result = pstrdup(response->data);
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
		snprintf(api_error_message, sizeof(api_error_message),
		         "Failed to get container %s state (return code: %d)", name, res);
		res = -1;
	}

	*result = pstrdup(response->data);
	return res;
}
