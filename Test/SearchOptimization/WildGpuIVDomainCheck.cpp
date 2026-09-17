#include "WildFixtures.hpp"
#include "BaselineWildGenerator5.hpp"
#include "IVDomainCases.hpp"
using CurrentGenerator=WildGenerator5;
#include <Core/Gen5/GPU/Session.hpp>
#include <Core/Gen5/GPU/Backend.hpp>
#include <Core/Gen5/IVCache.hpp>
#include <Core/RNG/MT.hpp>
#include <Core/RNG/MTFast.hpp>
#include <Core/Util/SearchOptimization.hpp>
#include <chrono>
#include <filesystem>
#include <thread>
#include <map>
using Row=std::pair<u32,WildState5>;
static double seconds(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
static bool passes(const std::array<u8,6>&ivs,const GpuWild::IVBounds&b){for(size_t i=0;i<6;++i)if(ivs[i]<b.min[i]||ivs[i]>b.max[i])return false;return true;}
static std::array<u8,6> ivs(u32 seed){MTFast<6,true> mt(seed);std::array<u8,6> result;for(auto&iv:result)iv=mt.next();return result;}
static CurrentGenerator current(const Request&r){return CurrentGenerator(r.initialPID,r.maxPID,r.offset,r.method,r.leads,r.powers,r.moving,r.required,r.area,r.profile.make(),r.filter.make(),r.requirePowerIV,r.requiredLeads,r.optimized,r.settings);}
static BaselineWildGenerator5 baseline(const Request&r){return BaselineWildGenerator5(r.initialPID,r.maxPID,r.offset,r.method,r.leads,r.powers,r.moving,r.required,r.area,r.profile.make(),r.filter.make(),r.requirePowerIV,r.requiredLeads);}
static Request request(const GpuWild::IVBounds&bounds){Request r;r.filter.low=bounds.min;r.filter.high=bounds.max;return r;}
static void same(const std::vector<Row>&a,const std::vector<Row>&b){check(a.size()==b.size(),"count/multiplicity mismatch");for(size_t i=0;i<a.size();++i)check(a[i].first==b[i].first&&fields(a[i].second)==fields(b[i].second),"seed index, order or full state mismatch");}
template<class G>static std::vector<Row> run(const G&g,const std::vector<u64>&seeds){std::vector<Row> rows;for(u32 i=0;i<seeds.size();++i)for(const auto&s:g.generate(seeds[i],0,0))rows.emplace_back(i,s);return rows;}
static std::vector<u32> indices(const std::vector<Row>&rows){std::vector<u32> out;for(const auto&row:rows)out.push_back(row.first);return out;}
static double median(std::vector<double>v){std::sort(v.begin(),v.end());return v[v.size()/2];}
static std::vector<Row> gpuRun(GpuWild::Session&session,const CurrentGenerator&g,const std::vector<u64>&seeds,const std::vector<Row>&expected,bool requireGpu=true){
    std::atomic<bool> cancel{false};std::vector<Row> rows;std::vector<u32> accepted;
    for(size_t begin=0;begin<seeds.size();begin+=GpuWild::Session::BatchSize){
        auto end=std::min(seeds.size(),begin+GpuWild::Session::BatchSize);std::vector<u64> batch(seeds.begin()+begin,seeds.begin()+end);
        auto kept=session.filter(batch,cancel,{g.getMinIVs(),g.getMaxIVs()});if(requireGpu)check(kept.has_value(),"GPU unexpectedly fell back");
        auto finish=[&](u32 index){for(const auto&s:g.generate(batch[index],0,0))rows.emplace_back(u32(begin)+index,s);};
        if(kept){for(u32 index:*kept){accepted.push_back(u32(begin)+index);finish(index);}}
        else for(u32 index=0;index<batch.size();++index)finish(index);
    }
    // CPU revalidation must not hide a GPU that ignored or broadened the bounds.
    if(requireGpu)check(accepted==indices(expected),"GPU raw survivor indices differ from exact requested bounds");
    same(expected,rows);return rows;
}
int main(int argc,char**argv){try{
    check(argc==5,"expected retained positives, IV cache, report JSON, legacy runtime directory");
    SearchOptimization::setPruningEnabled(true);SearchOptimization::setGpuEnabled(true);
    auto cases=ivDomainCases();Request broad=request({});auto broadGenerator=current(broad);
    check(broadGenerator.payloadFirstEnabled(0,0),"broad CPU proof guard enabled");
    std::ifstream f(argv[1]);auto known=json::parse(f);IVCache cache(argv[2]);check(cache.isValid(),"positive IV-cache fixture available");
    auto cacheMap=cache.getCache(0,0,Game::White,CacheType::Normal,broad.filter.make());std::vector<u32> cached;
    for(const auto&e:cacheMap)cached.push_back(u32(e.first));std::sort(cached.begin(),cached.end());
    std::vector<u64> positive;
    auto addPositive=[&](u32 upper){
        for(u32 low=0;low<(1<<23);++low){u64 seed=(u64(upper)<<32)|u32(mix(low));if(!broadGenerator.generate(seed,0,0).empty()){positive.push_back(seed);return;}}
        throw std::runtime_error("Unable to construct positive shiny payload fixture");
    };
    // Every matrix row has a genuine positive seed, including narrow mixed bounds.
    for(const auto&c:cases){
        auto found=std::find_if(cached.begin(),cached.end(),[&](u32 upper){return passes(cacheMap.at(upper),c.bounds);});
        if(found!=cached.end())addPositive(*found);
        else {bool done=false;for(u32 upper=0;upper<(1<<24);++upper)if(passes(ivs(upper),c.bounds)){addPositive(upper);done=true;break;}check(done,"mixed IV positive found");}
    }
    // Exercise each IV value 0..31 in each of the six positions on a surviving payload.
    std::array<std::array<bool,32>,6> covered{};unsigned remaining=192;
    for(u32 upper=0;remaining&&upper<65536;++upper){auto values=ivs(upper);bool useful=false;for(size_t stat=0;stat<6;++stat)useful|=!covered[stat][values[stat]];
        if(useful){addPositive(upper);for(size_t stat=0;stat<6;++stat)if(!covered[stat][values[stat]]){covered[stat][values[stat]]=true;--remaining;}}}
    check(!remaining,"positive corpus spans every stat and IV value");
    constexpr u32 count=1<<21;
    std::vector<u64> seeds(count);for(u32 i=0;i<count;++i)seeds[i]=mix(i);
    size_t next=32;for(const auto&row:known)seeds[next++]=row["seed"].get<u64>();
    for(u64 seed:positive){seeds[next++]=seed;seeds[next++]=seed;}
    seeds[next++]=0;seeds[next++]=~u64(0);seeds[next++]=u64(0x80000000u)<<32;
    std::vector<u64> warm(seeds.begin(),seeds.begin()+4096);
    auto directory=std::filesystem::path(argv[0]).parent_path();GpuWild::Options options;
    options.helperPath=(directory/"PokeFinderGpuHelper.exe").wstring();options.kernelPath=(directory/"Gen5Wild.cl").wstring();
    GpuWild::Session nvidia(options);options.device=1;GpuWild::Session intel(options);options.device=0;
    std::atomic<bool> cancel{false};check(nvidia.filter(warm,cancel,{}).has_value()&&intel.filter(warm,cancel,{}).has_value(),"both GPU devices active");
    json report;report["corpus_seeds"]=count;report["positive_fixtures"]=positive.size();report["cases"]=json::array();
    for(const auto&c:cases){
        auto r=request(c.bounds);auto g=current(r);check(g.payloadFirstEnabled(0,0)&&g.reducedIVsEnabled(0,0),"legal IV range enables both CPU paths");
        double start=seconds();auto expected=run(baseline(r),seeds);double baselineTime=seconds()-start;check(expected.size()>=2,"nonempty duplicate coverage");
        auto disabled=r;disabled.optimized=false;auto off=current(disabled);check(!off.payloadFirstEnabled(0,0)&&!off.reducedIVsEnabled(0,0),"Smart OFF disables optimizations");
        same(run(baseline(r),warm),run(off,warm));
        std::vector<double> cpuTimes,nvidiaTimes,intelTimes;
        for(unsigned repeat=0;repeat<3;++repeat){start=seconds();same(expected,run(g,seeds));cpuTimes.push_back(seconds()-start);
            start=seconds();gpuRun(nvidia,g,seeds,expected);nvidiaTimes.push_back(seconds()-start);
            start=seconds();gpuRun(intel,g,seeds,expected);intelTimes.push_back(seconds()-start);}
        json row={{"name",c.name},{"min",c.bounds.min},{"max",c.bounds.max},{"results",expected.size()},
            {"baseline_s",baselineTime},{"smart_cpu_median_s",median(cpuTimes)},{"nvidia_median_s",median(nvidiaTimes)},{"intel_median_s",median(intelTimes)}};
        report["cases"].push_back(row);std::cout<<row.dump()<<std::endl;
    }
    // Exhaustive one-stat legal intervals, with every IV value represented in positives.
    unsigned legal=0,invalid=0;
    for(size_t stat=0;stat<6;++stat)for(u8 low=0;low<=32;++low)for(u8 high=0;high<=32;++high){
        GpuWild::IVBounds bounds;bounds.min[stat]=low;bounds.max[stat]=high;
        auto r=request(bounds);bool valid=low<=high&&high<=31;
        check(bounds.valid()==valid&&current(r).payloadFirstEnabled(0,0)==valid,"legal interval guard");
        if(!valid){++invalid;continue;}++legal;
        std::vector<u32> expected;for(u32 i=0;i<positive.size();++i)if(passes(ivs(u32(positive[i]>>32)),bounds))expected.push_back(i);
        auto a=nvidia.filter(positive,cancel,bounds),b=intel.filter(positive,cancel,bounds);
        check(a&&b&&*a==expected&&*b==expected,"exhaustive GPU interval comparison");
    }
    report["legal_intervals_per_gpu"]=legal;report["invalid_intervals_rejected"]=invalid;
    // Raw GPU outputs are inspected independently of whether candidates survive.
    std::vector<u64> inspectSeeds(seeds.begin(),seeds.begin()+(1<<20));
    for(int device=0;device<2;++device){GpuWild::Backend backend(device,options.helperPath,options.kernelPath,L"",1);
        auto out=backend.execute(inspectSeeds,{},true);check(out&&out->size()==inspectSeeds.size()*8,"GPU inspect output size");
        for(size_t i=0;i<inspectSeeds.size();++i){MT mt(u32(inspectSeeds[i]>>32));u32 packed=0;for(unsigned j=0;j<6;++j)packed|=(mt.next()>>27)<<(5*j);
            check((*out)[8*i+4]==packed,"independent full-MT versus GPU IVs");}
    }
    report["inspected_iv_seeds_per_gpu"]=inspectSeeds.size();
    // Changing per-batch bounds across simultaneous workers cannot mix parameters.
    options.queues=2;GpuWild::Session concurrent(options);std::atomic<unsigned> completed{0};std::vector<std::thread> workers;
    for(unsigned worker=0;worker<8;++worker)workers.emplace_back([&,worker]{try{for(unsigned round=0;round<10;++round){
        const auto&bounds=cases[(worker+round)%cases.size()].bounds;auto r=request(bounds);auto expected=run(baseline(r),warm);gpuRun(concurrent,current(r),warm,expected);}
        ++completed;}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;}});
    for(auto&t:workers)t.join();check(completed==8,"concurrent bounds remain associated with each batch");options.queues=1;
    // Protocol mismatch, stale kernels, and execution faults replay exact broad/mixed requests.
    for(const wchar_t*fault:{L"no-device",L"init",L"build",L"allocation",L"execute",L"reset",L"timeout",L"invalid-output",L"after-one"}){
        auto fail=options;fail.fault=fault;fail.timeoutMs=250;GpuWild::Session session(fail);
        if(fail.fault==L"after-one")gpuRun(session,broadGenerator,warm,run(baseline(broad),warm));
        for(size_t i:{size_t(0),size_t(14)}){auto r=request(cases[i].bounds);gpuRun(session,current(r),warm,run(baseline(r),warm),false);}
    }
    for(bool staleHelper:{false,true}){auto old=options;auto legacy=std::filesystem::path(argv[4]);
        if(staleHelper)old.helperPath=(legacy/"PokeFinderGpuHelper.exe").wstring();else old.kernelPath=(legacy/"Gen5Wild.cl").wstring();
        GpuWild::Session session(old);gpuRun(session,broadGenerator,warm,run(baseline(broad),warm),false);
        check(!json::parse(session.diagnostics())["backend"]["available"].get<bool>(),"stale helper/kernel must fail closed to CPU");}
    GpuWild::Session uninitialized(options);GpuWild::IVBounds bad;bad.min[0]=32;
    check(!uninitialized.filter(warm,cancel,bad)&&json::parse(uninitialized.diagnostics())["initialization_attempts"]==0,"invalid bounds never initialize GPU");
    SearchOptimization::setGpuEnabled(false);check(!uninitialized.filter(warm,cancel,{})&&json::parse(uninitialized.diagnostics())["initialization_attempts"]==0,"GPU OFF never initializes");
    SearchOptimization::setGpuEnabled(true);
    auto stalled=options;stalled.fault=L"timeout";GpuWild::Session pending(stalled);double started=seconds();
    std::thread stop([&]{std::this_thread::sleep_for(std::chrono::milliseconds(1500));cancel=true;});auto ignored=pending.filter(warm,cancel,{});stop.join();
    check(!ignored&&seconds()-started<4,"cancellation remains responsive");
    report["nvidia"]=json::parse(nvidia.diagnostics());report["intel"]=json::parse(intel.diagnostics());report["pass"]=true;
    std::ofstream(argv[3])<<report.dump(2);std::cout<<"IV_DOMAIN_PASS exact_raw_GPU_indices_fields_and_multiplicities"<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"IV_DOMAIN_FAIL "<<e.what()<<std::endl;return 1;}}
