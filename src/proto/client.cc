#include "client.h"
#include "proto_utils.h"

PLContainerClient *PLContainerClient::client = NULL; 

PLContainerClient::PLContainerClient() {
    this->stub_ = NULL;
    this->ctx = NULL;
}

PLContainerClient *PLContainerClient::GetPLContainerClient() {
    if (client == NULL) {
        client = new PLContainerClient;
    }
    return client;
}

std::string get_pl4k_service_address() {
    return std::string(plcontainer_service_address);
}

void PLContainerClient::Init(const plcContext *ctx) {
    this->ctx = ctx;

    if (ctx->is_new_ctx) {
        std::string uds(ctx->service_address);
        this->stub_ = PLContainer::NewStub(grpc::CreateChannel(uds, grpc::InsecureChannelCredentials()));
    }
}

void PLContainerClient::FunctionCall(const CallRequest &request, CallResponse &response) {
    std::chrono::system_clock::time_point deadline;
    grpc::Status status; 
    while (true) {
        CHECK_FOR_INTERRUPTS();

        grpc::ClientContext context;
        context.set_wait_for_ready(true);

        if (::plc_client_timeout != -1) {
            deadline = std::chrono::system_clock::now() + std::chrono::seconds(::plc_client_timeout);
            context.set_deadline(deadline);
        }

        plc_elog(DEBUG1, "function call request:%s", request.DebugString().c_str());
        status = stub_->FunctionCall(&context, request, &response);
        plc_elog(DEBUG1, "function call response:%s", response.DebugString().c_str());
        if (!status.ok()) {
            if (status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED) {
                plc_elog(LOG, "plcontainer functioncall timeout");
            } else {
                plc_elog(ERROR, "plcontainer function call RPC failed., error:%s", status.error_message().c_str());
            }
            continue;
        } else if (response.has_exception()) {
            Error *error = response.mutable_exception();
            plc_elog(ERROR, "plcontainer function call failed. error:%s stacktrace:%s", error->message().c_str(), error->stacktrace().c_str());
            break;
        } else {
            // success
            break;
        }
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
}

void PLContainerClient::initCallRequestArgument(const FunctionCallInfo fcinfo, const plcProcInfo *proc, int argIdx, SetOfData &arg) {
    if (proc->argnames[argIdx]) {
        arg.set_name(proc->argnames[argIdx]);
    } else {
        arg.set_name("");
    }

    if (!fcinfo->argnull[argIdx]) {
        char *argvalue = proc->args[argIdx].outfunc(fcinfo->arg[argIdx], &proc->args[argIdx]);
        int size = *(int *)argvalue;
        if (!arg.ParseFromArray(argvalue+sizeof(int), size)) {
            plc_elog(ERROR, "setof outfunc parse failed");
        }
    }

    plc_elog(DEBUG1, "setof data parse result:%s", arg.DebugString().c_str());
}

void PLContainerClient::InitCallRequest(const FunctionCallInfo fcinfo, PlcRuntimeType type, CallRequest &request) {
    const InlineCodeBlock * const icb = (InlineCodeBlock *)PG_GETARG_POINTER(0);
    plc_elog(DEBUG1, "plcontainer inline function :%s, source_text:%s",
            PLContainerClient::functionCallInfoToStr(fcinfo).c_str(),
            icb->source_text);

    request.set_runtimetype(type);
    request.set_haschanged(1);
    request.mutable_proc()->set_src(icb->source_text);
    request.set_loglevel(log_min_messages);
    request.mutable_rettype()->set_type(VOID);
    if (GetDatabaseEncoding() == PG_SQL_ASCII) {
        request.set_serverenc("ascii");
    } else {
        request.set_serverenc(GetDatabaseEncodingName());
    }
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
    PLContainerClient::setFunctionReturnType(request.mutable_rettype(), &proc->result, fcinfo->flinfo->fn_retset);
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
        case VOID:
            // do nothing for void type
            break;
        default:
            plc_elog(ERROR, "invalid data type %d in argument %d in InitCallRequest", arg->type(), i);
        }
    }
}

void PLContainerClient::setFunctionReturnType(::plcontainer::ReturnType* rettype, const plcTypeInfo *type, bool setof) {
    PlcDataType rt = PLContainerProtoUtils::GetDataType(type);
    if (setof) {
        rettype->set_type(SETOF); 
    } else { 
        rettype->set_type(rt);
    }
    if (rt == ARRAY || rt == COMPOSITE || rt == SETOF) {
        // subtypes return type
        const plcTypeInfo *t = (rt == SETOF ? &type->subTypes[0] : type);
        for (int i=0; i<t->nSubTypes; i++) {
            rettype->add_subtypes(PLContainerProtoUtils::GetDataType(&t->subTypes[i]));
        }
    } else if (setof) {
        // setof scalar
        rettype->add_subtypes(PLContainerProtoUtils::GetDataType(type)); 
    } else {
        // scalar return type
    }
}

Datum PLContainerClient::getCallResponseAsDatum(const FunctionCallInfo fcinfo, plcProcInfo *proc, const ScalarData &response) {
    Datum retresult = (Datum)0;

    if (response.isnull()) {
        return retresult;
    }

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
        buffer = (char *)palloc(response.byteavalue().size()+sizeof(int32_t));
        *(int32_t *)buffer = response.byteavalue().size();
        memcpy(buffer+sizeof(int32_t), response.byteavalue().data(), response.byteavalue().size());
        break;
    default:
        plc_elog(ERROR, "unknown scalar type:%d", proc->result.type);
    }
       
    if (buffer) { 
        fcinfo->isnull = false;
        retresult = proc->result.infunc(buffer, &proc->result);
        pfree(buffer);
    }
    return retresult;
}

Datum PLContainerClient::getCallResponseAsDatum(const FunctionCallInfo fcinfo, plcProcInfo *proc, const ArrayData &response) {
    if (response.values_size() == 0) {
        return (Datum)0;
    } else {
        fcinfo->isnull = false;
        return proc->result.infunc((char *)&response, &proc->result);
    }
}

Datum PLContainerClient::getCallResponseAsDatum(const FunctionCallInfo fcinfo, plcProcInfo *proc, const CompositeData &response) {
    if (response.values_size() == 0) {
        return (Datum)0;
    } else {
        fcinfo->isnull = false;
        return proc->result.infunc((char *)&response, &proc->result);
    }
}

Datum PLContainerClient::getCallResponseAsDatum(const FunctionCallInfo fcinfo, plcProcInfo *proc, const SetOfData &response, int row_index) {
    if (response.rowvalues_size() == 0) {
        return (Datum)0;
    } else {
        fcinfo->isnull = false;
        if (proc->result.type == PLC_DATA_ARRAY) {
            // array of UDT
            return proc->result.infunc((char *)&response, &proc->result);
        } else {
            // setof UDT
            return proc->result.infunc((char *)&response.rowvalues(row_index), &proc->result);
        }
    }
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
        return PLContainerClient::getCallResponseAsDatum(fcinfo, proc, result.setofvalue(), response.result_rows());
    case VOID:
        // return void will do nothing 
        break;
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

    char flinfo[512];
    snprintf(flinfo, sizeof(flinfo),
                    "flinfo->fn_addr:%p "
                    "flinfo->fn_oid:%d "
                    "flinfo->fn_nargs:%d "
                    "flinfo->fn_strict:%d "
                    "flinfo->fn_retset:%d "
                    "flinfo->fn_stats:%d",
                    fcinfo->flinfo->fn_addr,
                    fcinfo->flinfo->fn_oid,
                    fcinfo->flinfo->fn_nargs,
                    fcinfo->flinfo->fn_strict,
                    fcinfo->flinfo->fn_retset,
                    fcinfo->flinfo->fn_stats); 

    snprintf(result, sizeof(result), 
                    "fcinfo->isnull:%d "
                    "fcinfo->nargs:%d "
                    "fcinfo->argnull:[%s] "
                    "fcinfo->flinfo:%s",
                    fcinfo->isnull,
                    fcinfo->nargs,
                    argnull.c_str(),
                    flinfo);
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
    CallResponse    * volatile  response = NULL;
    PLContainerClient * volatile client = NULL;

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
        } else {
            oldcontext = MemoryContextSwitchTo(function_cxt);
        }

        if (!fcinfo->flinfo->fn_retset || bFirstTimeCall) {
            runtime_id = parse_container_meta(proc->src);
            ctx = get_container_context(runtime_id);
            /*
             * TODO will be reuse client channel if possible
             */
            client = PLContainerClient::GetPLContainerClient();
            client->Init(ctx);
            client->InitCallRequest(fcinfo, proc, R, request);
            response = new CallResponse;
            response->set_result_rows(0);
 
            plcContextBeginStage(ctx, "R_function_call", NULL);
            client->FunctionCall(request, *response);
            plcContextEndStage(ctx, "R_function_call",
                    PLC_CONTEXT_STAGE_SUCCESS,
                    "[REQUEST]:%s, [RESPONSE]:%s", request.DebugString().c_str(), response->DebugString().c_str());

            plcContextLogging(LOG, ctx);
            bFirstTimeCall = false;
        }

        if (fcinfo->flinfo->fn_retset) {
            ReturnSetInfo *rsi = (ReturnSetInfo *) fcinfo->resultinfo;

            if (funcctx->user_fctx == NULL) {
                plc_elog(DEBUG1, "first time call, preparing the result set...");

                /* first time -- do checks and setup */
                if (!rsi || !IsA(rsi, ReturnSetInfo)
                        || (rsi->allowedModes & SFRM_ValuePerCall) == 0) {
                    ereport(ERROR,
                            (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg(
                                    "unsupported set function return mode"), errdetail(
                                    "PL/Python set-returning functions only support returning only value per call.")));
                }
                rsi->returnMode = SFRM_ValuePerCall;

                funcctx->user_fctx = (void *) response;

                if (funcctx->user_fctx == NULL)
                    ereport(ERROR,
                            (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg(
                                    "returned object cannot be iterated"), errdetail(
                                    "PL/Python set-returning functions must return an iterable object.")));
            }

            response = (CallResponse *) funcctx->user_fctx;
            if (response->results_size() > 0
                && response->result_rows() < response->results(0).setofvalue().rowvalues_size()) {
                rsi->isDone = ExprMultipleResult;
            } else {
                rsi->isDone = ExprEndResult;
            }

            if (rsi->isDone == ExprEndResult) {
                MemoryContextSwitchTo(oldcontext);

                delete response;
                funcctx->user_fctx = NULL;

                SRF_RETURN_DONE(funcctx);
            }
        }

        /* Process the result message from client */
        datumreturn = client->GetCallResponseAsDatum(fcinfo, proc, *response);
        response->set_result_rows(response->result_rows() + 1);
        MemoryContextSwitchTo(oldcontext);
    }
    PG_CATCH();
    {
        /*
         * If there was an error the iterator might have not been exhausted
         * yet. Set it to NULL so the next invocation of the function will
         * start the iteration again.
         */
        if (fcinfo->flinfo->fn_retset && funcctx->user_fctx != NULL) {
            funcctx->user_fctx = NULL;
        }

        if (response) {
            delete response;
        }

        MemoryContextSwitchTo(oldcontext);
        PG_RE_THROW();
    }
    PG_END_TRY();
    
    if (fcinfo->flinfo->fn_retset) {
        SRF_RETURN_NEXT(funcctx, datumreturn);
    } else {
        delete response;
        return datumreturn;
    }
}

void plcontainer_inline_function_handler(FunctionCallInfo fcinfo, MemoryContext function_cxt) {
    const InlineCodeBlock * const icb = (InlineCodeBlock *)PG_GETARG_POINTER(0);
    MemoryContext volatile      oldcontext = CurrentMemoryContext;
    char *runtime_id;
    plcContext *ctx = NULL;
    CallRequest     request;
    CallResponse    response;
    PLContainerClient * client = NULL;
    
    PG_TRY();
    {
        if (fcinfo->flinfo->fn_retset) {
            plc_elog(ERROR, "plcontainer inline function could not return setof"); 
        }

        if (icb == NULL || icb->source_text == NULL) {
            plc_elog(ERROR, "plcontainer inline function param error");
        } 

        oldcontext = MemoryContextSwitchTo(function_cxt);
        runtime_id = parse_container_meta(icb->source_text);
        ctx = get_container_context(runtime_id);

        client = PLContainerClient::GetPLContainerClient(); 
        client->Init(ctx);
        client->InitCallRequest(fcinfo, R, request);
        
        plcContextBeginStage(ctx, "R_inline_function", NULL);
        client->FunctionCall(request, response);
        plcContextEndStage(ctx, "R_inline_function",
                    PLC_CONTEXT_STAGE_SUCCESS,
                    "[REQUEST]:%s, [RESPONSE]:%s", request.DebugString().c_str(), response.DebugString().c_str());

        plcContextLogging(LOG, ctx);
        MemoryContextSwitchTo(oldcontext);
    }
    PG_CATCH();
    {
        MemoryContextSwitchTo(oldcontext);
        PG_RE_THROW();
    }
    PG_END_TRY();
}

int get_new_container_from_coordinator(const char *runtime_id, plcContext *ctx) {
    (void) runtime_id;
    StartContainerRequest   request;
    StartContainerResponse  response;
    const char *username;
    plcContextBeginStage(ctx, "request_coordinator_for_container", NULL);
    int dbid;
#ifndef PLC_PG
	dbid = (int)GpIdentity.dbid;
#endif
    username = GetUserNameFromId(GetUserId());
    request.set_runtime_id(runtime_id);
    request.set_qe_pid(getpid());
    request.set_session_id(gp_session_id);
    request.set_command_count(gp_command_count);
    request.set_ownername(username);
    request.set_dbid(dbid);

    if (!is_plcontainer_for_k8s) {
        std::string server_addr = get_coordinator_address();
        PLCoordinatorClient     client(grpc::CreateChannel(
            "unix://"+server_addr, grpc::InsecureChannelCredentials()));
        client.StartContainer(request, response);
        plcContextEndStage(ctx, "request_coordinator_for_container",
                    response.status() == 0 ? PLC_CONTEXT_STAGE_SUCCESS : PLC_CONTEXT_STAGE_FAIL,
                    "[REQUEST]:%s, [RESPONSE]:%s", request.DebugString().c_str(), response.DebugString().c_str());

        if (response.status() != 0) {
            return -1;
        }
        ctx->service_address = plc_top_strdup(("unix://"+response.container_address()).c_str());
        ctx->container_id = plc_top_strdup(response.container_id().c_str());
    } else {
        std::string server_addr = get_pl4k_service_address();
        ctx->service_address = plc_top_strdup(server_addr.c_str());
        ctx->container_id = plc_top_strdup("");
    }

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
        response.set_status(1);
        plc_elog(ERROR, "StartContainer RPC failed., error:%s", status.error_message().c_str());
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
