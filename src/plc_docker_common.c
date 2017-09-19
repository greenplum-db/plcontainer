/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#include "plc_docker_common.h"
#include "regex/regex.h"

int docker_parse_string_mapping(char *response, char **element, char *plc_docker_regex) {
    // Regular expression for parsing container inspection JSON response

    regex_t     preg;
    regmatch_t  pmatch[2];
    int         res = 0;
    int         len = 0;
    int         wmasklen, masklen;
    pg_wchar   *mask;
    int         wdatalen, datalen;
    pg_wchar   *data;

    masklen = strlen(plc_docker_regex);
    mask = (pg_wchar *) palloc((masklen + 1) * sizeof(pg_wchar));
    wmasklen = pg_mb2wchar_with_len(plc_docker_regex, mask, masklen);

    res = pg_regcomp(&preg, mask, wmasklen, REG_ADVANCED);
    pfree(mask);
    if (res < 0) {
        elog(LOG, "Cannot compile Postgres regular expression: '%s'", strerror(errno));
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
        elog(LOG, "No regex match '%s' for '%s'", plc_docker_regex, response);
        return -1;
    }

    if (pmatch[1].rm_so == -1) {
        elog(LOG, "Could not find regex match '%s' for '%s'", plc_docker_regex, response);
        return -1;
    }

    len = pmatch[1].rm_eo - pmatch[1].rm_so;
    *element = palloc(len + 1);
    memcpy(*element, response + pmatch[1].rm_so, len);
    (*element)[len] = '\0';

    return 0;
}

int docker_inspect_string(char *buf, char **element, plcInspectionMode type) {
    char *regex = NULL;
	int res = 0;

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

	res = docker_parse_string_mapping(buf, element, regex);

    return res;
}

/* Parse container ID out of JSON response */
int docker_parse_container_id(char* response, char **name) {
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
        elog(LOG, "Cannot compile Postgres regular expression: '%s'", strerror(errno));
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
        elog(LOG, "Docker API response does not match regular expression: '%s'", response);
        return -1;
    }

    if (pmatch[1].rm_so == -1) {
        elog(LOG, "Postgres regex failed to extract created container name from Docker API response: '%s'", response);
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
        elog(LOG, "Docker API 'create' call returned warning message: '%s'", err);
    }

    pg_regfree(&preg);
    return 0;
}
