#include "docker/docker_client.h"
#include <utility>
#include <sstream>

Docker::Docker() : host_uri("http:/v1.40"){
    curl_global_init(CURL_GLOBAL_ALL);
    is_remote = false;
    mActiveTransfers = 0;
}

Docker::~Docker(){
    curl_global_cleanup();
}

JSON_DOC Docker::inspect_containers(const std::vector<std::string>& container_ids){
    std::vector<std::string> paths;
    for (unsigned int i  = 0; i < container_ids.size(); i++) {
        std::string path = "/containers/" + container_ids[i] + "/json";
        paths.push_back(path);
    }
    JSON_DOC param = JSON_DOC();
    return requestAndParseJson(GET,paths, param);
}

JSON_DOC Docker::list_containers(JSON_DOC& filters, bool all, int limit, const std::string& since, const std::string& before, int size) {
    std::string path = "/containers/json?";
    path += param("all", all);
    path += param("limit", limit);
    path += param("since", since);
    path += param("before", before);
    path += param("size", size);
    path += param("filters", filters);
    JSON_DOC param = JSON_DOC();
    std::vector<std::string> paths;
    paths.push_back(path);
    return requestAndParseJson(GET, paths, param, 2);
}

JSON_DOC Docker::create_container(JSON_DOC& parameters){
    std::vector<std::string> paths;
    std::string path = "/containers/create";
    paths.push_back(path);
    JSON_DOC res = requestAndParseJson(POST,paths,parameters, 201);
    return res;
}
JSON_DOC Docker::start_container(const std::string& container_id){
    std::vector<std::string> paths;
    std::string path = "/containers/" + container_id + "/start";
    paths.push_back(path);
    JSON_DOC param = JSON_DOC();
    JSON_DOC res = requestAndParse(POST, paths, param, 204);
    param.SetNull();
    param.GetAllocator().Clear();
    return res;
}

JSON_DOC Docker::delete_containers(const std::vector<std::string>& container_ids, bool v, bool force){
    std::vector<std::string> paths;
    for (unsigned int i  = 0; i < container_ids.size(); i++) {
        if (container_ids[i].length() == 0)
            continue;
        std::string path = "/containers/" + container_ids[i] + "?";
        path += param("v", v);
        path += param("force", force);
        paths.push_back(path);
    }
    JSON_DOC param = JSON_DOC();
    return requestAndParse(DELETE,paths,param, 204);
}
JSON_DOC Docker::stat_containers(const std::vector<std::string>& container_ids, bool is_stream) {
    std::vector<std::string> paths;
    for (unsigned int i  = 0; i < container_ids.size(); i++) {
        if (container_ids[i].length() == 0)
            continue;
        std::string path = "/containers/" + container_ids[i] + "/stats?";
        path += param("stream", is_stream);
        paths.push_back(path);
    }
    JSON_DOC param = JSON_DOC();
    return requestAndParse(GET,paths,param);
}
int Docker::multiCurlRequests(std::string& readBuffer, const std::vector<std::string>& paths, JSON_DOC& param, std::string method_str) {
    curlm = curl_multi_init();
    std::vector<CURL *> handles;
    if(!curlm){
        //plc_elog("error while initiating curl");
        curl_global_cleanup();
        exit(1);
    }
    rapidjson::StringBuffer buffer;
    std::string paramString;
    const char *paramChar;
    struct curl_slist *headers = nullptr;
    buffer.Clear();
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    param.Accept(writer);
    paramString = std::string(buffer.GetString());
    paramChar = paramString.c_str();
    headers = curl_slist_append(headers, "Content-Type: application/json");

    for (unsigned int i = 0; i < paths.size(); i++) {
        CURL* curl = curl_easy_init();
        handles.push_back(curl);
        std::string path = paths[i];
        curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, "/var/run/docker.sock");
        curl_easy_setopt(curl, CURLOPT_URL, (host_uri + path).c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method_str.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        if(method_str == "POST"){
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, paramChar);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(paramChar));
        }
        curl_multi_add_handle(curlm, curl);
    }
    CURLMcode res;
    while ((res = curl_multi_perform(curlm, &mActiveTransfers)) == CURLM_CALL_MULTI_PERFORM) {}
    if(res != CURLM_OK)
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_multi_strerror(res));
    while (mActiveTransfers) {
        int rc = 0;
        fd_set fdread, fdwrite, fdexecp;
        int maxfd = -1;
        long curl_timeno = -1;
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        FD_ZERO(&fdread);
        FD_ZERO(&fdwrite);
        FD_ZERO(&fdexecp);
        curl_multi_timeout(curlm, &curl_timeno);
        if (curl_timeno > 0) {
            timeout.tv_sec = curl_timeno/1000;
            if (timeout.tv_sec > 1)
                timeout.tv_sec = 1;
            else
                timeout.tv_usec = (curl_timeno % 1000) * 1000;
        }
        res = curl_multi_fdset(curlm, &fdread, &fdwrite, &fdexecp, &maxfd);
        if (res != CURLM_OK) {
            fprintf(stderr, "curl_multi_fdset() failed: %s\n", strerror(errno));
        }
        if (maxfd == -1) {
            fprintf(stderr, "No socket");
        } else {
            rc = select(maxfd+1, &fdread, &fdwrite, &fdexecp, &timeout);
        }
        switch(rc) {
        case -1:
            fprintf(stderr, "select() failed: %s\n", strerror(errno));
            if (errno == EINTR || errno == EAGAIN) {
            }
            break;
        case 0:
        default:
            curl_multi_perform(curlm, &mActiveTransfers);
            break;
        }
    }
    curl_slist_free_all(headers);
    curl_multi_cleanup(curlm);
    for(unsigned int i = 0; i<paths.size(); i++)
        curl_easy_cleanup(handles[i]);

    return 0;
}

JSON_DOC Docker::requestAndParse(Method method, const std::vector<std::string>& paths, JSON_DOC& param, long success_code){
    std::string readBuffer;
    std::string method_str;

	(void) success_code;

    switch(method){
        case GET:
            method_str = "GET";
            break;
        case POST:
            method_str = "POST";
            break;
        case DELETE:
            method_str = "DELETE";
            break;
        case PUT:
            method_str = "PUT";
            break;
        default:
            method_str = "GET";
    }
    long status = 0;
    multiCurlRequests(readBuffer, paths, param, method_str);
    const char* buf = readBuffer.c_str();
    JSON_DOC doc(rapidjson::kObjectType);
    struct CURLMsg *m;
    do {
        int msgq = 0;
        m = curl_multi_info_read(curlm, &msgq);
        if (m && (m->msg == CURLMSG_DONE)) {
            CURL *e = m->easy_handle;
			if (status == 0 || status == CURLE_OK)
			{
				status = m->data.result;
			}

            curl_multi_remove_handle(curlm, e);
            curl_easy_cleanup(e);
        }
    } while(m);
    if(status == CURLE_OK){
        doc.AddMember("success", true, doc.GetAllocator());

        JSON_VAL dataString;
        dataString.SetString(readBuffer.c_str(), doc.GetAllocator());

        doc.AddMember("data", dataString, doc.GetAllocator());
    }else{
        JSON_DOC resp(&doc.GetAllocator());
        resp.Parse(buf);
        doc.AddMember("success", false, doc.GetAllocator());
        doc.AddMember("code", status, doc.GetAllocator());
        doc.AddMember("data", resp, doc.GetAllocator());
        resp.SetNull();
        resp.GetAllocator().Clear();
    }
    return doc;
}

JSON_DOC Docker::requestAndParseJson(Method method, const std::vector<std::string>& paths, JSON_DOC& param, long success_code){
    JSON_DOC result_obj = requestAndParse(method,paths,param, success_code);
    bool result = (result_obj.HasMember("success") && result_obj["success"].IsBool() && result_obj["success"].GetBool());
    if(result){
        JSON_DOC doc(rapidjson::kObjectType);

        JSON_DOC data(&doc.GetAllocator());
        data.Parse(result_obj["data"].GetString());

        doc.AddMember("success", true, doc.GetAllocator());
        doc.AddMember("data", data, doc.GetAllocator());
        result_obj.SetNull();
        result_obj.GetAllocator().Clear();
        return doc;
    }else{
        return result_obj;
    }
}

/*
*  
* END Docker Implementation
* 
*/

std::string param( const std::string& param_name, const std::string& param_value){
    if(!param_value.empty()){
        return "&" + param_name + "=" + param_value;
    }
    else{
        return "";
    }
}

std::string param( const std::string& param_name, const char* param_value){
    if(param_value != nullptr){
        return "&" + param_name + "=" + param_value;
    }
    else{
        return "";
    }
}

std::string param( const std::string& param_name, bool param_value){
    std::string ret;
    ret = "&" + param_name + "=";
    if(param_value){
        return ret + "true";
    }
    else{
        return ret + "false";
    }
}

std::string param( const std::string& param_name, int param_value){
    if(param_value != -1){
        std::ostringstream convert;
        convert << param_value;
        return "&" + param_name + "=" + convert.str();
    }
    else{
        return "";
    }
}

std::string param( const std::string& param_name, JSON_DOC& param_value){
    if(param_value.IsObject()){
        std::string paramString;
        rapidjson::StringBuffer buffer;
        buffer.Clear();
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        param_value.Accept(writer);
        paramString = std::string(buffer.GetString());
        return "&" + param_name + "=" + paramString;
    }
    else{
        return "";
    }
}

std::string jsonToString(JSON_VAL & doc){
    rapidjson::StringBuffer buffer;
    buffer.Clear();
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    return std::string(buffer.GetString());
}
