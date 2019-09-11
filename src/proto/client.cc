#include "client.h"

PLContainerClient::PLContainerClient(std::shared_ptr<grpc::Channel> channel) {
    this->stub_ = PLContainer::NewStub(channel);
}

PLContainerClient::PLContainerClient(const plcContext *ctx) {
    this->ctx = ctx;
    std::string uds(ctx->service_address);
    uds = "unix://" + uds;
    this->stub_ = PLContainer::NewStub(grpc::CreateChannel(uds, grpc::InsecureChannelCredentials()));
}

void PLContainerClient::FunctionCall(const CallRequest &request, CallResponse &response) {
    grpc::ClientContext context;
    plc_elog(WARNING, "function call request:%s", request.DebugString().c_str());
    grpc::Status status = stub_->FunctionCall(&context, request, &response);
    plc_elog(WARNING, "function call response:%s", response.DebugString().c_str());
    if (!status.ok()) {
        Error *error = response.mutable_exception();
        error->set_message(status.error_message());
    }
    plc_elog(DEBUG1, "PLContainerClient function call finished with status %d", status.error_code());
}

bool PLContainerClient::isSetOf(const plcTypeInfo *type) {
    int validSubTypes = 0;
    int validSubTypeIdx = -1;
    for (int i=0;i<type->nSubTypes;i++) {
        if (!type->subTypes[i].attisdropped) {
            validSubTypes += 1;
            validSubTypeIdx = i;
        }
    }

    if (type->type == PLC_DATA_ARRAY
        && validSubTypes == 1
        && type->subTypes[validSubTypeIdx].type == PLC_DATA_UDT) {
        return true;
    } else {
        return false;
    }
}


PlcDataType PLContainerClient::GetDataType(const plcTypeInfo *type) {
    PlcDataType ret;
    switch (type->type) {
    case PLC_DATA_INT1:
        ret = LOGICAL;
        break;
    case PLC_DATA_INT2:
    case PLC_DATA_INT4:
        ret = INT;
        break;
    case PLC_DATA_INT8:
    case PLC_DATA_FLOAT4:
    case PLC_DATA_FLOAT8:
        ret = REAL;
        break;
    case PLC_DATA_TEXT:
        ret = TEXT;
        break;
    case PLC_DATA_BYTEA:
        ret = BYTEA;
        break;
    case PLC_DATA_ARRAY:
        ret = ARRAY;
        break;
    case PLC_DATA_UDT:
        ret = COMPOSITE;
        break;
    default:
        plc_elog(ERROR, "unknown data type %d of plcType", type->type);
    }

    if (this->isSetOf(type)) {
        ret = SETOF;
    }
 
    return ret;
}
void PLContainerClient::InitCallRequestArgument(const FunctionCallInfo fcinfo, const plcProcInfo *proc, int argIdx, ScalarData &arg) {
    arg.set_type(this->GetDataType(&proc->args[argIdx]));
    arg.set_name(proc->argnames[argIdx]);
    if (fcinfo->argnull[argIdx]) {
        arg.set_isnull(true);
    } else {
        arg.set_isnull(false);
        char *value = proc->args[argIdx].outfunc(fcinfo->arg[argIdx], &proc->args[argIdx]);
        switch (proc->args[argIdx].type) {
        case PLC_DATA_INT1:
            arg.set_logicalvalue(*(int8_t *)value); 
            break;
        case PLC_DATA_INT2:
            arg.set_intvalue(*(int16_t *)value); 
            break;
        case PLC_DATA_INT4:
            arg.set_intvalue(*(int32_t *)value);
            break; 
        case PLC_DATA_FLOAT4:
            arg.set_realvalue(*(float *)value);
            break;
        case PLC_DATA_INT8:
            arg.set_realvalue(*(int64_t *)value);
            break;
        case PLC_DATA_FLOAT8:
            arg.set_realvalue(*(double *)value);
            break;
        case PLC_DATA_TEXT:
            arg.set_stringvalue(value);
            break;
        case PLC_DATA_BYTEA:
            // int32_length + data
            arg.set_byteavalue(value+sizeof(int32_t), *((int32_t *)(value)));
            break;
        default:
            plc_elog(ERROR, "invalid data type %d in argument %d of call request", proc->args[argIdx].type, argIdx);
        }
    }
}

void PLContainerClient::InitCallRequest(const FunctionCallInfo fcinfo, const plcProcInfo *proc, PlcRuntimeType type, CallRequest &request) {
    request.set_runtimetype(type);
    request.set_objectid(proc->funcOid);
    request.set_haschanged(proc->hasChanged);
    request.mutable_proc()->set_src(proc->src);
    request.mutable_proc()->set_name(proc->name);
    request.set_loglevel(log_min_messages);
    request.set_rettype(this->GetDataType(&proc->result));
    if (GetDatabaseEncoding() == PG_SQL_ASCII) {
        request.set_serverenc("ascii");
    } else {
        request.set_serverenc(GetDatabaseEncodingName());            
    }

    for (int i=0;i<proc->nargs;i++) {
        PlcValue *arg = request.add_args();
        arg->set_type(this->GetDataType(&proc->args[i]));
        arg->set_name(proc->argnames[i]);

        switch (arg->type()) {
        case LOGICAL:
        case INT:
        case REAL:
        case TEXT:
        case BYTEA:
            PLContainerClient::InitCallRequestArgument(fcinfo, proc, i, *arg->mutable_scalarvalue());
            break;
        case ARRAY:
        case COMPOSITE:
        case SETOF:
        default:
            plc_elog(ERROR, "invalid data type %d in argument %d in InitCallRequest", arg->type(), i);
        }
    }
}
Datum PLContainerClient::GetCallResponseAsDatum(const FunctionCallInfo fcinfo, plcProcInfo *proc, const ScalarData &response) {
    Datum retresult;

    if (response.isnull()) {
        return (Datum)0;
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

Datum PLContainerClient::GetCallResponseAsDatum(const FunctionCallInfo fcinfo, plcProcInfo *proc, const ArrayData &response) {
    (void) fcinfo;
    (void) proc;
    (void) response;
    plc_elog(ERROR, "Array type of response data is not supported yet."); 
    return (Datum)0; 
}

Datum PLContainerClient::GetCallResponseAsDatum(const FunctionCallInfo fcinfo, plcProcInfo *proc, const CompositeData &response) {
    (void) fcinfo;
    (void) proc;
    (void) response;
    plc_elog(ERROR, "Composite type of response data is not supported yet."); 
    return (Datum)0; 
}

Datum PLContainerClient::GetCallResponseAsDatum(const FunctionCallInfo fcinfo, plcProcInfo *proc, const SetOfData &response) {
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
        return this->GetCallResponseAsDatum(fcinfo, proc, result.scalarvalue());
    case ARRAY:
        return this->GetCallResponseAsDatum(fcinfo, proc, result.arrayvalue());
    case COMPOSITE:
        return this->GetCallResponseAsDatum(fcinfo, proc, result.compositevalue());
    case SETOF:
        return this->GetCallResponseAsDatum(fcinfo, proc, result.setofvalue());
    default:
        plc_elog(ERROR, "uninvalid data type %d in ProtoToMessage", result.type());
    }
    
    return retresult;
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
    PLContainerClient client(
        grpc::CreateChannel("unix:///tmp/plcontainer.socket", grpc::InsecureChannelCredentials())
    );

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
            (void) ctx; // TODO init client 
            client.InitCallRequest(fcinfo, proc, R, request);
            client.FunctionCall(request, response);
        }

        if (fcinfo->flinfo->fn_retset) {
            // process each call of retset 
            plc_elog(ERROR, "plcontainer_function_handler not support fn_retset");
        }

        /* Process the result message from client */
        //datumreturn = plcontainer_process_result(fcinfo, proc, presult);
        //presult->resrow += 1;
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
