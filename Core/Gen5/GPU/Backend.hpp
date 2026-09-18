#pragma once
#include <stdexcept>
#include "Protocol.hpp"
#include "IVBounds.hpp"
#include "IVPlan.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>
namespace GpuWild {
inline double now(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
class Backend {
    HANDLE mapping=nullptr,process=nullptr,job=nullptr,ready=nullptr,request[Slots]{},done[Slots]{};
    Shared *shared=nullptr;
    std::mutex mutex;std::condition_variable condition;bool occupied[Slots]{};
    bool enabled=false;unsigned slots;DWORD timeout;
    std::string failure;
    uint64_t batches=0,candidates=0,survivors=0,fallbackBatches=0;
    double upload=0,kernel=0,download=0,helperWall=0,wait=0,copy=0;
    nlohmann::json device;
    double initialize=0;
    static DWORD waitFor(HANDLE *events,DWORD timeout,const std::atomic<bool>* cancelled) {
        const auto start=GetTickCount64();
        while(true) {
            if(cancelled && cancelled->load(std::memory_order_relaxed))return WAIT_TIMEOUT;
            auto status=WaitForMultipleObjects(2,events,FALSE,25);
            if(status!=WAIT_TIMEOUT || GetTickCount64()-start>=timeout)return status;
        }
    }
    void disable(const std::string &why){
        std::lock_guard lock(mutex);
        if(enabled){enabled=false;failure=why;if(process)TerminateProcess(process,92);condition.notify_all();
            std::cerr<<"GPU_DISABLED CPU replay: "<<why<<std::endl;}
    }
public:
    Backend(int selected,const std::wstring &helper,const std::wstring &kernelPath,const std::wstring &fault=L"",unsigned count=2,DWORD timeoutMs=15000,const std::atomic<bool>* cancelled=nullptr):slots(count),timeout(timeoutMs){
        const auto start=now();
        try {
            wincheck(count>=1&&count<=Slots,"Slot count outside bounds");
            static std::atomic<unsigned> serial{0};
            auto base=L"Local\\PokeFinderGen5Gpu-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(serial++);
            mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(Shared),base.c_str());wincheck(mapping!=nullptr,"Create mapping");
            shared=static_cast<Shared*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared)));wincheck(shared!=nullptr,"Map view");
            ready=CreateEventW(nullptr,FALSE,FALSE,named(base,L"ready").c_str());wincheck(ready!=nullptr,"Create ready event");
            for(unsigned i=0;i<slots;++i){
                request[i]=CreateEventW(nullptr,FALSE,FALSE,named(base,L"request",i).c_str());
                done[i]=CreateEventW(nullptr,FALSE,FALSE,named(base,L"done",i).c_str());wincheck(request[i]&&done[i],"Create slot events");
            }
            std::wstring command=L"\""+helper+L"\" \""+base+L"\" "+std::to_wstring(selected)+L" \""+kernelPath+L"\" \""+fault+L"\" "+std::to_wstring(slots)+L" "+std::to_wstring(ProtocolVersion);
            STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESHOWWINDOW;si.wShowWindow=SW_HIDE;PROCESS_INFORMATION pi{};
            job=CreateJobObjectW(nullptr,nullptr);wincheck(job!=nullptr,"Create helper lifetime job");
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit{};limit.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            wincheck(SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limit,sizeof(limit)),"Set helper lifetime limit");
            wincheck(CreateProcessW(helper.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,nullptr,&si,&pi),"Create isolated GPU helper");
            process=pi.hProcess;
            if(!AssignProcessToJobObject(job,process)){CloseHandle(pi.hThread);throw std::runtime_error("Assign helper lifetime job");}
            ResumeThread(pi.hThread);CloseHandle(pi.hThread);
            HANDLE events[]={ready,process};auto status=waitFor(events,30000,cancelled);
            wincheck(status==WAIT_OBJECT_0&&shared->ready==1,"GPU initialization failed or timed out");
            device=nlohmann::json::parse(shared->info);enabled=true;
        }catch(const std::exception&e){failure=e.what();if(process)TerminateProcess(process,92);}
        initialize=now()-start;
    }
    ~Backend(){if(process){TerminateProcess(process,0);WaitForSingleObject(process,2000);CloseHandle(process);}if(job)CloseHandle(job);if(shared)UnmapViewOfFile(shared);if(mapping)CloseHandle(mapping);if(ready)CloseHandle(ready);for(unsigned i=0;i<Slots;++i){if(request[i])CloseHandle(request[i]);if(done[i])CloseHandle(done[i]);}}
    Backend(const Backend&)=delete;
    bool available(){std::lock_guard lock(mutex);return enabled;}
    bool independentIVUseful() const { return !device.empty() && !device["device"]["unified_memory"].get<bool>(); }
    // nullopt means replay the ENTIRE uncommitted input using current CPU code.
    // No results are published until one whole batch has completed and validated.
    std::optional<std::vector<uint32_t>> execute(const std::vector<uint64_t>&seeds,const IVBounds &bounds,bool inspect=false,const std::atomic<bool>* cancelled=nullptr,std::optional<IVPlan> plan={}){
        if(plan && !plan->valid())return std::nullopt;
        if(!bounds.valid())return std::nullopt;
        if(seeds.empty())return std::vector<uint32_t>{};
        if(seeds.size()>Capacity)throw std::runtime_error("Batch exceeds transport capacity");
        unsigned slot=0;double began=now();
        {std::unique_lock lock(mutex);
            auto ready=[&]{if(!enabled)return true;for(unsigned i=0;i<slots;++i)if(!occupied[i])return true;return false;};
            while(!ready()) {
                if(cancelled && cancelled->load(std::memory_order_relaxed)){++fallbackBatches;return std::nullopt;}
                condition.wait_for(lock,std::chrono::milliseconds(25));
            }
            if(!enabled || (cancelled && cancelled->load(std::memory_order_relaxed))){++fallbackBatches;return std::nullopt;}
            for(;slot<slots&&occupied[slot];++slot){}occupied[slot]=true;wait+=now()-began;}
        auto &s=shared->slots[slot];double copying=now();
        memcpy(s.seeds,seeds.data(),seeds.size()*8);s.n=uint32_t(seeds.size());s.inspect=inspect;s.count=0;
        s.ivMin=bounds.packedMin();s.ivMax=bounds.packedMax();
        s.mode=plan?(plan->cacheFrames?2:1):0;s.ivOffset=plan?plan->offset:0;s.ivRoamer=plan?(plan->cacheFrames?plan->cacheFrames:plan->roamer):0;
        double copyElapsed=now()-copying;
        SetEvent(request[slot]);HANDLE events[]={done[slot],process};auto status=waitFor(events,timeout,cancelled);
        std::optional<std::vector<uint32_t>> output;
        if(status!=WAIT_OBJECT_0)disable(status==WAIT_TIMEOUT?"batch timeout":"helper exited / device failure");
        else if(s.count>(inspect?seeds.size()*8:seeds.size()) || (inspect && s.count!=seeds.size()*8))disable("invalid output count");
        else try {
            output.emplace(s.output,s.output+s.count);
            if(!inspect){
                std::sort(output->begin(),output->end());
                if((!output->empty()&&output->back()>=seeds.size())||std::adjacent_find(output->begin(),output->end())!=output->end()){
                    output.reset();disable("invalid or duplicate candidate index");
                }
            }
        }catch(const std::bad_alloc&){output.reset();disable("host output allocation failed");}
        {std::lock_guard lock(mutex);occupied[slot]=false;
            // A concurrent slot failure invalidates any still-uncommitted batch.
            if(!enabled)output.reset();
            if(output){++batches;candidates+=seeds.size();if(!inspect)survivors+=output->size();upload+=s.upload;kernel+=s.kernel;download+=s.download;helperWall+=s.wall;copy+=copyElapsed;}
            else ++fallbackBatches;condition.notify_all();}
        return output;
    }
    nlohmann::json report(){std::lock_guard lock(mutex);return {{"available",enabled},{"failure",failure},{"initialization_s",initialize},
        {"device_info",device},{"slots",slots},{"completed_batches",batches},{"candidates",candidates},{"survivors",survivors},
        {"fallback_batches",fallbackBatches},{"upload_sum_s",upload},{"kernel_sum_s",kernel},{"download_sum_s",download},
        {"helper_wall_sum_s",helperWall},{"slot_wait_sum_s",wait},{"host_copy_sum_s",copy}};}
};
}
