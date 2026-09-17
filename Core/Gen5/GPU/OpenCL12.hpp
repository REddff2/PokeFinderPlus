#pragma once
// Minimal ABI declarations used by the isolated experiment. No OpenCL SDK needed.
#include "Protocol.hpp"
#include <stdexcept>
#include <vector>
namespace CL {
using I=int32_t;using U=uint32_t;using Q=uint64_t;using S=size_t;using H=void*;
#define CL_FN(name,result,...) using name##Type=result (__stdcall *)(__VA_ARGS__);name##Type name=nullptr
struct API {
    HMODULE library=nullptr;
    CL_FN(GetPlatformIDs,I,U,H*,U*);
    CL_FN(GetDeviceIDs,I,H,Q,U,H*,U*);
    CL_FN(GetDeviceInfo,I,H,U,S,void*,S*);
    CL_FN(CreateContext,H,const intptr_t*,U,const H*,void*,void*,I*);
    CL_FN(CreateCommandQueue,H,H,H,Q,I*);
    CL_FN(CreateBuffer,H,H,Q,S,void*,I*);
    CL_FN(CreateProgramWithSource,H,H,U,const char**,const S*,I*);
    CL_FN(BuildProgram,I,H,U,const H*,const char*,void*,void*);
    CL_FN(GetProgramBuildInfo,I,H,H,U,S,void*,S*);
    CL_FN(CreateKernel,H,H,const char*,I*);
    CL_FN(SetKernelArg,I,H,U,S,const void*);
    CL_FN(EnqueueWriteBuffer,I,H,H,U,S,S,const void*,U,const H*,H*);
    CL_FN(EnqueueReadBuffer,I,H,H,U,S,S,void*,U,const H*,H*);
    CL_FN(EnqueueNDRangeKernel,I,H,H,U,const S*,const S*,const S*,U,const H*,H*);
    CL_FN(Flush,I,H);
    CL_FN(WaitForEvents,I,U,const H*);
    CL_FN(GetEventProfilingInfo,I,H,U,S,void*,S*);
    CL_FN(ReleaseEvent,I,H);CL_FN(ReleaseMemObject,I,H);CL_FN(ReleaseKernel,I,H);
    CL_FN(ReleaseProgram,I,H);CL_FN(ReleaseCommandQueue,I,H);CL_FN(ReleaseContext,I,H);
    API(){
        library=LoadLibraryExW(L"OpenCL.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!library)throw std::runtime_error("No system OpenCL loader; use CPU");
#define LOAD(n) n=reinterpret_cast<n##Type>(GetProcAddress(library,"cl" #n));if(!n)throw std::runtime_error("Missing cl" #n)
        LOAD(GetPlatformIDs);LOAD(GetDeviceIDs);LOAD(GetDeviceInfo);LOAD(CreateContext);LOAD(CreateCommandQueue);
        LOAD(CreateBuffer);LOAD(CreateProgramWithSource);LOAD(BuildProgram);LOAD(GetProgramBuildInfo);LOAD(CreateKernel);
        LOAD(SetKernelArg);LOAD(EnqueueWriteBuffer);LOAD(EnqueueReadBuffer);LOAD(EnqueueNDRangeKernel);LOAD(Flush);
        LOAD(WaitForEvents);LOAD(GetEventProfilingInfo);LOAD(ReleaseEvent);LOAD(ReleaseMemObject);LOAD(ReleaseKernel);
        LOAD(ReleaseProgram);LOAD(ReleaseCommandQueue);LOAD(ReleaseContext);
#undef LOAD
    }
    ~API(){if(library)FreeLibrary(library);}
    API(const API&)=delete;
};
#undef CL_FN
inline void check(I status){if(status)throw std::runtime_error("OpenCL error "+std::to_string(status));}
template<class T>T info(API &a,H d,U key){T out{};check(a.GetDeviceInfo(d,key,sizeof(T),&out,nullptr));return out;}
inline std::string info(API &a,H d,U key){S n=0;check(a.GetDeviceInfo(d,key,0,nullptr,&n));std::string out(n,'\0');check(a.GetDeviceInfo(d,key,n,out.data(),nullptr));if(!out.empty()&&out.back()==0)out.pop_back();return out;}
inline double seconds(API &a,H e){Q start=0,end=0;check(a.GetEventProfilingInfo(e,0x1282,8,&start,nullptr));check(a.GetEventProfilingInfo(e,0x1283,8,&end,nullptr));return (end-start)*1e-9;}
}
