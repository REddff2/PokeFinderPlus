#define main coverageCpuMain
#include "Gen5CoverageCheck.cpp"
#undef main
#include <Core/Gen5/Generators/WildGenerator5.hpp>
#include <Core/Gen5/Searchers/WildSearcher5.hpp>
#include <atomic>

static Profile5 bulkProfile()
{
    return {"Coverage bulk",Game::Black2,12345,54321,"","",0x001122334455ULL,
        {true,false,false,false,false,false,false,false,false},0x60,6,6,false,0x1100,0x111f,false,false,DSType::DS,Language::English};
}
static double ms(Clock::time_point start){return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
template<class Searcher>static auto bulkCollect(Searcher &s,double &duration)
{
    Date start(2026,6,1),end(2026,6,13);s.setMaxProgress(s.getMaxProgress(start,end));auto begin=Clock::now();s.startSearch(4,start,end);auto rows=s.getResults();
    while(s.isSearching())
    {
        if(ms(begin)>120000){s.cancelSearch();throw std::runtime_error("bulk search timeout");}
        auto next=s.getResults();rows.insert(rows.end(),next.begin(),next.end());std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    auto next=s.getResults();rows.insert(rows.end(),next.begin(),next.end());duration=ms(begin);require(s.getProgress()==100,"GPU/CPU complete progress");require(s.getWorkerCount()==4,"bulk threads preserved");
    std::sort(rows.begin(),rows.end(),[](const auto&a,const auto&b){return std::tuple(a.getDateTime(),a.getTimer0(),a.getButtons(),a.getState().getAdvances())<std::tuple(b.getDateTime(),b.getTimer0(),b.getButtons(),b.getState().getAdvances());});return rows;
}
template<class Factory>static json runFamily(const std::string &name,Factory factory)
{
    using Family = SearchOptimization::Family;
    const Family owner = name=="Static"?Family::Static:name=="Grotto Pokemon"?Family::HiddenGrotto:
        name=="Dream Radar"?Family::DreamRadar:name=="Pickup"?Family::Pickup:Family::Wild;
    for (size_t i=0;i<SearchOptimization::FamilyCount;++i)
        SearchOptimization::setFamilyEnabled(static_cast<Family>(i),static_cast<Family>(i)==owner);
    json row={{"family",name},{"candidates",13ULL*32*86400},{"workers",4}};mode(0);auto baseline=factory(nullptr);double elapsed;auto expected=bulkCollect(*baseline,elapsed);row["baseline_ms"]=elapsed;require(!expected.empty(),"nonempty bulk fixture");row["records"]=expected.size();
    for(int m=1;m<=2;++m)
    {
        std::vector<double> times;
        for(int rep=0;rep<3;++rep)
        {
            mode(m);auto session=std::make_shared<GpuWild::Session>();auto s=factory(session);auto actual=bulkCollect(*s,elapsed);same(expected,actual,name+" bulk mode "+std::to_string(m));times.push_back(elapsed);
            auto diagnostic=json::parse(session->diagnostics());
            if(m==2){require(diagnostic["backend"]["completed_batches"].get<u64>()>0,"IV GPU actually dispatched");require(diagnostic["backend"]["fallback_batches"]==0,"healthy bulk GPU no replay");row["gpu_runs"].push_back(diagnostic);}
            else require(diagnostic["initialization_attempts"]==0,"Smart CPU never initializes GPU");
        }
        std::sort(times.begin(),times.end());row[m==1?"smart_ms":"gpu_ms"]=times[1];row[m==1?"smart_samples_ms":"gpu_samples_ms"]=times;
    }
    row["gpu_speedup"]=row["smart_ms"].get<double>()/row["gpu_ms"].get<double>();std::cout<<row.dump()<<std::endl;return row;
}
static std::vector<char> contents(const std::filesystem::path &path){std::ifstream file(path,std::ios::binary);return {std::istreambuf_iterator<char>(file),{}};}
static json cacheRun(const std::filesystem::path &dir)
{
    for (size_t i=0;i<SearchOptimization::FamilyCount;++i)
        SearchOptimization::setFamilyEnabled(static_cast<SearchOptimization::Family>(i),static_cast<SearchOptimization::Family>(i)==SearchOptimization::Family::CacheBuilders);
    constexpr u32 count=1<<25;json row={{"candidates",count},{"workers",4}};std::vector<char> expected;
    for(int m=0;m<3;++m)
    {
        std::vector<double> times;for(int repeat=0;repeat<(m?3:1);++repeat)
        {
            mode(m);auto session=std::make_shared<GpuWild::Session>();IVCacheSearcher cache(0,0,session);cache.setMaxProgress(count);auto start=Clock::now();cache.startSearch(4,0,count-1);
            while(cache.isSearching()){require(ms(start)<120000,"cache timeout");std::this_thread::sleep_for(std::chrono::milliseconds(1));}
            times.push_back(ms(start));require(cache.getProgress()==100,"GPU cache full progress");auto file=dir/("cache-gpu-mode"+std::to_string(m)+".bin");cache.writeResults(file.string());auto bytes=contents(file);
            if(m==0)expected=bytes;else require(bytes==expected,"GPU cache exact binary/multiplicity equality");
            if(m==2){auto d=json::parse(session->diagnostics());require(d["backend"]["completed_batches"].get<u64>()>0,"cache GPU used");require(d["backend"]["candidates"]==count,"cache GPU entire range");row["gpu_runs"].push_back(d);}
        }
        std::sort(times.begin(),times.end());row[m==0?"baseline_ms":m==1?"smart_ms":"gpu_ms"]=times[times.size()/2];
    }
    row["file_bytes"]=expected.size();row["gpu_speedup"]=row["smart_ms"].get<double>()/row["gpu_ms"].get<double>();std::cout<<row.dump()<<std::endl;return row;
}
static json fallbacks(const std::filesystem::path &dir)
{
    for (size_t i=0;i<SearchOptimization::FamilyCount;++i) SearchOptimization::setFamilyEnabled(static_cast<SearchOptimization::Family>(i),true);
    json report;mode(2);std::vector<u64> seeds(1<<20);for(u32 i=0;i<seeds.size();++i)seeds[i]=seedAt(i);std::atomic<bool> cancelled=false;
    GpuWild::IVBounds bounds{{20,20,20,20,20,20},{31,31,31,31,31,31}};GpuWild::IVPlan plan{2,false};
    std::vector<u32> expected;for(u32 i=0;i<seeds.size();++i){Gen5::IVRNG iv(seeds[i]>>32,2,6,true);bool ok=true;for(int k=0;k<6;++k)ok&=iv.next()>=20;if(ok)expected.push_back(i);}
    for(const wchar_t *fault:{L"no-device",L"init",L"build",L"allocation",L"execute",L"reset",L"timeout",L"invalid-output",L"after-one"})
    {
        GpuWild::Options opts;opts.fault=fault;opts.timeoutMs=250;GpuWild::Session session(opts);
        if(opts.fault==L"after-one"){auto prefix=session.filter(seeds,cancelled,bounds,plan);require(prefix&&*prefix==expected,"committed GPU prefix");}
        auto indices=session.filter(seeds,cancelled,bounds,plan);require(!indices,"IV GPU fault requests complete CPU replay");report.push_back({{"fault",std::filesystem::path(fault).string()},{"diagnostics",json::parse(session.diagnostics())}});
    }
    GpuWild::Options integrated;integrated.device=1;GpuWild::Session integratedSession(integrated);require(!integratedSession.filter(seeds,cancelled,bounds,plan),"integrated IV candidate falls back for performance");
    GpuWild::Session small;std::vector<u64> shortBatch(seeds.begin(),seeds.begin()+65536);require(!small.filter(shortBatch,cancelled,bounds,plan),"small batch CPU fallback");require(json::parse(small.diagnostics())["initialization_attempts"]==0,"small batch does not initialize OpenCL");
    GpuWild::Options missing;missing.helperPath=(dir/"absent-helper.exe").wstring();GpuWild::Session absent(missing);require(!absent.filter(seeds,cancelled,bounds,plan),"missing helper fallback");
    GpuWild::Options timeout;timeout.fault=L"timeout";GpuWild::Session stalled(timeout);auto began=Clock::now();std::thread stop([&]{std::this_thread::sleep_for(std::chrono::milliseconds(750));cancelled=true;});auto ignored=stalled.filter(seeds,cancelled,bounds,plan);stop.join();require(!ignored&&ms(began)<3000,"GPU cancellation responsive");
    // Replay through a real cache builder, including a committed prefix.
    constexpr u32 count=1<<25;mode(1);IVCacheSearcher cpu(0,0);cpu.startSearch(4,0,count-1);while(cpu.isSearching())std::this_thread::sleep_for(std::chrono::milliseconds(1));cpu.writeResults((dir/"cache-replay-expected.bin").string());
    mode(2);GpuWild::Options failure;failure.fault=L"after-one";auto session=std::make_shared<GpuWild::Session>(failure);IVCacheSearcher failed(0,0,session);failed.setMaxProgress(count);failed.startSearch(4,0,count-1);while(failed.isSearching())std::this_thread::sleep_for(std::chrono::milliseconds(1));failed.writeResults((dir/"cache-replay-actual.bin").string());require(contents(dir/"cache-replay-actual.bin")==contents(dir/"cache-replay-expected.bin"),"cache failed batch replay exact bytes");require(failed.getProgress()==100,"cache replay progress");
    return report;
}
int main(int argc,char**argv)
{
    QCoreApplication app(argc,argv);
    try
    {
        require(argc==2,"evidence directory required");std::filesystem::path dir=argv[1];std::filesystem::create_directories(dir);json report;auto p=bulkProfile();Filters f;f.low.fill(25);
        auto save=[&]{std::ofstream out(dir/"gpu-integration.json");out<<report.dump(2)<<'\n';};
        auto t=*Encounters5::getStaticEncounter(0,0);
        report["families"].push_back(runFamily("Static",[&](auto session){StaticGenerator5 g(0,0,0,Method::Method5,Lead::None,0,t,p,f.state());return std::make_unique<StaticSearcher5>(0,0,g,p,session);}));save();
        auto slot=Encounters5::getHiddenGrottoEncounters().front().getPokemon(0,0,p.getVersion());
        report["families"].push_back(runFamily("Grotto Pokemon",[&](auto session){HiddenGrottoGenerator g(0,0,0,Lead::None,0,slot,p,f.state());return std::make_unique<HiddenGrottoIVSearcher>(0,0,g,p,session);}));save();
        std::vector<DreamRadarTemplate> radar{*Encounters5::getDreamRadarEncounters(0)};
        report["families"].push_back(runFamily("Dream Radar",[&](auto session){DreamRadarGenerator g(0,0,8,radar,p,f.state());return std::make_unique<Searcher5<DreamRadarGenerator,DreamRadarState>>(g,p,session);}));save();
        auto area=Encounters5::getEncounters(Encounter::Grass,{false,0},&p).front();std::array<PickupGenerator::Slot,6>slots;for(auto &s:slots)s={true,100,{}};
        report["families"].push_back(runFamily("Pickup",[&](auto session){PickupGenerator g(0,0,slots,area,p,f.wild());return std::make_unique<Searcher5<PickupGenerator,PickupState>>(g,p,session);}));save();
        report["families"].push_back(runFamily("Wild independent IV",[&](auto session){WildGenerator5 g(0,0,0,Method::Method5,Lead::None,0,false,false,area,p,f.wild());return std::make_unique<WildSearcher5>(0,0,g,p,session);}));save();
        report["cache"]=cacheRun(dir);save();report["fallbacks"]=fallbacks(dir);report["passed"]=true;save();std::cout<<"GEN5_GPU_INTEGRATION_PASS\n";return 0;
    }
    catch(const std::exception&e){std::cerr<<"GEN5_GPU_INTEGRATION_FAIL "<<e.what()<<'\n';return 1;}
}
