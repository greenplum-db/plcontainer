#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <sys/types.h>
#include <unistd.h>

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
#include "plc/plc_coordinator.h"
#include "cdb/cdbvars.h"
}

using namespace plcontainer;


class PLContainerClient {
public:
    static PLContainerClient *GetPLContainerClient();

    void Init(const plcContext *ctx);

    void FunctionCall(const CallRequest &request, CallResponse &response);

    static void InitCallRequest(const FunctionCallInfo fcinfo, PlcRuntimeType type, CallRequest &request);
    static void InitCallRequest(const FunctionCallInfo fcinfo, const plcProcInfo *proc, PlcRuntimeType type, CallRequest &request);

    static Datum GetCallResponseAsDatum(const FunctionCallInfo fcinfo, plcProcInfo *proc, const CallResponse &response);

private:
    PLContainerClient();

    static void initCallRequestArgument(const FunctionCallInfo fcinfo, const plcProcInfo *proc, int argIdx, ScalarData &arg);
    static void initCallRequestArgument(const FunctionCallInfo fcinfo, const plcProcInfo *proc, int argIdx, ArrayData &arg);
    static void initCallRequestArgument(const FunctionCallInfo fcinfo, const plcProcInfo *proc, int argIdx, CompositeData &arg);
    static void initCallRequestArgument(const FunctionCallInfo fcinfo, const plcProcInfo *proc, int argIdx, SetOfData &arg);

    static Datum getCallResponseAsDatum(const FunctionCallInfo fcinfo, plcProcInfo *proc, const ScalarData &response);
    static Datum getCallResponseAsDatum(const FunctionCallInfo fcinfo, plcProcInfo *proc, const ArrayData &response);
    static Datum getCallResponseAsDatum(const FunctionCallInfo fcinfo, plcProcInfo *proc, const CompositeData &response);
    static Datum getCallResponseAsDatum(const FunctionCallInfo fcinfo, plcProcInfo *proc, const SetOfData &response, int row_index);

    static void setFunctionReturnType(::plcontainer::ReturnType* rettype, const plcTypeInfo *type, bool setof);

    static std::string functionCallInfoToStr(const FunctionCallInfo fcinfo);
    static std::string procInfoToStr(const plcProcInfo *proc);
    static std::string typeInfoToStr(const plcTypeInfo *type);

private:
    static PLContainerClient *client;

    std::unique_ptr<PLContainer::Stub> stub_;
    const plcContext  *ctx;
};

class PLCoordinatorClient {
public:
    PLCoordinatorClient(std::shared_ptr<grpc::Channel> channel);

    void StartContainer(const StartContainerRequest &request, StartContainerResponse &response);
    void StopContainer(const StopContainerRequest &request, StopContainerResponse &response);

private:
    std::unique_ptr<PLCoordinator::Stub> stub_;
};

#endif
