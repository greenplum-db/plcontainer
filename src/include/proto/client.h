#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <thread>

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include "plcontainer.grpc.pb.h"

#include "interface.h"

extern "C"
{
#include "postgres.h"
#include "mb/pg_wchar.h"
#include "common/comm_dummy.h"
#include "plc/containers.h"
}

using namespace plcontainer;


class PLContainerClient {
public:
    PLContainerClient(std::shared_ptr<grpc::Channel> channel);
    PLContainerClient(const plcContext *ctx);

    void FunctionCall(const CallRequest &request, CallResponse &response);
    
    void InitCallRequest(const FunctionCallInfo fcinfo, const plcProcInfo *proc, PlcRuntimeType type, CallRequest &request);
    void InitCallRequestArgument(const FunctionCallInfo fcinfo, const plcProcInfo *proc, int argIdx, ScalarData &arg);
    
    Datum GetCallResponseAsDatum(const FunctionCallInfo fcinfo, plcProcInfo *proc, const CallResponse &response);
    Datum GetCallResponseAsDatum(const FunctionCallInfo fcinfo, plcProcInfo *proc, const ScalarData &response);
    Datum GetCallResponseAsDatum(const FunctionCallInfo fcinfo, plcProcInfo *proc, const ArrayData &response);
    Datum GetCallResponseAsDatum(const FunctionCallInfo fcinfo, plcProcInfo *proc, const CompositeData &response);
    Datum GetCallResponseAsDatum(const FunctionCallInfo fcinfo, plcProcInfo *proc, const SetOfData &response);

private:
    std::unique_ptr<PLContainer::Stub> stub_;
    const plcContext  *ctx;

    PlcDataType GetDataType(const plcTypeInfo *type);
    bool isSetOf(const plcTypeInfo *type);
};

#endif
