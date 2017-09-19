/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */


#ifndef CURL_DOCKER_API

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "postgres.h"

#include "plc_docker_api_common.h"

/* Templates for Docker API communication */

// Docker API version used in all the API calls
// v1.21 is available in Docker v1.9.x+
// URL prefix specifies Docker API version
#ifdef DOCKER_API_LOW
    static char *plc_docker_api_version = "v1.19";
#else
    static char *plc_docker_api_version = "v1.27";
#endif

// Default location of the Docker API unix socket
static char *plc_docker_socket = "/var/run/docker.sock";

// Post message template. Used by "create" call
static char *plc_docker_post_message_json =
        "POST %s HTTP/1.1\r\n"
        "Host: http\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "%s";

// Post message with text content. Used by "start" and "wait" calls
static char *plc_docker_post_message_text =
        "POST %s HTTP/1.1\r\n"
        "Host: http\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "%s";

static char *plc_docker_get_message =
        "GET /%s/containers/%s/json HTTP/1.1\r\nHost: http\r\n\r\n";

// JSON body of the "create" call with container creation parameters
static char *plc_docker_create_request =
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

//"DELETE /%s/containers/%s?v=1 HTTP/1.1\r\n\r\n";
// Request for deleting the container
static char *plc_docker_delete_request =
        "DELETE /%s/containers/%s?v=1&force=1 HTTP/1.1\r\n"
        "Host: http\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "%s";

/* End of templates */

/* Static functions of the Docker API module */
static int get_content_length(char *msg, int *len);
static int send_message(int sockfd, char *message);
static int recv_message(int sockfd, char **response);
static int docker_call(int sockfd, char *request, char **response);
static int plc_docker_container_command(int sockfd, char *name, const char *cmd);

static int get_return_status( char * msg )
{
    // look for 200 to tell us the request was OK
    char * http11 = strstr(msg, "HTTP/1.1");
    long http_status = 0;
    char *tailptr;

    if (http11 != NULL ){
        http_status = strtol(&msg[8], &tailptr, 10);
    }
    return http_status;
}
static int get_content_length(char *msg, int *len) {
    char *contentlen = NULL;
    char *content = NULL;

    contentlen = strstr(msg, "No Content");
    if (contentlen) {
        *len = 1;
        return 0;
    }

    contentlen = strstr(msg, "Content-Length:");
    if (!contentlen) {
        return -1;
    }

    content = strstr(msg, "\r\n\r\n");
    if (!content) {
        return -1;
    }

    contentlen += strlen("Content-Length:") + 1;
    *len = strtol(contentlen, NULL, 10);
    *len += content - msg + 4;

    return 0;
}

static int send_message(int sockfd, char *message) {
    int sent = 0;
    int len = strlen(message);

    while (sent < len) {
        int bytes = 0;

        bytes = send(sockfd, message+sent, len-sent, 0);
        if (bytes < 0) {
            snprintf(api_error_message, sizeof(api_error_message), "Error writing message to the Docker API socket: '%s'", strerror(errno));
			return bytes;
        }

        sent += bytes;
    }

    return 0;
}

static int recv_message(int sockfd, char **response) {
    int   received = 0;
    int   len = 0;
    char *buf;
    int   buflen = 8192;
    int   status = 0;

    buf = palloc(buflen);
    memset(buf, 0, buflen);

    while (len == 0 || received < len) {
        int bytes = 0;

        bytes = recv(sockfd, buf + received, buflen - received, 0);
        if (bytes < 0) {
			snprintf(api_error_message, sizeof(api_error_message), "Error reading message from the Docker API socket: '%s'", strerror(errno));
            return bytes;
        }
        received += bytes;

        if (len == 0) {
            status =  get_return_status(buf);

			if (status >= 300){
				*response = buf;
				return status;
			}

            /* Parse the message to find Content-Length */
            get_content_length(buf, &len);

            /* If the message will not fit into buffer - reallocate it */
            if (len + 1000 > buflen) {
                int   newbuflen;
                char *newbuf;

                newbuflen = len + 1000;
                newbuf = palloc(newbuflen);

                /* Copy the old buffer data into the new buffer */
                memset(newbuf, 0, newbuflen);
                memcpy(newbuf, buf, buflen);

                /* Free old buffer replacing it with the new one */
                pfree(buf);
                buf = newbuf;
                buflen = newbuflen;
            }
        }
    }

    *response = buf;
    return status;
}

static int inspect_string_mapping(int sockfd, char **element, plcInspectionMode type) {
    int   received = 0;
    char *buf;
    int   buflen = 16384;
    bool  headercheck = false;
    char *regex;

    buf = palloc(buflen);
    memset(buf, 0, buflen);
    while (received < buflen) {
        int bytes = 0;

        bytes = recv(sockfd, buf + received, buflen - received, 0);
        if (bytes < 0) {
			snprintf(api_error_message, sizeof(api_error_message),
					"Error reading message from the Docker API socket: '%s'",
					strerror(errno));
			pfree(buf);
            return bytes;
        }
        received += bytes;

        /* Check that the message contain correct HTTP response header */
        if (!headercheck) {
            if (strncmp(buf, "HTTP/1.1 404 ", strlen("HTTP/1.1 404 ")) == 0 &&
				type == PLC_INSPECT_STATUS) {

				/*
				 * container does not exist, might be already deleted,
				 * return OK with message
				 */

				*element = pstrdup("unexist");
				pfree(buf);
				return 0;
			}

            if (strncmp(buf, "HTTP/1.1 200 ", strlen("HTTP/1.1 200 ")) != 0) {
				elog(LOG, "Cannot inspect docker container, response: %s", buf);
				snprintf(api_error_message, sizeof(api_error_message),
						"Cannot inspect docker container");
				pfree(buf);
                return -1;
            }

            headercheck = true;
        }

        if (type == PLC_INSPECT_PORT) {
            regex =
                    "\"8080\\/tcp\"\\s*\\:\\s*\\[.*\"HostPort\"\\s*\\:\\s*\"([0-9]*)\".*\\]";
        } else if (type == PLC_INSPECT_STATUS) {
#ifdef DOCKER_API_LOW
			regex = "\\s*\"Running\\s*\"\\:\\s*(\\w+)\\s*";
#else
			regex = "\\s*\"Status\\s*\"\\:\\s*\"(\\w+)\"\\s*";
#endif
        }

        if (docker_parse_string_mapping(buf, element, regex) == 0) {
            break;
        }

		/* If we reach the end of the http packet. */
        if (strstr(buf, "\r\n0\r\n")) {
			elog(LOG, "Cannot inspect container information: %s", buf);
			snprintf(api_error_message, sizeof(api_error_message),
					"Cannot inspect docker information.");
            return -1;
        }

        /* If the buffer is close to the end, we shift it to the beginning */
        if (buflen - received < 1000) {
            memcpy(buf, buf + received - 1000, 1000);
            received = 1000;
            memset(buf + received, 0, buflen - received);
        }
    }

	pfree(buf);
    return 0;
}

static int docker_call(int sockfd, char *request, char **response) {
    int res = 0;

    res = send_message(sockfd, request);
    if (res < 0) {
        return res;
    }

    res = recv_message(sockfd, response);
    if (res < 0) {
        return res;
    }

    return res;
}

static int plc_docker_container_command(int sockfd, char *name, const char *cmd) {
    char *message     = NULL;
    char *apiendpoint = NULL;
    char *response    = NULL;
    char *apiendpointtemplate = "/%s/containers/%s/%s";
    int   res = 0;

    /* Get Docker API endpoint for current API version */
    apiendpoint = palloc(20 + strlen(apiendpointtemplate) + strlen(plc_docker_api_version)
                            + strlen(name) + strlen(cmd));
    sprintf(apiendpoint,
            apiendpointtemplate,          // URL to request
            plc_docker_api_version,       // Docker API version we use
            name,                         // Container name
            cmd);                         // Command to execute

    /* Fill in the HTTP message */
    message = palloc(40 + strlen(plc_docker_post_message_text) + strlen(apiendpoint));
    sprintf(message,
            plc_docker_post_message_text, // POST message template
            apiendpoint,                  // API endpoint to call
            0,                            // Content-Length of the message we passing
            "");                          // Content of the message (empty message)

    res = docker_call(sockfd, message, &response);

    pfree(apiendpoint);
    pfree(message);
    if (response) {
        pfree(response);
    }

    return res;
}

int plc_docker_connect() {
    struct sockaddr_un address;
    int                sockfd;

    /* Create the socket */
    sockfd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
		snprintf(api_error_message, sizeof(api_error_message),
				"Error creating UNIX socket for Docker API: %s", strerror(errno));
        return -1;
    }

    /* Clean up address structure and fill the socket address */
    memset(&address, 0, sizeof(struct sockaddr_un));
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, plc_docker_socket);

    /* connect the socket */
    if (connect(sockfd, (struct sockaddr *)&address, sizeof(address)) < 0) {
		snprintf(api_error_message, sizeof(api_error_message),
				"Error connecting to the Docker API socket '%s': %s",
				plc_docker_socket, strerror(errno));
        return -1;
    }

    return sockfd;
}

int plc_docker_create_container(int sockfd, plcContainerConf *conf, char **name, int container_slot) {
    char *message      = NULL;
    char *message_body = NULL;
    char *apiendpoint  = NULL;
    char *response     = NULL;
    char *sharing      = NULL;
    char *apiendpointtemplate = "/%s/containers/create";
    int   res = 0;
	bool  has_error;

    /* Get Docker API endpoint for current API version */
    apiendpoint = palloc(20 + strlen(apiendpointtemplate) + strlen(plc_docker_api_version));
    sprintf(apiendpoint,
            apiendpointtemplate,
            plc_docker_api_version);

    /* Get Docket API "create" call JSON message body */
    sharing = get_sharing_options(conf, container_slot, &has_error);
	if (has_error == true) {
		return -1;
	}

    message_body = palloc(40 + strlen(plc_docker_create_request) + strlen(conf->command)
                             + strlen(conf->dockerid) + strlen(sharing));
    sprintf(message_body,
            plc_docker_create_request,
            conf->command,
            conf->isNetworkConnection ? "true" : "false",
            conf->isNetworkConnection ? "false" : "true",
            conf->dockerid,
            sharing,
            ((long long)conf->memoryMb) * 1024 * 1024);

    /* Fill in the HTTP message */
    message = palloc(40 + strlen(plc_docker_post_message_json) + strlen(apiendpoint)
                        + strlen(message_body));
    sprintf(message,
            plc_docker_post_message_json, // HTTP POST request template
            apiendpoint,                  // API endpoint to call
            strlen(message_body),         // Content-length
            message_body);                // POST message JSON content

    res = docker_call(sockfd, message, &response);
    pfree(apiendpoint);
    pfree(sharing);
    pfree(message_body);
    pfree(message);

	if (res == 201) {
		res = 0;
	} else if (res >= 0) {
		elog(LOG, "Docker can not create container, response: %s", response);
		snprintf(api_error_message, sizeof(api_error_message),
				"Failed to create container (return code: %d).", res);
		res = -1;
	}

	if (res < 0) {
		goto cleanup;
	}

    res = docker_parse_container_id(response, name);
    if (res < 0) {
		snprintf(api_error_message, sizeof(api_error_message),
				 "Error parsing container ID during creating container");
		goto cleanup;
    }

cleanup:
    if (response) {
        pfree(response);
    }

    return res;
}

int plc_docker_start_container(int sockfd, char *name) {
    int res;

	res = plc_docker_container_command(sockfd, name, "start");
	if (res == 204 || res == 304) {
		res = 0;
	} else if (res >= 0) {
		snprintf(api_error_message, sizeof(api_error_message),
				"Failed to start container (return code: %d)", res);
		res = -1;
	}

	return res;
}

int plc_docker_kill_container(int sockfd, char *name) {
	elog(FATAL, "Not finished yet. Do not call it.");
    return plc_docker_container_command(sockfd, name, "kill?signal=KILL");
}

int plc_docker_inspect_container(int sockfd, char *name, char **element, plcInspectionMode type) {
    char *message;
    int  res = 0;

    /* Fill in the HTTP message */
    message = palloc(20 + strlen(plc_docker_get_message) + strlen(name)
                        + strlen(plc_docker_api_version));
    sprintf(message,
            plc_docker_get_message,
            plc_docker_api_version,
            name);

    res = send_message(sockfd, message);
    pfree(message);
    if (res < 0) {
        return res;
    }

    res = inspect_string_mapping(sockfd, element, type);

    return res;
}

int plc_docker_wait_container(int sockfd, char *name) {
	elog(FATAL, "Not finished yet. Do not call it.");
    return plc_docker_container_command(sockfd, name, "wait");
}

int plc_docker_delete_container(int sockfd, char *name) {
    char *message;
    char *response = NULL;
    int   res = 0;

    /* Fill in the HTTP message */
    message = palloc(20 + strlen(plc_docker_delete_request) + strlen(name)
                        + strlen(plc_docker_api_version));
    sprintf(message,
            plc_docker_delete_request, // Delete message template
            plc_docker_api_version,    // API version
            name,
	    0,
	    "");                     // Container name

    res = docker_call(sockfd, message, &response);
    pfree(message);
    if (response) {
        pfree(response);
    }
	/* 200 = deleted success, 404 = container not found, both are OK for delete */
	if (res == 204 || res == 404) {
		res = 0;
	} else if (res >= 0) {
		snprintf(api_error_message, sizeof(api_error_message),
				"Failed to delete container (return code: %d)", res);
		res = -1;
	}

    return res;
}

int plc_docker_disconnect(int sockfd) {
    return close(sockfd);
}

#endif
