/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */

#include "plc_docker_common.h"
#include "regex/regex.h"

int docker_inspect_string(char *buf, char **element, plcInspectionMode type) {
	elog(DEBUG1, "plcontainer: docker_inspect_string:%s", buf);
	struct json_object *response = json_tokener_parse(buf);
	if (response == NULL)
		return -1;
	if (type == PLC_INSPECT_NAME) {
		struct json_object *nameidObj = NULL;
		if (!json_object_object_get_ex(response, "Id", &nameidObj)) {
			elog(WARNING, "failed to get json \"Id\" field.");
			return -1;
		}
		const char *namestr = json_object_get_string(nameidObj);
		*element = pstrdup(namestr);
		return 0;
	} else if (type == PLC_INSPECT_PORT) {
		struct json_object *NetworkSettingsObj = NULL;
		if (!json_object_object_get_ex(response, "NetworkSettings", &NetworkSettingsObj)) {
			elog(WARNING, "failed to get json \"NetworkSettings\" field.");
			return -1;
		}
		struct json_object *PortsObj = NULL;
		if (!json_object_object_get_ex(NetworkSettingsObj, "Ports", &PortsObj)) {
			elog(WARNING, "failed to get json \"Ports\" field.");
			return -1;
		}
		struct json_object *HostPortArray = NULL;
		if (!json_object_object_get_ex(PortsObj, "8080/tcp", &HostPortArray)) {
			elog(WARNING, "failed to get json \"HostPortArray\" field.");
			return -1;
		}
		if (json_object_get_type(HostPortArray) != json_type_array) {
			elog(WARNING, "no element found in json \"HostPortArray\" field.");
			return -1;
		}
		int arraylen = json_object_array_length(HostPortArray);
		for (int i = 0; i < arraylen; i++) {
			struct json_object *PortBindingObj = NULL;
			PortBindingObj = json_object_array_get_idx(HostPortArray, i);
			if (PortBindingObj == NULL) {
				elog(WARNING, "failed to get json \"PortBinding\" field.");
				return -1;
			}
			struct json_object *HostPortObj = NULL;
			if (!json_object_object_get_ex(PortBindingObj, "HostPort", &HostPortObj)) {
				elog(WARNING, "failed to get json \"HostPort\" field.");
				return -1;
			}
			const char *HostPortStr = json_object_get_string(HostPortObj);
			*element = pstrdup(HostPortStr);
			return 0;
		}
	} else if (type == PLC_INSPECT_STATUS) {
		struct json_object *StateObj = NULL;
		if (!json_object_object_get_ex(response, "State", &StateObj)) {
			elog(WARNING, "failed to get json \"State\" field.");
			return -1;
		}
#ifdef DOCKER_API_LOW
		struct json_object *RunningObj = NULL;
		if (!json_object_object_get_ex(StateObj, "Running", &RunningObj)) {
			elog(WARNING, "failed to get json \"Running\" field.");
			return -1;
		}
		const char *RunningStr = json_object_get_string(RunningObj);
		*element = pstrdup(RunningStr);
		return 0;
#else
		struct json_object *StatusObj = NULL;
		if (!json_object_object_get_ex(StateObj, "Status", &StatusObj)) {
			elog(WARNING, "failed to get json \"Status\" field.");
			return -1;
		}
		const char *StatusStr = json_object_get_string(StatusObj);
		*element = pstrdup(StatusStr);
		return 0;
#endif
	} else {
		elog(LOG, "Error PLC inspection mode, unacceptable inpsection type %d", type);
		return -1;
	}

	return -1;
}

