// Reuse the all-field record serializer and independent baseline fixtures.
#define main coverageCpuMain
#include "Gen5CoverageCheck.cpp"
#undef main
#include <Core/Gen5/GPU/Backend.hpp>
#include <Core/Gen5/Generators/WildGenerator5.hpp>
#include "BaselineWildGenerator5.hpp"
#include <atomic>

static u32 packedIV(u64 seed,GpuWild::IVPlan plan)
{
    MT mt(seed>>32,plan.offset);IVs values;
    for(u8 i=0;i<6;++i)values[i]=mt.next()>>27;
    if(plan.roamer) {auto copy=values;values[4]=copy[3];values[5]=copy[4];values[3]=copy[5];}
    u32 result=0;for(u8 i=0;i<6;++i)result|=u32(values[i])<<(i*5);return result;
}
static bool ivPass(u32 packed,const GpuWild::IVBounds &bounds)
{
    for(u8 i=0;i<6;++i){auto value=(packed>>(i*5))&31;if(value<bounds.min[i]||value>bounds.max[i])return false;}return true;
}
static double elapsed(Clock::time_point start){return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}

static json kernelProof(GpuWild::Backend &backend)
{
    std::vector<u64> seeds(32768);for(u32 i=0;i<seeds.size();++i)seeds[i]=seedAt(i);
    seeds[11]=seeds[0];seeds[12]=seeds[0];
    const std::vector<GpuWild::IVBounds> bounds={{{0,0,0,0,0,0},{31,31,31,31,31,31}},
        {{15,15,15,15,15,15},{31,31,31,31,31,31}},{{0,25,10,0,31,7},{31,31,22,31,31,29}},
        {{0,0,0,0,0,0},{0,0,0,0,0,0}},{{31,31,31,31,31,31},{31,31,31,31,31,31}}};
    u64 checked=0,occurrences=0;
    for(u32 offset:{0u,1u,2u,3u,7u,9u,22u,35u,79u,127u,191u,218u})for(bool roamer:{false,true})
    {
        GpuWild::IVPlan plan{offset,roamer};auto inspect=backend.execute(seeds,bounds.front(),true,nullptr,plan);
        require(inspect&&inspect->size()==seeds.size()*8,"IV kernel inspect size");
        std::vector<u32> packed;for(u32 i=0;i<seeds.size();++i)
        {
            auto expected=packedIV(seeds[i],plan);require((*inspect)[8*i+4]==expected,"MT/IV GPU field equality");
            u8 bits=(expected&1)|(((expected>>5)&1)<<1)|(((expected>>10)&1)<<2)|(((expected>>25)&1)<<3)|(((expected>>15)&1)<<4)|(((expected>>20)&1)<<5);
            require((*inspect)[8*i+5]==bits*15/63,"GPU Hidden Power equality");packed.push_back(expected);
        }
        for(const auto &bound:bounds)
        {
            std::vector<u32> expected;for(u32 i=0;i<seeds.size();++i)if(ivPass(packed[i],bound))expected.push_back(i);
            auto actual=backend.execute(seeds,bound,false,nullptr,plan);require(actual&&*actual==expected,"GPU IV ranges and multiplicity");occurrences+=expected.size();
        }
        checked+=seeds.size();
    }
    return {{"MT_windows",checked},{"accepted_occurrences",occurrences},{"passed",true}};
}

template<class Generate>static std::pair<double,u64> parallelTime(const std::vector<u64>&seeds,Generate generate,int workers)
{
    const auto start=Clock::now();std::atomic<u64> count=0;std::vector<std::thread> threads;
    for(int worker=0;worker<workers;++worker)threads.emplace_back([&,worker]{u64 local=0;for(size_t i=worker;i<seeds.size();i+=workers)local+=generate(seeds[i]).size();count+=local;});
    for(auto&t:threads)t.join();return {elapsed(start),count.load()};
}
template<class Baseline,class Smart>static json family(const std::string&name,GpuWild::Backend&backend,
    const std::vector<u64>&seeds,GpuWild::IVPlan plan,const GpuWild::IVBounds&bounds,Baseline baseline,Smart smart)
{
    auto indices=backend.execute(seeds,bounds,false,nullptr,plan);require(indices.has_value(),"family GPU available");
    u64 expectedCount=0;size_t survivor=0;u64 comparisons=0;
    for(u32 i=0;i<seeds.size();++i)
    {
        auto expected=baseline(seeds[i]);auto cpu=smart(seeds[i]);same(expected,cpu,name+" baseline/CPU");
        const bool present=survivor<indices->size()&&(*indices)[survivor]==i;
        require(expected.empty()||present,name+" no GPU false negatives");
        if(present){same(expected,smart(seeds[i]),name+" baseline/GPU");++survivor;}
        expectedCount+=expected.size();++comparisons;
    }
    require(expectedCount>0,name+" nonempty restrictive fixture");
    json row={{"family",name},{"seeds",seeds.size()},{"results",expectedCount},{"comparisons",comparisons}};
    for(int workers:{1,8,24})
    {
        std::vector<double> times;
        for(int rep=0;rep<5;++rep){auto [time,count]=parallelTime(seeds,smart,workers);require(count==expectedCount,"CPU timed count");times.push_back(time);}
        std::sort(times.begin(),times.end());row["smart_ms"][std::to_string(workers)]=times[2];
    }
    std::vector<double> gpuTimes;
    for(int rep=0;rep<5;++rep)
    {
        auto start=Clock::now();auto accepted=backend.execute(seeds,bounds,false,nullptr,plan);require(accepted.has_value(),"GPU timed batch");
        u64 count=0;for(auto i:*accepted)count+=smart(seeds[i]).size();require(count==expectedCount,"GPU timed result count");gpuTimes.push_back(elapsed(start));
    }
    std::sort(gpuTimes.begin(),gpuTimes.end());row["gpu_ms"]=gpuTimes[2];row["speedup_vs_24_workers"]=row["smart_ms"]["24"].template get<double>()/gpuTimes[2];
    std::cout<<row.dump()<<std::endl;return row;
}

static bool cacheCandidate(u64 seed,GpuWild::IVPlan plan)
{
    MT mt(seed>>32,plan.offset);std::vector<u8> values(plan.cacheFrames+31);
    for(auto &v:values)v=mt.next()>>27;
    auto pass=[](u8 hp,u8 atk,u8 def,u8 spa,u8 spd,u8 spe,bool roamer){return hp>=30&&def>=30&&spd>=30&&(atk>=30||spa>=30)&&(spe>=30||(!roamer&&spe<=1));};
    for(u32 i=0;i<plan.cacheFrames+4;++i)
    {
        u32 j=i+22;if(pass(values[j],values[j+1],values[j+2],values[j+3],values[j+4],values[j+5],false))return true;
        if(i<plan.cacheFrames+2&&pass(values[i],values[i+1],values[i+2],values[i+3],values[i+4],values[i+5],false))return true;
        j=i+1;if(i<plan.cacheFrames&&pass(values[j],values[j+1],values[j+2],values[j+5],values[j+3],values[j+4],true))return true;
    }
    return false;
}
static json cacheProof(GpuWild::Backend &backend)
{
    json report;GpuWild::IVBounds bounds{{0,0,0,0,0,0},{31,31,31,31,31,31}};
    std::vector<u64> seeds(1<<20);for(u32 i=0;i<seeds.size();++i)seeds[i]=u64(i)<<32;
    for(auto plan:std::vector<GpuWild::IVPlan>{{0,false,1},{0,false,8},{100,false,4},{185,false,8},{192,false,1}})
    {
        std::vector<u32> expected;for(u32 i=0;i<seeds.size();++i)if(cacheCandidate(seeds[i],plan))expected.push_back(i);
        require(!expected.empty(),"cache GPU positive fixtures");
        auto start=Clock::now();auto actual=backend.execute(seeds,bounds,false,nullptr,plan);auto time=elapsed(start);
        require(actual&&*actual==expected,"GPU union of Entralink/normal/roamer cache predicates");
        report.push_back({{"offset",plan.offset},{"frames",plan.cacheFrames},{"seeds",seeds.size()},{"survivors",expected.size()},{"gpu_ms",time}});
    }
    return report;
}

int main(int argc,char**argv)
{
    try
    {
        require(argc>=2,"output JSON required");mode(2);json report;
        auto dir=std::filesystem::absolute(argv[0]).parent_path();
        std::vector<u64> seeds(1<<20);for(u32 i=0;i<seeds.size();++i)seeds[i]=seedAt(i);seeds[11]=seeds[0];seeds[12]=seeds[0];
        for(int device=0;device<2;++device)
        {
            GpuWild::Backend backend(device,(dir/"PokeFinderGpuHelper.exe").wstring(),(dir/"Gen5Wild.cl").wstring());
            require(backend.available(),"requested GPU available");json deviceReport;deviceReport["kernel_proof"]=kernelProof(backend);deviceReport["cache_proof"]=cacheProof(backend);
            Filters f;f.low.fill(20);auto p=profile();GpuWild::IVBounds bounds{f.low,f.high};
            auto t=*Encounters5::getStaticEncounter(0,0);CoverageBaselineStaticGenerator5 sb(0,0,0,Method::Method5,Lead::None,0,t,p,f.state());StaticGenerator5 sg(0,0,0,Method::Method5,Lead::None,0,t,p,f.state());
            deviceReport["benchmarks"].push_back(family("Static",backend,seeds,*sg.gpuIVPlan(0,0),bounds,[&](u64 s){return sb.generate(s,0,0);},[&](u64 s){return sg.generate(s,0,0);}));
            auto slot=Encounters5::getHiddenGrottoEncounters().front().getPokemon(0,0,p.getVersion());CoverageBaselineHiddenGrottoGenerator gb(0,0,0,Lead::None,0,slot,p,f.state());HiddenGrottoGenerator gg(0,0,0,Lead::None,0,slot,p,f.state());
            deviceReport["benchmarks"].push_back(family("Grotto Pokemon",backend,seeds,*gg.gpuIVPlan(0,0),bounds,[&](u64 s){return gb.generate(s,0,0);},[&](u64 s){return gg.generate(s,0,0);}));
            std::vector<DreamRadarTemplate> radar{*Encounters5::getDreamRadarEncounters(0)};CoverageBaselineDreamRadarGenerator rb(0,0,8,radar,p,f.state());DreamRadarGenerator rg(0,0,8,radar,p,f.state());
            deviceReport["benchmarks"].push_back(family("Dream Radar",backend,seeds,*rg.gpuIVPlan(),bounds,[&](u64 s){return rb.generate(s);},[&](u64 s){return rg.generate(s);}));
            auto area=Encounters5::getEncounters(Encounter::Grass,{false,0},&p).front();std::array<PickupGenerator::Slot,6>slots;std::array<CoverageBaselinePickupGenerator::Slot,6>oldSlots;for(int j=0;j<6;++j){slots[j]={true,100,{}};oldSlots[j]={true,100,{}};}
            CoverageBaselinePickupGenerator pb(0,0,oldSlots,area,p,f.wild());PickupGenerator pg(0,0,slots,area,p,f.wild());
            deviceReport["benchmarks"].push_back(family("Pickup",backend,seeds,*pg.gpuIVPlan(),bounds,[&](u64 s){return pb.generate(s);},[&](u64 s){return pg.generate(s);}));
            BaselineWildGenerator5 wb(0,0,0,Method::Method5,Lead::None,0,false,false,area,p,f.wild());WildGenerator5 wg(0,0,0,Method::Method5,Lead::None,0,false,false,area,p,f.wild());
            deviceReport["benchmarks"].push_back(family("Wild independent IV",backend,seeds,*wg.gpuIVPlan(0,0),bounds,[&](u64 s){return wb.generate(s,0,0);},[&](u64 s){return wg.generate(s,0,0);}));
            deviceReport["backend"]=backend.report();report["devices"].push_back(deviceReport);
            std::ofstream output(argv[1]);output<<report.dump(2)<<'\n';
        }
        report["passed"]=true;std::ofstream output(argv[1]);output<<report.dump(2)<<'\n';std::cout<<"GEN5_GPU_COVERAGE_PASS\n";return 0;
    }
    catch(const std::exception&e){std::cerr<<"GEN5_GPU_COVERAGE_FAIL "<<e.what()<<'\n';return 1;}
}
