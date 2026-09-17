#include "WildFixtures.hpp"
#include "BaselineWildGenerator5.hpp"
#include "IVDomainCases.hpp"
using CurrentGenerator=WildGenerator5;
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
static bool passes(const std::array<u8,6>&ivs,const IVBounds&b){for(size_t i=0;i<6;++i)if(ivs[i]<b.min[i]||ivs[i]>b.max[i])return false;return true;}
static std::array<u8,6> ivs(u32 seed){MTFast<6,true> mt(seed);std::array<u8,6> result;for(auto&iv:result)iv=mt.next();return result;}
static CurrentGenerator current(const Request&r){return CurrentGenerator(r.initialPID,r.maxPID,r.offset,r.method,r.leads,r.powers,r.moving,r.required,r.area,r.profile.make(),r.filter.make(),r.requirePowerIV,r.requiredLeads,r.optimized,r.settings);}
static BaselineWildGenerator5 baseline(const Request&r){return BaselineWildGenerator5(r.initialPID,r.maxPID,r.offset,r.method,r.leads,r.powers,r.moving,r.required,r.area,r.profile.make(),r.filter.make(),r.requirePowerIV,r.requiredLeads);}
static Request request(const IVBounds&bounds){Request r;r.filter.low=bounds.min;r.filter.high=bounds.max;return r;}
static void same(const std::vector<Row>&a,const std::vector<Row>&b){check(a.size()==b.size(),"count/multiplicity mismatch");for(size_t i=0;i<a.size();++i)check(a[i].first==b[i].first&&fields(a[i].second)==fields(b[i].second),"seed index, order or full state mismatch");}
template<class G>static std::vector<Row> run(const G&g,const std::vector<u64>&seeds){std::vector<Row> rows;for(u32 i=0;i<seeds.size();++i)for(const auto&s:g.generate(seeds[i],0,0))rows.emplace_back(i,s);return rows;}
static std::vector<u32> indices(const std::vector<Row>&rows){std::vector<u32> out;for(const auto&row:rows)out.push_back(row.first);return out;}
static double median(std::vector<double>v){std::sort(v.begin(),v.end());return v[v.size()/2];}
int main(int argc,char**argv){try{
    check(argc==4,"expected retained positives, IV cache, report JSON");
    SearchOptimization::setPruningEnabled(true);
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
    json report;report["corpus_seeds"]=count;report["positive_fixtures"]=positive.size();report["cases"]=json::array();
    for(const auto&c:cases){
        auto r=request(c.bounds);auto g=current(r);check(g.payloadFirstEnabled(0,0)&&g.reducedIVsEnabled(0,0),"legal IV range enables both CPU paths");
        double start=seconds();auto expected=run(baseline(r),seeds);double baselineTime=seconds()-start;check(expected.size()>=2,"nonempty duplicate coverage");
        auto disabled=r;disabled.optimized=false;auto off=current(disabled);check(!off.payloadFirstEnabled(0,0)&&!off.reducedIVsEnabled(0,0),"Smart OFF disables optimizations");
        same(expected,run(off,seeds));
        start=seconds();same(expected,run(g,seeds));double smartTime=seconds()-start;
        json row={{"name",c.name},{"min",c.bounds.min},{"max",c.bounds.max},{"results",expected.size()},
            {"baseline_s",baselineTime},{"smart_cpu_s",smartTime}};
        report["cases"].push_back(row);std::cout<<row.dump()<<std::endl;
    }
    // Exhaustive one-stat legal intervals, with every IV value represented in positives.
    unsigned legal=0,invalid=0;
    for(size_t stat=0;stat<6;++stat)for(u8 low=0;low<=32;++low)for(u8 high=0;high<=32;++high){
        IVBounds bounds;bounds.min[stat]=low;bounds.max[stat]=high;
        auto r=request(bounds);bool valid=low<=high&&high<=31;
        check(bounds.valid()==valid&&current(r).payloadFirstEnabled(0,0)==valid,"legal interval guard");
        if(!valid){++invalid;continue;}++legal;
        std::vector<u32> expected;for(u32 i=0;i<positive.size();++i)if(passes(ivs(u32(positive[i]>>32)),bounds))expected.push_back(i);
        auto actual=run(current(r),positive);
        check(indices(actual)==expected,"exhaustive interval survivors");
        same(run(baseline(r),positive),actual);
    }
    report["legal_intervals"]=legal;report["invalid_intervals_rejected"]=invalid;
    // Independently compare every reduced prefix against the full MT algorithm.
    for(u32 i=0;i<count;++i){MT full(u32(seeds[i]>>32));auto reduced=ivs(u32(seeds[i]>>32));
        for(size_t stat=0;stat<6;++stat)check((full.next()>>27)==reduced[stat],"reduced MT matches full MT");}
    report["full_mt_comparisons"]=count;report["pass"]=true;
    std::ofstream(argv[3])<<report.dump(2);std::cout<<"IV_DOMAIN_PASS exact_CPU_fields_order_and_multiplicities"<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"IV_DOMAIN_FAIL "<<e.what()<<std::endl;return 1;}}
