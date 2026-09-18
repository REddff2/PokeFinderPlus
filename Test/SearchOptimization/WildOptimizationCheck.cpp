#include "BaselineWildGenerator5.hpp"
#include "BaselineWildSearcher5.hpp"
#include <Core/Enum/Buttons.hpp>
#include <Core/Enum/DSType.hpp>
#include <Core/Enum/Encounter.hpp>
#include <Core/Enum/Game.hpp>
#include <Core/Enum/Language.hpp>
#include <Core/Enum/Method.hpp>
#include <Core/Gen5/Encounters5.hpp>
#include <Core/Gen5/SHA1Cache.hpp>
#include <Core/Gen5/Searchers/WildSearcher5.hpp>
#include <Core/RNG/MT.hpp>
#include <Core/RNG/RNGList.hpp>
#include <Core/RNG/SHA1.hpp>
#include <Core/Util/SearchMetrics.hpp>
#include <Core/Util/SearchOptimization.hpp>
#include <QCoreApplication>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <thread>
#include <tuple>

using json=nlohmann::json;
using Clock=std::chrono::steady_clock;
using IVs=std::array<u8,6>;
using IVList=std::vector<std::pair<u32,IVs>>;
static u64 comparisons=0, records=0;
static void check(bool ok,const std::string &message) { if(!ok) throw std::runtime_error(message); }

struct Filters
{
    IVs low{},high{31,31,31,31,31,31};
    std::array<bool,25> natures;
    std::array<bool,16> powers;
    std::array<bool,13> encounterSlots;
    u8 ability=255,gender=255,shiny=255,levelMin=1,levelMax=100;
    bool skip=false;
    Filters() { natures.fill(true); powers.fill(true); encounterSlots.fill(true); }
    WildStateFilter make() const { return {gender,ability,shiny,levelMin,levelMax,0,255,0,255,skip,low,high,natures,powers,encounterSlots}; }
};

static Profile5 profile(Game game=Game::Black,bool memory=false,bool charm=false,bool ns=false)
{
    return Profile5("Wild milestone 2",game,12345,54321,"","",0x001122334455ULL,
        {true,false,false,false,false,false,false,false,false},0x60,6,6,false,0x1100,0x1100,
        memory,charm,DSType::DS,Language::English,ns);
}
static EncounterArea5 area(const Profile5 &p,Encounter type=Encounter::Grass,int location=-1,u8 season=0,bool swarm=false)
{
    auto areas=Encounters5::getEncounters(type,{swarm,season},&p);
    check(!areas.empty(),"encounter areas missing: "+std::to_string(toInt(type)));
    if(location<0) return areas.front();
    auto found=std::ranges::find_if(areas,[&](const auto &a){return a.getLocation()==location;});
    check(found!=areas.end(),"location missing: "+std::to_string(location)); return *found;
}

static auto fields(const WildState5 &s)
{
    return std::tuple(s.getEC(),s.getPID(),s.getStats(),s.getAbilityIndex(),s.getIVs(),s.getAbility(),s.getCharacteristic(),
        s.getGender(),s.getHiddenPower(),s.getHiddenPowerStrength(),s.getLevel(),s.getNature(),s.getShiny(),
        s.getAdvances(),s.getIVAdvances(),s.getEncounterSlot(),s.getSpecie(),s.getForm(),s.getItem(),
        s.getLead(),s.getLeadMask(),s.getMovingTrigger(),s.getMovingSteps(),s.getPhenomenon(),s.getPhenomenonItem(),
        s.isValid(),static_cast<const WildGeneratorState&>(s).isValid(),s.getPassPower(),s.getVariableNature(),
        s.getLeadRequired(),s.getChatot(),s.getNeedle());
}
static auto fields(const SearcherState5<WildState5> &s)
{
    return std::tuple(s.getInitialSeed(),s.getDateTime(),s.getTimer0(),s.getButtons(),fields(s.getState()));
}
template<class State> static void same(const std::vector<State> &base,const std::vector<State> &actual,const std::string &name)
{
    check(base.size()==actual.size(),name+" count: "+std::to_string(base.size())+" vs "+std::to_string(actual.size()));
    for(size_t i=0;i<base.size();++i) check(fields(base[i])==fields(actual[i]),name+" field/order at "+std::to_string(i));
    ++comparisons; records+=base.size();
}

struct Config
{
    Profile5 p=profile();
    EncounterArea5 a=area(p,Encounter::Grass,41);
    Filters f;
    std::vector<Lead> leads{Lead::None};
    std::vector<u8> powers{0};
    u32 initial=0,count=127,offset=0;
    bool moving=false,requireMoving=false,requireIV=false,requiredLeads=true;
    BaselineWildGenerator5 baseline() const
    { return {initial,count,offset,Method::Method5,leads,powers,moving,requireMoving,a,p,f.make(),requireIV,requiredLeads}; }
    WildGenerator5 generator(bool optimized=true) const
    { return {initial,count,offset,Method::Method5,leads,powers,moving,requireMoving,a,p,f.make(),requireIV,requiredLeads,optimized}; }
};

static IVList ivList(u64 seed,Game game,u32 initial,u32 count)
{
    static constexpr auto gen=+[](MT &rng)->u8 { return rng.next()>>27; };
    // Same final MT output order as WildGenerator5; neither lead nor PID changes IVs.
    RNGList<u8,MT,8,gen> rng(seed>>32,initial+((game&Game::BW)!=Game::None?0:2));
    IVList result;
    for(u32 i=0;i<=count;++i,rng.advanceState())
    {
        IVs iv; for(auto &v:iv) v=rng.next(); result.emplace_back(initial+i,iv);
    }
    return result;
}

static Game parseGame(const std::string &s) { return s=="Black"?Game::Black:s=="Black2"?Game::Black2:s=="White"?Game::White:Game::White2; }
static Encounter parseEncounter(const std::string &s)
{
    return std::map<std::string,Encounter>{{"Grass",Encounter::Grass},{"Surfing",Encounter::Surfing},{"SuperRod",Encounter::SuperRod}}.at(s);
}
static Lead parseLead(const std::string &s)
{
    return std::map<std::string,Lead>{{"None",Lead::None},{"CompoundEyes",Lead::CompoundEyes},{"CuteCharmF",Lead::CuteCharmF},
        {"MagnetPull",Lead::MagnetPull},{"Pressure",Lead::Pressure},{"Static",Lead::Static},{"Synchronize",Lead::Synchronize},{"SuctionCups",Lead::SuctionCups}}.at(s);
}
static void fixtures()
{
    std::ifstream file(std::string(POKEFINDER_SOURCE_DIR)+"/Test/Gen5/wild5.json"); auto data=json::parse(file);
    for(const auto &d:data["generate"])
    {
        Config c; c.p=profile(parseGame(d["version"])); c.a=area(c.p,parseEncounter(d["encounter"]),d["location"]);
        c.leads={parseLead(d["lead"])}; c.requiredLeads=false; c.count=99;
        std::string power=d["luckyPower"]; c.powers={u8(power=="None"?0:power.back()-'0')};
        u64 seed=d["seed"]; auto base=c.baseline().generate(seed,0,0);
        check(base.size()>=d["results"].size(),"stored fixture count");
        for(size_t i=0;i<d["results"].size();++i)
        {
            const auto &s=base[i]; const auto &r=d["results"][i];
            check(s.getPID()==r["pid"] && s.getIVs()==r["ivs"].get<IVs>() && s.getStats()==r["stats"].get<std::array<u16,6>>()
                && s.getAdvances()==r["advances"] && s.getEncounterSlot()==r["encounterSlot"] && s.getSpecie()==r["specie"]
                && s.getForm()==r["form"] && s.getItem()==r["item"] && s.getNature()==r["nature"] && s.getGender()==r["gender"]
                && s.getAbility()==r["ability"] && s.getAbilityIndex()==r["abilityIndex"] && s.getCharacteristic()==r["characteristic"]
                && s.getLevel()==r["level"] && s.getShiny()==r["shiny"] && s.getChatot()==r["chatot"]
                && s.getHiddenPower()==r["hiddenPower"] && s.getHiddenPowerStrength()==r["hiddenPowerStrength"],"stored fixture "+d["name"].get<std::string>());
        }
        same(base,c.generator().generate(seed,0,0),"fixture enabled");
        same(base,c.generator(false).generate(seed,0,0),"fixture disabled");
        for(int hp=0;hp<16;++hp)
        {
            c.f.powers.fill(false); c.f.powers[hp]=true;
            same(c.baseline().generate(seed,0,4),c.generator().generate(seed,0,4),"fixture HP "+std::to_string(hp));
        }
    }
    std::cout<<"STORED_FIXTURES "<<data["generate"].size()<<" PASS\n";
}

static void equivalence()
{
    fixtures();
    const Game games[]={Game::Black,Game::White,Game::Black2,Game::White2};
    const Encounter types[]={Encounter::Grass,Encounter::GrassDark,Encounter::Surfing,Encounter::GrassRustling,
        Encounter::SurfingRippling,Encounter::SuperRodRippling,Encounter::SuperRod,Encounter::DustCloud,Encounter::FlyingShadow};
    const Lead leads[]={Lead::None,Lead::Synchronize,static_cast<Lead>(24),Lead::CuteCharmM,Lead::CuteCharmF,
        Lead::MagnetPull,Lead::Static,Lead::Pressure,Lead::CompoundEyes,Lead::SuctionCups,Lead::ArenaTrap};
    u64 sequence=0; size_t itemRows=0,invalidRows=0,exactRows=0,swarmRows=0;
    for(int i=0;i<1584;++i)
    {
        sequence=sequence*6364136223846793005ULL+1442695040888963407ULL;
        Config c; c.p=profile(games[i%4],i%3==0,i%5==0,i%7==0);
        auto type=types[(i/4)%9]; auto list=Encounters5::getEncounters(type,{i%3==0,u8((i/36)%4)},&c.p);
        check(!list.empty(),"branch has encounter table"); c.a=list[(sequence>>8)%list.size()];
        c.leads={leads[(i/36)%11]}; c.count=31+(i%4)*16; c.initial=i%9; c.offset=i%5;
        c.moving=i%3!=0; c.requireMoving=i%3==2; c.requireIV=i%2; c.requiredLeads=i%5!=0;
        c.powers={u8((i/11)%4),u8(PassPower5::combine(i%4,(i/7)%4))};
        if(i%7==0) c.leads={Lead::None,Lead::Synchronize,static_cast<Lead>(24),Lead::CuteCharmM,Lead::CuteCharmF,
            Lead::MagnetPull,Lead::Static,Lead::Pressure,Lead::CompoundEyes,Lead::SuctionCups,Lead::ArenaTrap};
        auto ivs=ivList(sequence,c.p.getVersion(),0,3);
        // Deliberately duplicate records and advances to exercise merge multiplicity.
        if(i%5==0) { ivs.push_back(ivs[0]); ivs.emplace_back(0,ivs[2].second); }
        const int filterMode=(sequence>>44)%4;
        if(filterMode==1) c.f.low.fill(10);
        if(filterMode==2) { c.f.low=c.f.high=ivs[0].second; }
        if(i%6==1) { c.f.powers.fill(false); c.f.powers[(i/6)%16]=true; }
        if(i%6==2) { c.f.encounterSlots.fill(false); c.f.encounterSlots[(i/6)%13]=true; }
        if(i%6==3) { c.f.ability=i%2; c.f.gender=(i/2)%3; c.f.shiny=1+(i%2); }
        if(i%6==4) { c.f.natures.fill(false); c.f.natures[(i/6)%25]=true; c.f.levelMin=5; c.f.levelMax=40; }
        if(i%13==0) c.f.skip=true;
        std::string context="varied "+std::to_string(i)+" type "+std::to_string(toInt(type));
        auto base=c.baseline().generate(sequence,0,3);
        auto opt=c.generator().generate(sequence,0,3);
        same(base,opt,context+" ordinary");
        same(base,c.generator(false).generate(sequence,0,3),context+" disabled");
        same(c.baseline().generate(sequence,ivs),c.generator().generate(sequence,ivs),context+" supplied IVs");
        for(const auto &s:base) { itemRows+=s.getPhenomenonItem(); invalidRows+=!s.isValid(); exactRows+=filterMode==2; swarmRows+=s.getEncounterSlot()==12; }
    }
    // Super Rod proof: restrictive IVs discard failed bites too at the public boundary.
    Config rod; rod.a=area(rod.p,Encounter::SuperRod,101); rod.count=511;
    auto all=rod.baseline().generate(0,0,0); check(!all.empty(),"rod nonempty");
    check(std::ranges::any_of(all,[](const auto &s){return !s.isValid();}),"rod failed bites covered");
    rod.f.low=rod.f.high=ivList(0,Game::Black,0,0)[0].second;
    same(rod.baseline().generate(0,0,3),rod.generator().generate(0,0,3),"rod exact nonempty");
    rod.f.low.fill(31); rod.f.high.fill(31);
    check(rod.baseline().generate(0,0,3).empty(),"rod baseline rejects mismatching IVs including invalid bites");
    same(rod.baseline().generate(0,0,3),rod.generator().generate(0,0,3),"rod rejects IVs");
    check(itemRows && invalidRows && exactRows && swarmRows,"nonempty item/invalid/exact/swarm coverage");
    std::cout<<"GENERATOR_EQUIVALENCE comparisons="<<comparisons<<" records="<<records
        <<" item_rows="<<itemRows<<" invalid_rows="<<invalidRows<<" exact_rows="<<exactRows<<" swarm_rows="<<swarmRows<<" PASS\n";
}

template<class Searcher> static std::vector<SearcherState5<WildState5>> collect(Searcher &searcher,const Date &start,const Date &end,int workers=1)
{
    searcher.setMaxProgress(searcher.getMaxProgress(start,end)); searcher.startSearch(workers,start,end);
    const auto deadline=Clock::now()+std::chrono::minutes(3);
    while(searcher.isSearching() && Clock::now()<deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if(searcher.isSearching()) { searcher.cancelSearch(); throw std::runtime_error("search timeout"); }
    check(searcher.getProgress()==100,"search complete"); return searcher.getResults();
}
template<class State> static void normalize(std::vector<State> &states)
{ std::sort(states.begin(),states.end(),[](const auto &a,const auto &b){return fields(a)<fields(b);}); }

struct Caches { fph::MetaFphMap<u64,IVs> iv; fph::MetaFphMap<u64,u64> sha; };
static Caches caches(const Config &c,const Date &start,const Date &end,u32 maxIV)
{
    Caches result; SHA1SSE sha(c.p); auto keys=Keypresses::getKeypresses(c.p); auto f=c.f.make();
    for(Date day=start;day<=end;day=day+1)
    {
        sha.setDate(day); sha.setTimer0(c.p.getTimer0Min(),c.p.getVCount()); auto alpha=sha.precompute();
        for(auto key:keys)
        {
            sha.setButton(key.value);
            for(u32 second=0;second<86400;second+=4)
            {
                sha.setTime(second,c.p.getDSType()); auto seeds=sha.hashSeed(alpha);
                for(u32 k=0;k<4;++k)
                {
                    bool found=false;
                    for(const auto &iv:ivList(seeds[k],c.p.getVersion(),0,maxIV))
                    {
                        if(!f.compareIV(iv.second)) continue;
                        constexpr int order[]={0,1,2,5,3,4}; u8 bits=0;
                        for(int i=0;i<6;++i) bits|=(iv.second[order[i]]&1)<<i;
                        if(!f.compareHiddenPower(bits*15/63)) continue;
                        result.iv[(u64(iv.first)<<32)|(seeds[k]>>32)]=iv.second; found=true;
                    }
                    if(found) result.sha[SHA1Key(toInt(key.button),second+k,day.getJD()-Date().getJD(),c.p.getTimer0Min()).key]=seeds[k];
                }
            }
        }
    }
    return result;
}
static void searchEquivalence()
{

    for(Game game:{Game::Black,Game::Black2})
    {
        Config c; c.p=profile(game); c.a=area(c.p,Encounter::Grass,game==Game::Black?41:20);
        c.count=7; c.f.low.fill(20); c.leads={Lead::None,Lead::Synchronize,Lead::Static};
        Date start(2012,10,7),end(2012,10,8); auto cache=caches(c,start,end,2);
        check(!cache.iv.empty() && !cache.sha.empty(),"cache domain nonempty");
        auto base=c.baseline(); auto live=c.generator(); auto off=c.generator(false);
        BaselineWildSearcher5<BaselineWildGenerator5,WildState5> b(0,2,base,c.p);
        WildSearcher5 n(0,2,live,c.p),disabled(0,2,off,c.p);
        auto expected=collect(b,start,end,2); normalize(expected); check(!expected.empty(),"ordinary date matches nonempty");
        auto actual=collect(n,start,end,1); normalize(actual); same(expected,actual,"ordinary dates workers 2/1");
        actual=collect(disabled,start,end,2); normalize(actual); same(expected,actual,"ordinary dates toggle off");
        BaselineWildSearcher5Fast<BaselineWildGenerator5,WildState5> bf(0,2,cache.iv,base,c.p);
        WildSearcher5Fast nf(0,2,cache.iv,live,c.p);
        auto fastBase=collect(bf,start,end,2); normalize(fastBase); actual=collect(nf,start,end,1); normalize(actual);
        same(fastBase,actual,"fast dates"); same(expected,actual,"ordinary/fast domain");
        BaselineWildSearcher5CacheFast<BaselineWildGenerator5,WildState5> bc(0,2,cache.sha,cache.iv,base,c.p);
        WildSearcher5CacheFast nc(0,2,cache.sha,cache.iv,live,c.p);
        auto cachedBase=collect(bc,start,end,2); normalize(cachedBase); actual=collect(nc,start,end,1); normalize(actual);
        same(cachedBase,actual,"cache-fast dates"); same(expected,actual,"ordinary/cache-fast domain");
        std::cout<<"DATE_PATHS game="<<(game==Game::Black?"Black":"Black2")<<" seeds=172800 results="<<expected.size()<<" ordinary_fast_cachefast=PASS\n";
        Config cancelConfig=c; cancelConfig.count=999; cancelConfig.f=Filters();
        WildSearcher5 cancelled(0,2,cancelConfig.generator(),c.p); cancelled.startSearch(4,start,end); cancelled.cancelSearch();
        auto deadline=Clock::now()+std::chrono::seconds(5);
        while(cancelled.isSearching() && Clock::now()<deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        check(!cancelled.isSearching(),"Wild cancellation");
    }
}

struct Batch { u64 count=0,digest=0; };
template<class Generator> static Batch batch(const Generator &g,const std::vector<u64> &seeds,u32 maxIV=3)
{
    Batch total;
    for(auto seed:seeds)
    {
        auto states=g.generate(seed,0,maxIV); total.count+=states.size();
        if(!states.empty()) total.digest^=(u64(states.front().getPID())<<32)|states.back().getPID();
    }
    return total;
}
static void printCounters(const std::string &name,bool optimized,u64 accepted)
{
#ifdef POKEFINDER_SEARCH_INSTRUMENTATION
    std::cout<<"COUNTS "<<name<<" optimized="<<optimized<<" iv_candidates="<<SearchMetrics::wildIVCandidates
        <<" pid_frames="<<SearchMetrics::wildPIDFrames<<" iv_checks="<<SearchMetrics::ivChecks
        <<" existing_early_iv="<<SearchMetrics::wildExistingIVRejected
        <<" added_early_iv="<<SearchMetrics::earlyIVRejected<<" early_hp="<<SearchMetrics::earlyHiddenPowerRejected
        <<" early_frames="<<SearchMetrics::wildEarlyFrameRejected<<" full_states="<<SearchMetrics::statesConstructed
        <<" accepted="<<accepted<<" result_reallocations="<<SearchMetrics::wildResultReallocations<<'\n';
#endif
}
static void benchmark()
{
    std::vector<u64> seeds; u64 seed=0;
    for(int i=0;i<256;++i) { seeds.push_back(seed); seed=seed*6364136223846793005ULL+1442695040888963407ULL; }
    for(std::string name:{"normal_broad","normal_moderate","normal_exact","normal_hidden_power","normal_slot_nature","surfing_moderate","super_rod_moderate"})
    {
        Config c; c.count=255;
        if(name.find("moderate")!=std::string::npos) c.f.low.fill(10);
        if(name=="normal_exact") c.f.low=c.f.high=ivList(0,Game::Black,0,0)[0].second;
        if(name=="normal_hidden_power") { c.f.powers.fill(false); c.f.powers[13]=true; }
        if(name=="normal_slot_nature") { c.f.encounterSlots.fill(false); c.f.encounterSlots[0]=true; c.f.natures.fill(false); c.f.natures[0]=true; }
        if(name=="surfing_moderate") c.a=area(c.p,Encounter::Surfing,101);
        if(name=="super_rod_moderate") c.a=area(c.p,Encounter::SuperRod,101);
        auto base=c.baseline(); auto opt=c.generator();
#ifdef POKEFINDER_SEARCH_INSTRUMENTATION
        SearchMetrics::reset(); auto b=batch(base,seeds); printCounters(name,false,b.count);
        SearchMetrics::reset(); auto o=batch(opt,seeds); printCounters(name,true,o.count);
        check(b.count==o.count && b.digest==o.digest,"instrumented count/digest");
#else
        auto warmBase=batch(base,seeds),warmOpt=batch(opt,seeds); check(warmBase.count==warmOpt.count,"warmup count");
        std::vector<double> bt,ot; u64 sink=0;
        auto timed=[&](const auto &g){auto began=Clock::now();auto result=batch(g,seeds);sink+=result.count^result.digest;
            return std::chrono::duration<double,std::milli>(Clock::now()-began).count();};
        for(int i=0;i<7;++i) { if(i%2){ot.push_back(timed(opt));bt.push_back(timed(base));}else{bt.push_back(timed(base));ot.push_back(timed(opt));} }
        std::sort(bt.begin(),bt.end());std::sort(ot.begin(),ot.end());
        std::cout<<std::fixed<<std::setprecision(3)<<"TIMING "<<name<<" workers=1 seeds=256 iv_frames=4 pid_frames=256 baseline_ms="<<bt[3]
            <<" optimized_ms="<<ot[3]<<" speedup="<<bt[3]/ot[3]<<" baseline_range="<<bt.front()<<':'<<bt.back()
            <<" optimized_range="<<ot.front()<<':'<<ot.back()<<" accepted="<<warmBase.count<<" sink="<<sink<<std::endl;
        for(auto s:seeds) same(base.generate(s,0,3),opt.generate(s,0,3),"timed "+name);
#endif
    }
    std::cout<<"BENCHMARK_EQUIVALENCE comparisons="<<comparisons<<" records="<<records<<'\n';
}

static void searchBenchmark()
{
    Config c; c.count=999; c.f.low.fill(10); c.f.powers.fill(false); c.f.powers[13]=true;
    // Normal Wild, Black / Chargestone Cave 1F / no lead / one day. No caches.
    Date day(2012,10,7);
    auto base=c.baseline(); auto opt=c.generator();
    auto runBase=[&]{ BaselineWildSearcher5<BaselineWildGenerator5,WildState5> s(0,0,base,c.p); return collect(s,day,day); };
    auto runOpt=[&]{ WildSearcher5 s(0,0,opt,c.p); return collect(s,day,day); };
#ifdef POKEFINDER_SEARCH_INSTRUMENTATION
    SearchMetrics::reset(); auto b=runBase(); printCounters("normal_date_search",false,b.size());
    SearchMetrics::reset(); auto o=runOpt(); printCounters("normal_date_search",true,o.size());
    normalize(b);normalize(o);same(b,o,"instrumented date benchmark");
#else
    auto b=runBase(),o=runOpt();normalize(b);normalize(o);same(b,o,"date benchmark all fields");
    check(!b.empty(),"practical normal Wild search nonempty");
    std::vector<double> bt,ot;
    auto timed=[&](auto run){auto began=Clock::now();auto states=run();check(states.size()==b.size(),"timed date count");
        return std::chrono::duration<double,std::milli>(Clock::now()-began).count();};
    for(int i=0;i<5;++i) { if(i%2){ot.push_back(timed(runOpt));bt.push_back(timed(runBase));}else{bt.push_back(timed(runBase));ot.push_back(timed(runOpt));} }
    std::sort(bt.begin(),bt.end());std::sort(ot.begin(),ot.end());
    std::cout<<std::fixed<<std::setprecision(3)<<"SEARCH_TIMING normal_date_search workers=1 seeds=86400 pid_frames=1000 baseline_ms="<<bt[2]
        <<" optimized_ms="<<ot[2]<<" speedup="<<bt[2]/ot[2]<<" baseline_range="<<bt.front()<<':'<<bt.back()
        <<" optimized_range="<<ot.front()<<':'<<ot.back()<<" accepted="<<b.size()<<std::endl;
    const auto &first=b.front(); const auto &s=first.getState();
    std::cout<<"MANUAL_FIRST seed="<<std::hex<<first.getInitialSeed()<<std::dec<<" advances="<<s.getAdvances()<<" pid="<<s.getPID()
        <<" species="<<s.getSpecie()<<" slot="<<int(s.getEncounterSlot())<<" HP="<<int(s.getHiddenPower())<<" IVs=";
    for(auto v:s.getIVs()) std::cout<<int(v)<<',';std::cout<<'\n';
#endif
}

int main(int argc,char **argv)
{
    QCoreApplication app(argc,argv);
    SearchOptimization::setPruningEnabled(true);
    try
    {
        std::string mode=argc>1?argv[1]:"--verify";
        if(mode=="--benchmark") benchmark();
        else if(mode=="--search-benchmark") searchBenchmark();
        else {equivalence();searchEquivalence();std::cout<<"TOTAL comparisons="<<comparisons<<" records="<<records<<" PASS\n";}
        std::cout<<"PASS\n"; return 0;
    }
    catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
}
