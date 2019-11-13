#include "client.h"
#include "proto_utils.h"

PLContainerClient::PLContainerClient(std::shared_ptr<grpc::Channel> channel) {
    this->stub_ = PLContainer::NewStub(channel);
}

PLContainerClient::PLContainerClient(const plcContext *ctx) {
    this->ctx = ctx;
    std::string uds(ctx->service_address);
    uds = "unix://" + uds;
    this->stub_ = PLContainer::NewStub(grpc::CreateChannel(uds, grpc::InsecureChannelCredentials()));
}

PLContainerClient::PLContainerClient() {
    this->stub_ = NULL;
    this->ctx = NULL;
}

void PLContainerClient::Init(const plcContext *ctx) {
    this->ctx = ctx;
    std::string uds(ctx->service_address);
    uds = "unix://" + uds;
    this->stub_ = PLContainer::NewStub(grpc::CreateChannel(uds, grpc::InsecureChannelCredentials()));
}

void PLContainerClient::FunctionCall(const CallRequest &request, CallResponse &response) {
    grpc::ClientContext context;
    context.set_wait_for_ready(true);
    plc_elog(DEBUG1, "function call request:%s", request.DebugString().c_str());
    grpc::Status status = stub_->FunctionCall(&context, request, &response);
    plc_elog(DEBUG1, "function call response:%s", response.DebugString().c_str());
    if (!status.ok()) {
        plc_elog(ERROR, "plcontainer function call RPC failed., error:%s", status.error_message().c_str());
    } else if (response.has_exception()) {
        Error *error = response.mutable_exception();
        plc_elog(ERROR, "plcontainer function call failed. error:%s stacktrace:%s", error->message().c_str(), error->stacktrace().c_str());
    }
    plc_elog(DEBUG1, "PLContainerClient function call finished with status %d", status.error_code());
}

void PLContainerClient::initCallRequestArgument(const FunctionCallInfo fcinfo, const plcProcInfo *proc, int argIdx, ScalarData &arg) {
    PLContainerProtoUtils::SetScalarValue(arg,
                        proc->argnames[argIdx],
                        fcinfo->argnull[argIdx],
                        &proc->args[argIdx],
                        fcinfo->argnull[argIdx] ? NULL : proc->args[argIdx].outfunc(fcinfo->arg[argIdx], &proc->args[argIdx]));
}

void PLContainerClient::initCallRequestArgument(const FunctionCallInfo fcinfo, const plcProcInfo *proc, int argIdx, ArrayData &arg) {
    if (proc->argnames[argIdx]) {
        arg.set_name(proc->argnames[argIdx]);
    } else {
        arg.set_name("");
    }
     
    if (!fcinfo->argnull[argIdx]) {
        char *argvalue = proc->args[argIdx].outfunc(fcinfo->arg[argIdx], &proc->args[argIdx]);
        int size = *(int *)argvalue;
        if (!arg.ParseFromArray(argvalue+sizeof(int), size)) {
            plc_elog(ERROR, "array outfunc parse failed");
        }
    }

    plc_elog(DEBUG1, "array data parse result:%s", arg.DebugString().c_str());

    // TODO will be enabled once the RServer array type done.

    plc_elog(ERROR, "init call request for array type has not implemented.");
}

void PLContainerClient::initCallRequestArgument(const FunctionCallInfo fcinfo, const plcProcInfo *proc, int argIdx, CompositeData &arg) {
    if (proc->argnames[argIdx]) {
        arg.set_name(proc->argnames[argIdx]);
    } else {
        arg.set_name("");
    }
     
    if (!fcinfo->argnull[argIdx]) {
        char *argvalue = proc->args[argIdx].outfunc(fcinfo->arg[argIdx], &proc->args[argIdx]);
        int size = *(int *)argvalue;
        if (!arg.ParseFromArray(argvalue+sizeof(int), size)) {
            plc_elog(ERROR, "composited outfunc parse failed");
        }
    }

    plc_elog(DEBUG1, "composite data parse result:%s", arg.DebugString().c_str());

    // TODO will be enabled once the RServer composite type done.
    plc_elog(ERROR, "init call request for composite type has not implemented.");
}

void PLContainerClient::initCallRequestArgument(const FunctionCallInfo fcinfo, const plcProcInfo *proc, int argIdx, SetOfData &arg) {
    (void) fcinfo;
    (void) proc;
    (void) argIdx;
    (void) arg;
    plc_elog(ERROR, "init call request for setof type has not implemented.");
}

void PLContainerClient::InitCallRequest(const FunctionCallInfo fcinfo, const plcProcInfo *proc, PlcRuntimeType type, CallRequest &request) {
    plc_elog(DEBUG1, "fcinfo is :%s", PLContainerClient::functionCallInfoToStr(fcinfo).c_str());
    plc_elog(DEBUG1, "proc is %s", PLContainerClient::procInfoToStr(proc).c_str());

    request.set_runtimetype(type);
    request.set_objectid(proc->funcOid);
    request.set_haschanged(proc->hasChanged);
    request.mutable_proc()->set_src(proc->src);
    request.mutable_proc()->set_name(proc->name);
    request.set_loglevel(log_min_messages);
    request.set_rettype(PLContainerProtoUtils::GetDataType(&proc->result));
    if (GetDatabaseEncoding() == PG_SQL_ASCII) {
        request.set_serverenc("ascii");
    } else {
        request.set_serverenc(GetDatabaseEncodingName());            
    }

    for (int i=0;i<proc->nargs;i++) {
        PlcValue *arg = request.add_args();
        arg->set_type(PLContainerProtoUtils::GetDataType(&proc->args[i]));
        if (proc->argnames[i]) {
            arg->set_name(proc->argnames[i]);
        } else {
            arg->set_name("");
        }
        switch (arg->type()) {
        case LOGICAL:
        case INT:
        case REAL:
        case TEXT:
        case BYTEA:
            PLContainerClient::initCallRequestArgument(fcinfo, proc, i, *arg->mutable_scalarvalue());
            break;
        case ARRAY:
            PLContainerClient::initCallRequestArgument(fcinfo, proc, i, *arg->mutable_arrayvalue());
            break;
        case COMPOSITE:
            PLContainerClient::initCallRequestArgument(fcinfo, proc, i, *arg->mutable_compositevalue());
            break;
        case SETOF:
            PLContainerClient::initCallRequestArgument(fcinfo, proc, i, *arg->mutable_setofvalue());
            break;
        default:
            plc_elog(ERROR, "invalid data type %d in argument %d in InitCallRequest", arg->type(), i);
        }
    }
}
Datum PLContainerClient::getCallResponseAsDatum(const FunctionCallInfo fcinfo, plcProcInfo *proc, const ScalarData &response) {
    Datum retresult = (Datum)0;

    if (response.isnull()) {
        return retresult;
    }

    fcinfo->isnull = false;
    char *buffer = NULL;
    switch (proc->result.type) {
    case PLC_DATA_INT1:
        buffer = (char *)palloc(1); 
        *(int8_t *)buffer = response.logicalvalue();
        break;
    case PLC_DATA_INT2:
        buffer = (char *)palloc(2); 
        *(int16_t *)buffer = response.intvalue();
        break;
    case PLC_DATA_INT4:
        buffer = (char *)palloc(4); 
        *(int32_t *)buffer = response.intvalue();
        break;
    case PLC_DATA_INT8:
        buffer = (char *)palloc(8); 
        *(int64_t *)buffer = response.realvalue();
        break;
    case PLC_DATA_FLOAT4:
        buffer = (char *)palloc(4);
        *(float *)buffer = response.realvalue();
        break;
    case PLC_DATA_FLOAT8:
        buffer = (char *)palloc(8);
        *(double *)buffer = response.realvalue();
        break;
    case PLC_DATA_TEXT:
        buffer = (char *)palloc(response.stringvalue().size()+1);
        strncpy(buffer, response.stringvalue().c_str(), response.stringvalue().size()+1);
        break;
    case PLC_DATA_BYTEA:
        buffer = (char *)palloc(response.stringvalue().size()+sizeof(int32_t));
        *(int32_t *)buffer = response.stringvalue().size();
        memcpy(buffer+sizeof(int32_t), response.stringvalue().data(), response.stringvalue().size());
        break;
    default:
        plc_elog(ERROR, "unknown scalar type:%d", proc->result.type);
    }
       
    if (buffer) { 
        retresult = proc->result.infunc(buffer, &proc->result);
        pfree(buffer);
    }
    return retresult;
}

Datum PLContainerClient::getCallResponseAsDatum(const FunctionCallInfo fcinfo, plcProcInfo *proc, const ArrayData &response) {
    (void) fcinfo;
    (void) proc;
    (void) response;
    plc_elog(ERROR, "Array type of response data is not supported yet."); 
    return (Datum)0; 
}

Datum PLContainerClient::getCallResponseAsDatum(const FunctionCallInfo fcinfo, plcProcInfo *proc, const CompositeData &response) {
    (void) fcinfo;
    (void) proc;
    (void) response;
    plc_elog(ERROR, "Composite type of response data is not supported yet."); 
    return (Datum)0; 
}

Datum PLContainerClient::getCallResponseAsDatum(const FunctionCallInfo fcinfo, plcProcInfo *proc, const SetOfData &response) {
    (void) fcinfo;
    (void) proc;
    (void) response;
    plc_elog(ERROR, "SetOf type of response data is not supported yet."); 
    return (Datum)0; 
}

Datum PLContainerClient::GetCallResponseAsDatum(const FunctionCallInfo fcinfo, plcProcInfo *proc, const CallResponse &response) {
    Datum retresult = (Datum) 0;
    if (response.results_size() != 1) {
        plc_elog(ERROR, "currently, only one result set is supported");
        return retresult; 
    }

    const PlcValue &result = response.results(0);
    switch (result.type()) {
    case LOGICAL:
    case INT:
    case REAL:
    case TEXT:
    case BYTEA:
        return PLContainerClient::getCallResponseAsDatum(fcinfo, proc, result.scalarvalue());
    case ARRAY:
        return PLContainerClient::getCallResponseAsDatum(fcinfo, proc, result.arrayvalue());
    case COMPOSITE:
        return PLContainerClient::getCallResponseAsDatum(fcinfo, proc, result.compositevalue());
    case SETOF:
        return PLContainerClient::getCallResponseAsDatum(fcinfo, proc, result.setofvalue());
    default:
        plc_elog(ERROR, "uninvalid data type %d in ProtoToMessage", result.type());
    }
    
    return retresult;
}

std::string PLContainerClient::functionCallInfoToStr(const FunctionCallInfo fcinfo) {
    char result[1024];
    std::string argnull;
    for (int i=0;i<fcinfo->nargs;i++) {
        if (fcinfo->argnull[i]) {
            argnull += "1,";
        } else {
            argnull += "0,";
        }
    }
    argnull = argnull.substr(0, argnull.length()-1);

    snprintf(result, sizeof(result), 
                    "fcinfo->isnull:%d "
                    "fcinfo->nargs:%d "
                    "fcinfo->argnull:[%s]",
                    fcinfo->isnull,
                    fcinfo->nargs,
                    argnull.c_str());
    return std::string(result);
}

std::string PLContainerClient::procInfoToStr(const plcProcInfo *proc) {
    char result[4096];

    std::string argnames;
    std::string args;
    for (int i=0;i<proc->nargs;i++) {
        if (proc->argnames[i]) {
            argnames += std::string(proc->argnames[i]) + ",";
        } else {
            argnames += ",";
        }
        args += PLContainerClient::typeInfoToStr(&proc->args[i]) + ",";
    }
    argnames = argnames.substr(0, argnames.length()-1); 
    args = args.substr(0, args.length()-1); 
 
    snprintf(result, sizeof(result), 
                    "proname:%s "
                    "plname:%s "
                    "fn_readonly:%d "
                    "result:%s "
                    "src:%s "
                    "argnames:[%s] "
                    "args:[%s] "
                    "nargs:%d "
                    "name:%s "
                    "hasChanged:%d "
                    "retset:%d "
                    "funcOid:%d",
                    proc->proname,
                    proc->pyname,
                    proc->fn_readonly,
                    PLContainerClient::typeInfoToStr(&proc->result).c_str(),
                    proc->src,
                    argnames.c_str(),
                    args.c_str(),
                    proc->nargs,
                    proc->name,
                    proc->hasChanged,
                    proc->retset,
                    proc->funcOid);
    return std::string(result);
}

std::string PLContainerClient::typeInfoToStr(const plcTypeInfo *type) {
    char result[2048];

    snprintf(result, sizeof(result),
                    "type:%d "
                    "nSubTypes:%d "
                    "is_rowtype:%d "
                    "is_record:%d "
                    "attisdropped:%d "
                    "typ_relid:%d "
                    "typeName:%s ",
                    type->type,
                    type->nSubTypes,
                    type->is_rowtype,
                    type->is_record,
                    type->attisdropped,
                    type->typ_relid,
                    type->typeName);
    for (int i=0;i<type->nSubTypes;i++) {

        snprintf(result+strlen(result), sizeof(result) - strlen(result) - 2, "subtype_%d:%s", i,
                    PLContainerClient::typeInfoToStr(&type->subTypes[i]).c_str());
    }
    return std::string(result);
}

Datum plcontainer_function_handler(FunctionCallInfo fcinfo, plcProcInfo *proc, MemoryContext function_cxt) {
    Datum datumreturn;
    MemoryContext volatile      oldcontext = CurrentMemoryContext;
    FuncCallContext * volatile  funcctx =       NULL;
    bool     volatile               bFirstTimeCall = false;
    char *runtime_id;
    plcContext *ctx = NULL;
    CallRequest     request;
    CallResponse    response;
    PLContainerClient client;

    PG_TRY();
    {
        // 1. initialize function handler context, both return set or not
        plc_elog(DEBUG1, "fcinfo->flinfo->fn_retset: %d", fcinfo->flinfo->fn_retset);

        if (fcinfo->flinfo->fn_retset) {
            /* First Call setup */
            if (SRF_IS_FIRSTCALL())
            {
                funcctx = SRF_FIRSTCALL_INIT();
                bFirstTimeCall = true;

                plc_elog(DEBUG1, "The funcctx pointer returned by SRF_FIRSTCALL_INIT() is: %p", funcctx);
            }

            /* Every call setup */
            funcctx = SRF_PERCALL_SETUP();
            plc_elog(DEBUG1, "The funcctx pointer returned by SRF_PERCALL_SETUP() is: %p", funcctx);

            Assert(funcctx != NULL);
            /* SRF uses multi_call_memory_ctx context shared between function calls,
             * since EXPR etc. context will be cleared after one of the SRF calls.
             * Note that plpython doesn't need it, because it doesn't use palloc to store
             * the SRF result.
             */
            oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
            plc_elog(ERROR, "plcontainer_function_handler not support fn_retset");
        } else {
            oldcontext = MemoryContextSwitchTo(function_cxt);
        }

        if (!fcinfo->flinfo->fn_retset || bFirstTimeCall) {
            runtime_id = parse_container_meta(proc->src);
            ctx = get_container_context(runtime_id);
            /*
             * TODO will be reuse client channel if possible
             */
            client.Init(ctx);
            client.InitCallRequest(fcinfo, proc, R, request);
            
            plcContextBeginStage(ctx, "R_function_call", NULL);
            client.FunctionCall(request, response);
            plcContextEndStage(ctx, "R_function_call",
                    PLC_CONTEXT_STAGE_SUCCESS,
                    "[REQUEST]:%s, [RESPONSE]:%s", request.DebugString().c_str(), response.DebugString().c_str());

            plcContextLogging(LOG, ctx);
        }

        if (fcinfo->flinfo->fn_retset) {
            // process each call of retset 
            plc_elog(ERROR, "plcontainer_function_handler not support fn_retset");
        }

        /* Process the result message from client */
        datumreturn = client.GetCallResponseAsDatum(fcinfo, proc, response);
        MemoryContextSwitchTo(oldcontext);
    }
    PG_CATCH();
    {
        /*
         * If there was an error the iterator might have not been exhausted
         * yet. Set it to NULL so the next invocation of the function will
         * start the iteration again.
         */
        /*
        if (fcinfo->flinfo->fn_retset && funcctx->user_fctx != NULL) {
            funcctx->user_fctx = NULL;
        }
        */
        MemoryContextSwitchTo(oldcontext);
        PG_RE_THROW();
    }
    PG_END_TRY();
    
    if (fcinfo->flinfo->fn_retset) {
        plc_elog(ERROR, "plcontainer_function_handler not support fn_retset");
    }
    return datumreturn;
}

int get_new_container_from_coordinator(const char *runtime_id, plcContext *ctx) {
    (void) runtime_id;
    StartContainerRequest   request;
    StartContainerResponse  response;

    plcContextBeginStage(ctx, "request_coordinator_for_container", NULL);

    std::string server_addr = get_coordinator_address();
    PLCoordinatorClient     client(grpc::CreateChannel(
      "unix://"+server_addr, grpc::InsecureChannelCredentials()));

    request.set_runtime_id(runtime_id);
    request.set_qe_pid(getpid());
    request.set_session_id(gp_session_id);
    request.set_command_count(gp_command_count);
    client.StartContainer(request, response);

    plcContextEndStage(ctx, "request_coordinator_for_container",
                    response.status() == 0 ? PLC_CONTEXT_STAGE_SUCCESS : PLC_CONTEXT_STAGE_FAIL,
                    "[REQUEST]:%s, [RESPONSE]:%s", request.DebugString().c_str(), response.DebugString().c_str());

    if (response.status() != 0) {
        return -1;
    }
    ctx->service_address = plc_top_strdup(response.container_address().c_str());
    ctx->container_id = plc_top_strdup(response.container_id().c_str());
    return 0;
}

PLCoordinatorClient::PLCoordinatorClient(std::shared_ptr<grpc::Channel> channel) {
    this->stub_ = PLCoordinator::NewStub(channel);
}

void PLCoordinatorClient::StartContainer(const StartContainerRequest &request, StartContainerResponse &response) {
    grpc::ClientContext context;
    plc_elog(DEBUG1, "StartContainer request:%s", request.DebugString().c_str());
    grpc::Status status = stub_->StartContainer(&context, request, &response);
    if (!status.ok()) {
        plc_elog(ERROR, "StartContainer RPC failed., error:%s", status.error_message().c_str());
        response.set_status(1);
    }
    plc_elog(DEBUG1, "StartContainer response:%s", response.DebugString().c_str());
    plc_elog(DEBUG1, "StartContainer finished with status %d", status.error_code());
}

void PLCoordinatorClient::StopContainer(const StopContainerRequest &request, StopContainerResponse &response) {
    grpc::ClientContext context;
    plc_elog(DEBUG1, "StopContainer request:%s", request.DebugString().c_str());
    grpc::Status status = stub_->StopContainer(&context, request, &response);
    if (!status.ok()) {
        plc_elog(ERROR, "StopContainer RPC failed., error:%s", status.error_message().c_str());
        response.set_status(1);
    }
    plc_elog(DEBUG2, "StopContainer response:%s", response.DebugString().c_str());
    plc_elog(DEBUG1, "StopContainer finished with status %d", status.error_code());
}
