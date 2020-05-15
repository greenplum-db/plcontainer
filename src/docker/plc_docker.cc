extern "C" {
    #include "plc/runtime_config.h"
    #include "common/comm_connectivity.h"
    #include "plc/plc_coordinator.h"
}
#include "docker/plc_docker.h"
#include "docker/docker_client.h"

int PlcDocker_create(runtimeConfEntry *conf, char **name, char *uds_dir, int qe_pid, int session_id, int ccnt, int uid, int gid,int procid, int dbid, char *ownername) {
    std::string id = PlcDocker::create(conf, std::string(uds_dir), qe_pid, session_id, ccnt, uid, gid, procid, dbid, std::string(ownername));
    if (id.length() == 0) {
        return -1;
    }
    strncpy(*name, id.c_str(), DEFAULT_STRING_BUFFER_SIZE);
    return 0;
}

int PlcDocker_start(const char *id, char *msg) {
    std::string start_res;
    int res = PlcDocker::start(std::string(id), start_res);
    if (res < 0) {
        strncpy(msg, start_res.c_str(), DEFAULT_STRING_BUFFER_SIZE);
    }
    return res;
}
int PlcDocker_delete(const char** ids, int length, char *msg) {
    std::vector<std::string> container_ids(length-1);
    for (int i = 0; i < length; i++) {
        container_ids.push_back(std::string(ids[i]));
    }
    std::string remove_res;
    int res = PlcDocker::remove(container_ids, remove_res);
    if (res < 0) {
        strncpy(msg, remove_res.c_str(), DEFAULT_STRING_BUFFER_SIZE);
    }
    return res;
}
int PlcDocker_inspect(ContainerEntry** entries, int length) {
    std::vector<std::string> container_ids(length);
    std::vector<std::string> status(length);
    for (int i = 0; i < length; i++) {
        container_ids.push_back(std::string(entries[i]->containerId));
    }
    return PlcDocker::inspect_status(container_ids,status);
}

int PlcDocker_stat(const char** ids, int length, int64_t *mem_usage) {
    std::vector<std::string> container_ids;
    std::vector<int64_t> mem_usage_vec;
    for (int i = 0; i < length; i++) {
        container_ids.push_back(std::string(ids[i]));
    }
    int res = PlcDocker::mem_stats(container_ids, mem_usage_vec);
    if (res > 0) {
        if (mem_usage_vec.size() != length) {
            return -1;
        }
        for (int i = 0; i < length; i++) {
            mem_usage[i] = mem_usage_vec[i];
        }
    }
    return res;
}
JSON_VAL PlcDocker::get_volumes(JSON_DOC& param, runtimeConfEntry *conf, std::string uds_dir, bool& has_error) {
    JSON_VAL volumes(rapidjson::kArrayType);
	has_error = false;
	if (conf->nSharedDirs >= 0) {
		for (int i = 0; i < conf->nSharedDirs; i++) {
			std::string volume_string;

			if (conf->sharedDirs[i].mode == PLC_ACCESS_READONLY) {
                volume_string = std::string(conf->sharedDirs[i].host) + ":" + std::string(conf->sharedDirs[i].container)+":ro";
			} else if (conf->sharedDirs[i].mode == PLC_ACCESS_READWRITE) {
				volume_string = std::string(conf->sharedDirs[i].host) + ":" + std::string(conf->sharedDirs[i].container) + ":rw";
			} else {
				has_error = true;
				return volumes;
			}
            rapidjson::Value strVal;
            strVal.SetString(volume_string.c_str(), volume_string.length(), param.GetAllocator());
            volumes.PushBack(strVal, param.GetAllocator());
		}

		if (!conf->useContainerNetwork) {
			/* Directory for QE : IPC_GPDB_BASE_DIR + "." + PID + "." + container_slot */
			std::string volume_string = uds_dir + ":" + std::string(IPC_CLIENT_DIR)+":rw";
            rapidjson::Value strVal;
            strVal.SetString(volume_string.c_str(), volume_string.length(), param.GetAllocator());
			volumes.PushBack(strVal, param.GetAllocator());
		}
	}
    return volumes;
}
std::string PlcDocker::create(runtimeConfEntry *conf, std::string uds_dir,int qe_pid, int session_id, int ccnt, int uid, int gid,int procid, int dbid, std::string ownername) {
    JSON_DOC param(rapidjson::kObjectType);
    JSON_VAL commands(rapidjson::kArrayType);
    JSON_VAL host_config(rapidjson::kObjectType);
    bool has_error = false;
    JSON_VAL volumes = get_volumes(param, conf, uds_dir, has_error);
    if (!has_error) {
        host_config.AddMember("Binds", volumes, param.GetAllocator());
    }
    JSON_VAL logconfig(rapidjson::kObjectType);
    std::string logconfigVal = conf->useContainerLogging ? "journald" : "none";
    logconfig.AddMember("Type", logconfigVal, param.GetAllocator());
    host_config.AddMember("LogConfig", logconfig, param.GetAllocator());
    host_config.AddMember("Memory", conf->memoryMb*1024*1024, param.GetAllocator());
    host_config.AddMember("CpuShares", conf->cpuShare, param.GetAllocator());
    host_config.AddMember("PublishAllPorts", true, param.GetAllocator());
    host_config.AddMember("IpcMode", "shareable", param.GetAllocator());
    JSON_VAL labels(rapidjson::kObjectType);
    JSON_VAL environmet(rapidjson::kArrayType);
    std::string env_string = "EXECUTOR_UID="+std::to_string(uid);
    rapidjson::Value strVal;
    strVal.SetString(env_string.c_str(), env_string.length(), param.GetAllocator());
    environmet.PushBack(strVal, param.GetAllocator());
    env_string = "EXECUTOR_GID="+std::to_string(gid);
    strVal.SetString(env_string.c_str(), env_string.length(), param.GetAllocator());
    environmet.PushBack(strVal,param.GetAllocator());
    env_string = "DB_QE_PID="+std::to_string(procid);
    strVal.SetString(env_string.c_str(), env_string.length(), param.GetAllocator());
    environmet.PushBack(strVal, param.GetAllocator());
    env_string = "USE_CONTAINER_NETWORK="+ std::string(conf->useContainerNetwork?"true":"false");
    strVal.SetString(env_string.c_str(), env_string.length(), param.GetAllocator());
    environmet.PushBack(strVal,param.GetAllocator());
    env_string = std::string(conf->command);
    strVal.SetString(env_string.c_str(), env_string.length(), param.GetAllocator());
    commands.PushBack(strVal, param.GetAllocator());
    param.AddMember("Cmd", commands, param.GetAllocator());
    param.AddMember("Env", environmet, param.GetAllocator());
    param.AddMember("AttachStdin", false, param.GetAllocator());
    param.AddMember("AttachStdout", conf->useContainerLogging, param.GetAllocator());
    param.AddMember("AttachStderr", conf->useContainerLogging, param.GetAllocator());
    param.AddMember("Tty", false, param.GetAllocator());
    param.AddMember("NetworkDisabled", !conf->useContainerNetwork, param.GetAllocator());
    param.AddMember("Image", std::string(conf->image), param.GetAllocator());

    param.AddMember("HostConfig", host_config, param.GetAllocator());

    labels.AddMember("qepid", std::to_string(qe_pid), param.GetAllocator());
    labels.AddMember("sessionid", std::to_string(session_id), param.GetAllocator());
    labels.AddMember("ccnt", std::to_string(ccnt), param.GetAllocator());
    labels.AddMember("plcontainer", "true", param.GetAllocator());
    labels.AddMember("dbid", std::to_string(dbid), param.GetAllocator());
    labels.AddMember("owner", ownername, param.GetAllocator());
    param.AddMember("Labels", labels, param.GetAllocator());
    Docker client = Docker();
    JSON_DOC res = client.create_container(param);
    std::string container_id;
    if (res.HasMember("success") && res["success"].IsBool() && res["success"].GetBool()) {
        container_id = res["data"]["Id"].GetString();
    }
    res.SetNull();
    res.GetAllocator().Clear();
    param.SetNull();
    param.GetAllocator().Clear();
    return container_id;
}


int PlcDocker::start(std::string id, std::string& result) {
    Docker client = Docker();
    JSON_DOC res = client.start_container(id);
    if (res["success"] == true && res["data"].IsString()) {
        if (res["data"].GetStringLength() == 0) {
            return 0;
        } else {
            result = res["data"].GetString();
            return -1;
        }
    } else if (res["success"] == false) {
        result = jsonToString(res);
        return -1;
    }

	return -1;
}

int PlcDocker::remove(std::vector<std::string>& ids, std::string& result) {
    Docker client = Docker();
    JSON_DOC res = client.delete_containers(ids);
    if (res["data"].IsString() && res["data"].GetStringLength() > 0) {
        result = res["data"].GetString();
        return -1;
    }
    return 0;
}

int PlcDocker::inspect_status(std::vector<std::string>& ids, std::vector<std::string>& status) {
    Docker client = Docker();
    JSON_DOC res = client.inspect_containers(ids);
	(void) status;
    return 0;
}

int PlcDocker::mem_stats(std::vector<std::string>& ids, std::vector<std::int64_t>& mem_usage) {
    Docker client = Docker();
    JSON_DOC res = client.stat_containers(ids);
    if (res["success"] == true && res["data"].IsString()) {
        if (res["data"].GetStringLength() == 0) {
            return 0;
        } else {
            std::string stat_string = res["data"].GetString();
            std::string delimiter = "\n";
            JSON_DOC stat;
            size_t pos = 0;
            std::string token;
            while ((pos = stat_string.find(delimiter)) != std::string::npos) {
                token = stat_string.substr(0, pos);
                if (token.length() <= 0) {
                    break;
                }
                stat.Parse(token);
                if (stat.HasMember("memory_stats") && stat["memory_stats"].IsObject() == true) {
                    if (stat["memory_stats"].HasMember("usage") && stat["memory_stats"]["usage"].IsInt64() == true) {
                        mem_usage.push_back(stat["memory_stats"]["usage"].GetInt64());
                    }
                } else {
                    mem_usage.push_back(0);
                }
                stat_string.erase(0, pos + delimiter.length());
            }
            return 1;
        }
    } else if (res["success"] == false) {
        return -1;
    }

	return -1;
}
