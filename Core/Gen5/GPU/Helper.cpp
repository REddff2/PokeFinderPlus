#include "OpenCL12.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
using namespace GpuWild;
using json=nlohmann::json;
static double now(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
int wmain(int argc,wchar_t **argv){
    try {
        if(argc!=7 || std::stoul(argv[6])!=ProtocolVersion)throw std::runtime_error("GPU helper protocol mismatch; use CPU");
        std::wstring base=argv[1];int selected=std::stoi(argv[2]);std::wstring fault=argv[4];unsigned slots=std::stoul(argv[5]);
        SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
        HANDLE mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,base.c_str());wincheck(mapping!=nullptr,"Open mapping");
        auto shared=static_cast<Shared*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared)));wincheck(shared!=nullptr,"Map view");
        if(fault==L"init")throw std::runtime_error("injected initialization failure");
        CL::API cl;CL::U np=0;CL::check(cl.GetPlatformIDs(0,nullptr,&np));std::vector<CL::H> ps(np);CL::check(cl.GetPlatformIDs(np,ps.data(),nullptr));
        struct Device {CL::H id;json data;};std::vector<Device> devices;
        for(auto p:ps){CL::U n=0;auto status=cl.GetDeviceIDs(p,4,0,nullptr,&n);if(status==-1)continue;CL::check(status);
            std::vector<CL::H> ds(n);CL::check(cl.GetDeviceIDs(p,4,n,ds.data(),nullptr));
            for(auto d:ds)devices.push_back({d,{{"name",CL::info(cl,d,0x102b)},{"vendor",CL::info(cl,d,0x102c)},
                {"version",CL::info(cl,d,0x102f)},{"c_version",CL::info(cl,d,0x103d)},{"driver",CL::info(cl,d,0x102d)},
                {"compute_units",CL::info<CL::U>(cl,d,0x1002)},{"global_memory",CL::info<CL::Q>(cl,d,0x101f)},
                {"local_memory",CL::info<CL::Q>(cl,d,0x1023)},{"max_allocation",CL::info<CL::Q>(cl,d,0x1010)},
                {"unified_memory",bool(CL::info<CL::U>(cl,d,0x1035))}}});
        }
        std::stable_sort(devices.begin(),devices.end(),[](const auto&a,const auto&b){return a.data["unified_memory"]<b.data["unified_memory"];});
        if(fault==L"no-device" || selected<0 || selected>=int(devices.size()))throw std::runtime_error("No selected GPU");
        auto device=devices[selected].id;CL::I err=0;
        auto ctx=cl.CreateContext(nullptr,1,&device,nullptr,nullptr,&err);CL::check(err);
        std::ifstream input(argv[3]);std::string code((std::istreambuf_iterator<char>(input)),{});if(code.empty())throw std::runtime_error("Kernel unavailable");
        if(fault==L"build")code+="\n deliberate compile error";
        const char *source=code.data();size_t len=code.size();double started=now();
        auto program=cl.CreateProgramWithSource(ctx,1,&source,&len,&err);CL::check(err);
        auto status=cl.BuildProgram(program,1,&device,"-cl-std=CL1.2",nullptr,nullptr);
        if(status){size_t size=0;cl.GetProgramBuildInfo(program,device,0x1183,0,nullptr,&size);std::string log(size,0);cl.GetProgramBuildInfo(program,device,0x1183,size,log.data(),nullptr);throw std::runtime_error(log);}
        json info={{"selected",selected},{"device",devices[selected].data},{"compile_s",now()-started},{"devices",json::array()}};
        for(const auto&d:devices)info["devices"].push_back(d.data);
        auto description=info.dump();if(description.size()>=sizeof(shared->info))throw std::runtime_error("Device description overflow");
        memcpy(shared->info,description.c_str(),description.size()+1);
        struct Queue {CL::H queue,src,out,count,compact,inspect,ivCompact,ivInspect,cacheCompact;HANDLE request,done;};
        std::vector<Queue> qs;
        for(unsigned i=0;i<slots;++i){
            if(fault==L"allocation")throw std::runtime_error("injected allocation failure");
            Queue q{};q.queue=cl.CreateCommandQueue(ctx,device,2,&err);CL::check(err);
            q.src=cl.CreateBuffer(ctx,4,Capacity*8,nullptr,&err);CL::check(err);
            q.out=cl.CreateBuffer(ctx,1,Capacity*32,nullptr,&err);CL::check(err);
            q.count=cl.CreateBuffer(ctx,1,4,nullptr,&err);CL::check(err);
            q.compact=cl.CreateKernel(program,"compact",&err);CL::check(err);q.inspect=cl.CreateKernel(program,"inspect",&err);CL::check(err);
            q.ivCompact=cl.CreateKernel(program,"ivCompact",&err);CL::check(err);
            q.ivInspect=cl.CreateKernel(program,"ivInspect",&err);CL::check(err);
            q.cacheCompact=cl.CreateKernel(program,"cacheCompact",&err);CL::check(err);
            // Reject an old kernel before publishing readiness.
            CL::U bound=0;
            for(auto kernel:{q.compact,q.inspect,q.ivCompact,q.ivInspect,q.cacheCompact}){
                CL::check(cl.SetKernelArg(kernel,4,sizeof(bound),&bound));
                CL::check(cl.SetKernelArg(kernel,5,sizeof(bound),&bound));
            }
            q.request=OpenEventW(SYNCHRONIZE,FALSE,named(base,L"request",i).c_str());q.done=OpenEventW(EVENT_MODIFY_STATE,FALSE,named(base,L"done",i).c_str());
            wincheck(q.request&&q.done,"Open events");qs.push_back(q);
        }
        auto ready=OpenEventW(EVENT_MODIFY_STATE,FALSE,named(base,L"ready").c_str());shared->ready=1;SetEvent(ready);
        std::vector<std::thread> threads;std::atomic<unsigned> executions{0};
        for(unsigned i=0;i<slots;++i)threads.emplace_back([&,i]{
            try {
                const auto&q=qs[i];auto&s=shared->slots[i];
                while(WaitForSingleObject(q.request,INFINITE)==WAIT_OBJECT_0){
                    if(fault==L"after-one" && executions.fetch_add(1)>0)ExitProcess(91);
                    if(fault==L"reset")ExitProcess(91); // process death simulates driver reset/crash
                    if(fault==L"timeout")Sleep(INFINITE);
                    if(fault==L"execute")throw std::runtime_error("injected execution failure");
                    if(!s.n || s.n>Capacity)throw std::runtime_error("Batch outside capacity");
                    double start=now();CL::U zero=0,count=0;CL::H upload=nullptr,event=nullptr,readCount=nullptr,readOut=nullptr;
                    if(s.mode>2 || (s.mode==1 && (s.ivOffset>218 || s.ivRoamer>1)) || (s.mode==2 && (!s.ivRoamer || s.ivRoamer>8 || s.ivOffset+s.ivRoamer+31>224 || s.inspect)))throw std::runtime_error("Invalid IV plan");
                    auto k=s.mode==2?q.cacheCompact:s.mode==1?(s.inspect?q.ivInspect:q.ivCompact):(s.inspect?q.inspect:q.compact);
                    CL::check(cl.SetKernelArg(k,0,sizeof(q.src),&q.src));CL::check(cl.SetKernelArg(k,1,sizeof(q.out),&q.out));
                    CL::check(cl.SetKernelArg(k,2,sizeof(q.count),&q.count));CL::check(cl.SetKernelArg(k,3,4,&s.n));
                    CL::check(cl.SetKernelArg(k,4,sizeof(s.ivMin),&s.ivMin));
                    CL::check(cl.SetKernelArg(k,5,sizeof(s.ivMax),&s.ivMax));
                    if(s.mode!=0){CL::check(cl.SetKernelArg(k,6,sizeof(s.ivOffset),&s.ivOffset));CL::check(cl.SetKernelArg(k,7,sizeof(s.ivRoamer),&s.ivRoamer));}
                    CL::check(cl.EnqueueWriteBuffer(q.queue,q.src,0,0,size_t(s.n)*8,s.seeds,0,nullptr,&upload));
                    CL::check(cl.EnqueueWriteBuffer(q.queue,q.count,0,0,4,&zero,0,nullptr,nullptr));
                    size_t local=128,global=(s.n+127)/128*128;
                    CL::check(cl.EnqueueNDRangeKernel(q.queue,k,1,nullptr,&global,&local,0,nullptr,&event));
                    CL::check(cl.EnqueueReadBuffer(q.queue,q.count,0,0,4,&count,0,nullptr,&readCount));
                    CL::check(cl.Flush(q.queue));CL::check(cl.WaitForEvents(1,&readCount));
                    if(count>s.n)throw std::runtime_error("Counter overflow");
                    s.count=s.inspect?s.n*8:count;s.download=CL::seconds(cl,readCount);
                    if(s.count){CL::check(cl.EnqueueReadBuffer(q.queue,q.out,0,0,size_t(s.count)*4,s.output,0,nullptr,&readOut));
                        CL::check(cl.Flush(q.queue));CL::check(cl.WaitForEvents(1,&readOut));s.download+=CL::seconds(cl,readOut);cl.ReleaseEvent(readOut);}
                    s.upload=CL::seconds(cl,upload);s.kernel=CL::seconds(cl,event);s.wall=now()-start;
                    cl.ReleaseEvent(upload);cl.ReleaseEvent(event);cl.ReleaseEvent(readCount);
                    if(fault==L"invalid-output"){s.count=1;s.output[0]=s.n;}
                    SetEvent(q.done);
                }
            }catch(const std::exception&e){std::cerr<<"GPU_HELPER_FAILURE "<<e.what()<<std::endl;ExitProcess(90);}
        });
        for(auto&t:threads)t.join();return 0;
    }catch(const std::exception&e){std::cerr<<"GPU_HELPER_FAILURE "<<e.what()<<std::endl;return 90;}
}
