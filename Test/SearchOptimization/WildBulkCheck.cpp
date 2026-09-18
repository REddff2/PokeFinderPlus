#include "Milestone2WildGenerator5.hpp"
#include "Milestone2WildSearcher5.hpp"
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
    Milestone2WildGenerator5 baseline() const
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


static void equivalence()
{

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


#ifdef POKEFINDER_BULK_PROFILE
#include "BulkMetrics.hpp"
#endif
static void resetMetrics() {
#ifdef POKEFINDER_BULK_PROFILE
    SearchMetrics::reset(); BulkMetrics::reset();
#endif
}
static void printMetrics(const std::string &name, bool live, u64 count) {
#ifdef POKEFINDER_BULK_PROFILE
    std::cout<<"PROFILE "<<name<<" live="<<live<<" states="<<SearchMetrics::statesConstructed<<" accepted="<<count
        <<" pid_frames="<<SearchMetrics::wildPIDFrames<<" iv_candidates="<<SearchMetrics::wildIVCandidates
        <<" iv_checks="<<SearchMetrics::ivChecks<<" early_hp="<<SearchMetrics::earlyHiddenPowerRejected;
    BulkMetrics::print(); std::cout<<std::endl;
#endif
}
template<class Generator> static u64 batch(const Generator &g,const std::vector<u64> &seeds) {
    u64 count=0; for(auto seed:seeds) count+=g.generate(seed,0,7).size(); return count;
}
static void levelAndDuplicateChecks() {
    const Encounter types[]={Encounter::Grass,Encounter::GrassDark,Encounter::Surfing,Encounter::GrassRustling,
        Encounter::SurfingRippling,Encounter::SuperRodRippling,Encounter::SuperRod,Encounter::DustCloud,Encounter::FlyingShadow};
    u64 seed=0;u64 keptItems=0,keptInvalid=0,keptPokemon=0;
    for(auto game:{Game::Black,Game::White,Game::Black2,Game::White2}) for(auto type:types)
    for(auto lead:{Lead::None,Lead::Pressure,Lead::Synchronize,Lead::Static,Lead::CuteCharmF}) for(int moving=0;moving<3;++moving) {
        seed=seed*6364136223846793005ULL+1442695040888963407ULL;
        Config c;c.p=profile(game,true,true,true);c.a=area(c.p,type);c.count=127;c.initial=19;c.offset=7;
        c.leads={lead};c.moving=moving!=0;c.requireMoving=moving==2;c.requiredLeads=false;
        auto unfiltered=c.baseline().generate(seed,0,7);
        auto pokemon=std::ranges::find_if(unfiltered,[](const auto &s){return !s.getPhenomenonItem();});
        c.f.levelMin=c.f.levelMax=pokemon==unfiltered.end()?27:pokemon->getLevel();
        for(int skip=0;skip<2;++skip) {
            c.f.skip=skip;
            auto expected=c.baseline().generate(seed,0,7);same(expected,c.generator().generate(seed,0,7),"level ordinary");
            for(const auto &s:expected){keptItems+=s.getPhenomenonItem();keptInvalid+=!s.isValid();keptPokemon+=!s.getPhenomenonItem();}
            auto ivs=ivList(seed,game,0,7);
            same(c.baseline().generate(seed,ivs),c.generator().generate(seed,ivs),"level supplied ascending");
            std::reverse(ivs.begin(),ivs.end());ivs.push_back(ivs[0]);ivs.emplace_back(ivs[1].first,ivs[3].second);
            same(c.baseline().generate(seed,ivs),c.generator().generate(seed,ivs),"level supplied unsorted duplicate");
        }
    }
    check(keptItems && keptInvalid && keptPokemon,"level branch coverage nonempty");
    std::cout<<"LEVEL_DUPLICATE_EQUIVALENCE comparisons="<<comparisons<<" records="<<records<<" items="<<keptItems
        <<" invalid="<<keptInvalid<<" pokemon="<<keptPokemon<<std::endl;
}
static void benchmark() {
    std::vector<u64> seeds; u64 seed=0;
    for(int i=0;i<128;++i){seeds.push_back(seed);seed=seed*6364136223846793005ULL+1442695040888963407ULL;}
    for(std::string name:{"grass_broad","grass_moderate","grass_hidden_power","surfing_broad","super_rod_broad","grass_level","grass_slot","grass_multi_lead"}) {
        Config c; c.count=999;
        if(name=="grass_moderate") c.f.low.fill(10);
        if(name=="grass_hidden_power") { c.f.powers.fill(false);c.f.powers[13]=true; }
        if(name=="surfing_broad") c.a=area(c.p,Encounter::Surfing,101);
        if(name=="super_rod_broad") c.a=area(c.p,Encounter::SuperRod,101);
        if(name=="grass_level") c.f.levelMin=c.f.levelMax=27;
        if(name=="grass_slot") { c.f.encounterSlots.fill(false);c.f.encounterSlots[0]=true;c.leads={Lead::Pressure}; }
        if(name=="grass_multi_lead") c.leads={Lead::None,Lead::Synchronize,Lead::Static};
        auto base=c.baseline();auto live=c.generator();
        resetMetrics();auto b=batch(base,seeds);printMetrics(name,false,b);
        resetMetrics();auto o=batch(live,seeds);printMetrics(name,true,o);check(b==o,"bulk counts");
#ifndef POKEFINDER_BULK_PROFILE
        std::vector<double> bt,ot;
        auto time=[&](const auto &g){auto start=Clock::now();check(batch(g,seeds)==b,"timed count");return std::chrono::duration<double,std::milli>(Clock::now()-start).count();};
        for(int i=0;i<7;++i) {if(i%2){ot.push_back(time(live));bt.push_back(time(base));}else{bt.push_back(time(base));ot.push_back(time(live));}}
        std::sort(bt.begin(),bt.end());std::sort(ot.begin(),ot.end());
        std::cout<<std::fixed<<std::setprecision(3)<<"TIMING "<<name<<" baseline_ms="<<bt[3]<<" optimized_ms="<<ot[3]<<" speedup="<<bt[3]/ot[3]<<" count="<<b
            <<" baseline_range="<<bt.front()<<':'<<bt.back()<<" optimized_range="<<ot.front()<<':'<<ot.back()<<std::endl;
        for(auto s:seeds) same(base.generate(s,0,7),live.generate(s,0,7),name);
#endif
    }
    std::cout<<"BULK_EQUIVALENCE comparisons="<<comparisons<<" records="<<records<<std::endl;
}
static void dates() {
    Config c;c.count=999;c.f.low.fill(10);c.f.powers.fill(false);c.f.powers[13]=true;
    Date begin(2012,10,7),end(2012,10,10);
    for(int workers:{1,4}) {
        auto runBase=[&]{Milestone2WildSearcher5<Milestone2WildGenerator5,WildState5> s(0,0,c.baseline(),c.p);return collect(s,begin,end,workers);};
        auto runLive=[&]{WildSearcher5 s(0,0,c.generator(),c.p);return collect(s,begin,end,workers);};
        resetMetrics();auto b=runBase();printMetrics("date_w"+std::to_string(workers),false,b.size());
        resetMetrics();auto o=runLive();printMetrics("date_w"+std::to_string(workers),true,o.size());
        auto sortStart=Clock::now();normalize(b);normalize(o);
        double sortMs=std::chrono::duration<double,std::milli>(Clock::now()-sortStart).count();
        same(b,o,"date all fields w"+std::to_string(workers));auto count=b.size();b.clear();b.shrink_to_fit();o.clear();o.shrink_to_fit();
#ifndef POKEFINDER_BULK_PROFILE
        std::vector<double> bt,ot;
        auto time=[&](auto run){auto start=Clock::now();auto rows=run();check(rows.size()==count,"date timed count");return std::chrono::duration<double,std::milli>(Clock::now()-start).count();};
        for(int i=0;i<5;++i){if(i%2){ot.push_back(time(runLive));bt.push_back(time(runBase));}else{bt.push_back(time(runBase));ot.push_back(time(runLive));}}
        std::sort(bt.begin(),bt.end());std::sort(ot.begin(),ot.end());
        std::cout<<std::fixed<<std::setprecision(3)<<"DATE workers="<<workers<<" seeds=345600 baseline_ms="<<bt[2]<<" optimized_ms="<<ot[2]<<" count="<<count
            <<" baseline_range="<<bt.front()<<':'<<bt.back()<<" optimized_range="<<ot.front()<<':'<<ot.back()<<" equivalence_sort_both_ms="<<sortMs<<std::endl;
#endif
    }
    std::cout<<"DATE_EQUIVALENCE comparisons="<<comparisons<<" records="<<records<<std::endl;
}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    SearchOptimization::setPruningEnabled(true);
    try {
        std::string mode=argc>1?argv[1]:"--verify";
        if(mode=="--benchmark") benchmark();else if(mode=="--dates") dates();else {equivalence();levelAndDuplicateChecks();benchmark();}
        std::cout<<"PASS comparisons="<<comparisons<<" records="<<records<<std::endl;return 0;
    } catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
}
