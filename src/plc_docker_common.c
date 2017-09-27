/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#include "plc_docker_common.h"
#include "regex/regex.h"

static int docker_parse_string_mapping(char *response, regmatch_t pmatch[], int size, char *plc_docker_regex) {
    // Regular expression for parsing container inspection JSON response

    regex_t     preg;
    int         res = 0;
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
                     size,
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

    pg_regfree(&preg);
    return 0;
}

int docker_inspect_string(char *buf, char **element, plcInspectionMode type) {
    char *regex = NULL;
	int res = 0;
    regmatch_t  pmatch[2];
    int         len = 0;

    switch (type) {
        case PLC_INSPECT_PORT:
            regex =
            	"\"8080\\/tcp\"\\s*\\:\\s*\\[.*\"HostPort\"\\s*\\:\\s*\"([0-9]*)\".*\\]";
            break;
        case PLC_INSPECT_STATUS:
#ifdef DOCKER_API_LOW
		    regex = "\\s*\"Running\\s*\"\\:\\s*(\\w+)\\s*";
#else
		    regex = "\\s*\"Status\\s*\"\\:\\s*\"(\\w+)\"\\s*";
#endif
            break;
        case PLC_INSPECT_NAME:
            regex =
                "\\{\\s*\"[Ii][Dd]\\s*\"\\:\\s*\"(\\w+)\"\\s*,\\s*\"[Ww]arnings\"\\s*\\:([^\\}]*)\\s*\\}";
            break;
        default:
            elog(LOG, "Error PLC inspection mode, unacceptable inpsection type %d", type);
            return -1;
    }

	res = docker_parse_string_mapping(buf, pmatch, 2, regex);

    len = pmatch[1].rm_eo - pmatch[1].rm_so;
    *element = palloc(len + 1);
    memcpy(*element, buf + pmatch[1].rm_so, len);
    (*element)[len] = '\0';

    return res;
}

