#define main coverageCpuMain
#include "Gen5CoverageCheck.cpp"
#undef main
#include "coverage-baseline/CoverageBaselineIVCacheSearcher.hpp"
#include "coverage-baseline/CoverageBaselineSHA1CacheSearcher.hpp"
#include <Core/Gen5/IVCache.hpp>
#include <Core/Gen5/SHA1Cache.hpp>
#include <Core/Gen5/Searchers/SHA1CacheSearcher.hpp>
#include <Core/Gen5/Searchers/ProfileSearcher5.hpp>
#include <Core/Gen5/Searchers/IDSearcher5.hpp>
#include <Core/Gen5/States/ProfileSearcherState5.hpp>
#include <Core/RNG/SHA1.hpp>

static std::vector<char> bytes(const std::filesystem::path &path){std::ifstream in(path,std::ios::binary);require(in.good(),"open evidence file");return {std::istreambuf_iterator<char>(in),{}};}
static double ms(Clock::time_point start){return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
template<class S>static void waitSearch(S&s){auto deadline=Clock::now()+std::chrono::seconds(120);while(s.isSearching()){if(Clock::now()>deadline){s.cancelSearch();throw std::runtime_error("worker timeout");}std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
template<class V>static void writeValue(std::ofstream&out,V value){out.write(reinterpret_cast<const char*>(&value),sizeof(value));}

static json caches(const std::filesystem::path &dir)
{
    json report;u64 fileChecks=0;
    for(const auto &[initial,extra]:std::vector<std::pair<u32,u32>>{{0,0},{0,7},{192,0},{193,0},{217,3},{624,2}})
    {
        auto basePath=dir/"cache-baseline.bin";auto actualPath=dir/"cache-actual.bin";
        const u32 count=1<<20;mode(0);CoverageBaselineIVCacheSearcher baseline(initial,extra);auto start=Clock::now();baseline.runRange(0,count-1);double baselineMs=ms(start);baseline.writeResults(basePath.string());auto expected=bytes(basePath);
        require(expected.size()>12+4*(extra*3+9),"nonempty cache fixture");
        json row={{"initial",initial},{"extra",extra},{"seeds",count},{"baseline_ms",baselineMs},{"bytes",expected.size()}};
        for(int m=0;m<3;++m)for(int workers:{1,4})
        {
            mode(m);IVCacheSearcher searcher(initial,extra);searcher.setMaxProgress(count);start=Clock::now();searcher.startSearch(workers,0,count-1);waitSearch(searcher);double time=ms(start);searcher.writeResults(actualPath.string());
            require(bytes(actualPath)==expected,"cache files identical including table multiplicities");require(searcher.getProgress()==100,"cache complete progress");
            IVCache cache(actualPath.string(),true);require(cache.isValid(),"generated cache readable");
            Filters broad;for(auto type:{CacheType::Normal,CacheType::Roamer,CacheType::Entralink})for(auto game:{Game::Black,Game::Black2})
            {auto map=cache.getCache(initial,extra,game,type,broad.state());for(const auto &[key,ivs]:map){require((key>>32)>=initial,"cache advance metadata");for(auto iv:ivs)require(iv<=31,"cache legal IVs");}}
            row["modes"].push_back({{"mode",m},{"workers",workers},{"ms",time}});++fileChecks;
        }
        report["IV_builder"].push_back(row);std::cout<<"CACHE_RANGE_PASS "<<initial<<' '<<extra<<std::endl;
    }
    // Final 32-bit seeds exercise inclusive endpoint/wrap handling.
    mode(0);CoverageBaselineIVCacheSearcher last(0,0);last.runRange(0xfffffff0,0xffffffff);last.writeResults((dir/"last-base.bin").string());
    mode(1);IVCacheSearcher end(0,0);end.setMaxProgress(16);end.startSearch(4,0xfffffff0,0xffffffff);waitSearch(end);end.writeResults((dir/"last-smart.bin").string());require(bytes(dir/"last-base.bin")==bytes(dir/"last-smart.bin"),"full u32 cache endpoint");require(end.getProgress()==100,"cache endpoint progress");
    IVCacheSearcher cancelled(0,0);cancelled.startSearch(4);cancelled.cancelSearch();waitSearch(cancelled);require(cancelled.isCancelled(),"cache cancellation");
    report["identical_cache_files"]=fileChecks+1;

    // Standard-format deterministic IV fixture, spanning all three table types.
    // Seed membership is deliberately small; the reader re-derives actual IVs.
    auto p=profile(Game::Black);Date day(2026,6,15);SHA1 sha(p);sha.setDate(day);sha.setTimer0(p.getTimer0Min(),p.getVCount());sha.setButton(Keypresses::getValue(Buttons::None));auto alpha=sha.precompute();
    std::vector<u32> seedList;
    for(u32 second=0;second<64;++second){sha.setTime(0,0,second,p.getDSType());seedList.push_back(sha.hashSeed(alpha)>>32);}
    std::sort(seedList.begin(),seedList.end());
    auto fixture=dir/"iv-fixture.bin";
    {std::ofstream out(fixture,std::ios::binary);writeValue<u32>(out,0xd08cb7c0);writeValue<u32>(out,0);writeValue<u32>(out,0);for(int i=0;i<9;++i)writeValue<u32>(out,seedList.size());for(int i=0;i<9;++i)for(auto seed:seedList)writeValue(out,seed);}
    p = Profile5("Coverage cache",Game::Black,12345,54321,fixture.string(),"",0x001122334455ULL,
        {true,false,false,false,false,false,false,false,false},0x60,6,6,false,0x1100,0x1100,false,false,DSType::DS,Language::English);
    IVCache ivCache(fixture.string(),true);require(ivCache.isValid(),"synthetic IV fixture loads");
    CoverageBaselineSHA1CacheSearcher frozenSHA(ivCache,p,day,day);frozenSHA.startSearch(1);waitSearch(frozenSHA);frozenSHA.writeResults((dir/"sha-frozen.bin").string());
    std::vector<char> expectedSHA=bytes(dir/"sha-frozen.bin");
    for(int m=0;m<3;++m)
    {
        mode(m);auto start=Clock::now();SHA1CacheSearcher searcher(ivCache,p,day,day);searcher.setMaxProgress(searcher.getMaxProgress());searcher.startSearch(m?4:1);waitSearch(searcher);auto duration=ms(start);auto file=dir/("sha-"+std::to_string(m)+".bin");searcher.writeResults(file.string());auto contents=bytes(file);
        require(contents==expectedSHA,"SHA cache bytes invariant across policy/threads");
        SHA1Cache cache(file.string());require(cache.isValid(p),"SHA cache profile compatibility");Filters broad;auto ivMap=ivCache.getCache(0,0,p.getVersion(),CacheType::Normal,broad.state());auto shaMap=cache.getCache(0,0,day,day,ivMap,CacheType::Normal,p);require(shaMap.size()>=64,"SHA fixture positive membership");
        auto t=*Encounters5::getStaticEncounter(0,0);StaticGenerator5 sg(0,0,0,Method::Method5,Lead::None,0,t,p,broad.state());
        StaticSearcher5CacheFast staticCached(0,0,shaMap,ivMap,sg,p);staticCached.startSearch(1,day,day);waitSearch(staticCached);auto cachedRows=staticCached.getResults();require(cachedRows.size()>=64,"Static cache-fast positive rows");
        StaticSearcher5Fast staticFast(0,0,ivMap,sg,p);staticFast.startSearch(1,day,day);waitSearch(staticFast);same(staticFast.getResults(),cachedRows,"Static IV/SHA cache equality");
        auto ga=Encounters5::getHiddenGrottoEncounters().front().getPokemon(0,0,Game::Black2);HiddenGrottoGenerator gg(0,0,0,Lead::None,0,ga,p,broad.state());HiddenGrottoIVSearcherCacheFast gc(0,0,shaMap,ivMap,gg,p);gc.startSearch(1,day,day);waitSearch(gc);HiddenGrottoIVSearcherFast gf(0,0,ivMap,gg,p);gf.startSearch(1,day,day);waitSearch(gf);same(gf.getResults(),gc.getResults(),"Grotto IV/SHA cache equality");
        report["SHA_builder"].push_back({{"mode",m},{"ms",duration},{"file_bytes",contents.size()},{"entries",shaMap.size()}});
    }
    // A two-day job really starts concurrent SHA workers (one-day jobs cap at one).
    const Date secondDay = day + 1;
    CoverageBaselineSHA1CacheSearcher multiBase(ivCache,p,day,secondDay);
    multiBase.startSearch(2);waitSearch(multiBase);multiBase.writeResults((dir/"sha-threads-base.bin").string());
    mode(1);SHA1CacheSearcher multiSmart(ivCache,p,day,secondDay);
    multiSmart.startSearch(2);waitSearch(multiSmart);multiSmart.writeResults((dir/"sha-threads-smart.bin").string());
    require(bytes(dir/"sha-threads-base.bin")==bytes(dir/"sha-threads-smart.bin"),"concurrent immutable SHA membership exact bytes");
    report["SHA_two_day_two_workers"] = true;
    return report;
}
static json profileChecks()
{
    json report;Filters f;f.low.fill(15);Date day(2026,6,15);Time time(12,0,0);
    for(int kind=0;kind<3;++kind)
    {
        json expected;std::vector<double> elapsed;
        for(int m=0;m<3;++m)
        {
            mode(m);std::unique_ptr<ProfileSearcher5> s;
            if(kind==0)s=std::make_unique<ProfileIVSearcher5>(day,time,0,5,0x60,0x64,0x1100,0x1120,6,7,Game::Black2,Language::English,DSType::DS,0x001122334455ULL,Buttons::None,f.low,f.high);
            if(kind==1)s=std::make_unique<ProfileNeedleSearcher5>(day,time,0,5,0x60,0x64,0x1100,0x1120,6,7,Game::Black2,Language::English,DSType::DS,0x001122334455ULL,Buttons::None,std::vector<u8>{0},true,false);
            if(kind==2){auto p=profile();SHA1 sha(p);sha.setDate(day);sha.setTimer0(p.getTimer0Min(),p.getVCount());sha.setButton(Keypresses::getValue(Buttons::None));sha.setTime(12,0,0,p.getDSType());u64 seed=sha.hashSeed(sha.precompute());s=std::make_unique<ProfileSeedSearcher5>(day,time,0,5,0x60,0x64,0x1100,0x1120,6,7,Game::Black2,Language::English,DSType::DS,0x001122334455ULL,Buttons::None,seed);}
            auto start=Clock::now();s->startSearch(4,0,255);waitSearch(*s);elapsed.push_back(ms(start));json rows=json::array();for(const auto&r:s->getResults())rows.push_back({r.getSeed(),r.getTimer0(),r.getVCount(),r.getVFrame(),r.getGxStat(),r.getSecond()});std::sort(rows.begin(),rows.end());require(!rows.empty(),"profile positive results");if(!m)expected=rows;else require(rows==expected,"profile full VFrame domain equality");
        }
        report.push_back({{"kind",kind},{"records",expected.size()},{"mode_ms",elapsed}});
    }
    return report;
}
static json fallbackChecks()
{
    json report;auto p=profile();IDFilter idFilter({},{},{},{},{},{});
    IDGenerator5 id(0,127,0,false,false,p,idFilter);auto area=Encounters5::getPhenomenonEncounters().front();PhenomenonFilter filter(true,{});PhenomenonGenerator phenomenon(0,127,0,area,p,filter);
    for(u32 i=0;i<128;++i){auto seed=seedAt(i);mode(0);auto ids=id.generate(seed);auto phenomena=phenomenon.generate(seed);modes("ID fallback",ids,[&]{return id.generate(seed);});modes("Phenomenon fallback",phenomena,[&]{return phenomenon.generate(seed);});for(bool chatot:{false,true})require(AdjacentSeedsCalculator::previewPRNG(seed,i,16,chatot)==CoverageBaselineAdjacentSeedsCalculator::previewPRNG(seed,i,16,chatot),"Chatot/needle preview");}
    report.push_back(benchmark("ID (unchanged generator)",[&](u64 s){mode(0);return id.generate(s);},[&](u64 s){mode(1);return id.generate(s);},1024));
    report.push_back(benchmark("Phenomenon (unchanged generator)",[&](u64 s){mode(0);return phenomenon.generate(s);},[&](u64 s){mode(1);return phenomenon.generate(s);},1024));
    // Date result accumulation applies to ID, slots and Pickup too.
    IDFilter narrow({12345},{},{},{},{},{});mode(0);IDGenerator5 base(0,0,0,false,false,p,narrow);Searcher5<IDGenerator5,IDState> bs(base,p);auto expected=collect(bs,1);for(int m=1;m<3;++m){mode(m);Searcher5<IDGenerator5,IDState>s(base,p);same(expected,collect(s),"ID date accumulation");require(!s.getGpuSession(),"ID GPU unsupported fallback");}
    return report;
}
int main(int argc,char**argv)
{
    QCoreApplication app(argc,argv);
    try{require(argc==2,"evidence directory required");std::filesystem::path dir=argv[1];std::filesystem::create_directories(dir);json report;report["caches"]=caches(dir);report["profiles"]=profileChecks();report["fallbacks"]=fallbackChecks();report["passed"]=true;std::ofstream out(dir/"infrastructure.json");out<<report.dump(2)<<'\n';std::cout<<report.dump(2)<<"\nGEN5_INFRASTRUCTURE_PASS\n";return 0;}
    catch(const std::exception&e){std::cerr<<"GEN5_INFRASTRUCTURE_FAIL "<<e.what()<<'\n';return 1;}
}
