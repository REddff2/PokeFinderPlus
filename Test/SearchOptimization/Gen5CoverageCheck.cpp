#include "coverage-baseline/CoverageBaselineStaticGenerator5.hpp"
#include "coverage-baseline/CoverageBaselineEggGenerator5.hpp"
#include "coverage-baseline/CoverageBaselineDreamRadarGenerator.hpp"
#include "coverage-baseline/CoverageBaselineHiddenGrottoGenerator.hpp"
#include "coverage-baseline/CoverageBaselinePickupGenerator.hpp"
#include "coverage-baseline/CoverageBaselineAdjacentSeedsCalculator.hpp"
#include <Core/Enum/Buttons.hpp>
#include <Core/Enum/Encounter.hpp>
#include <Core/Enum/Game.hpp>
#include <Core/Enum/Method.hpp>
#include <Core/Gen5/Encounters5.hpp>
#include <Core/Gen5/Generators/StaticGenerator5.hpp>
#include <Core/Gen5/Generators/EggGenerator5.hpp>
#include <Core/Gen5/Generators/DreamRadarGenerator.hpp>
#include <Core/Gen5/Generators/HiddenGrottoGenerator.hpp>
#include <Core/Gen5/Generators/PickupGenerator.hpp>
#include <Core/Gen5/Generators/IDGenerator5.hpp>
#include <Core/Gen5/Generators/PhenomenonGenerator.hpp>
#include <Core/Gen5/Generators/IVRNG.hpp>
#include <Core/Gen5/Searchers/IVCacheSearcher.hpp>
#include <Core/Gen5/Searchers/Searcher5.hpp>
#include <Core/Gen5/Searchers/StaticSearcher5.hpp>
#include <Core/Gen5/Searchers/HiddenGrottoSearcher.hpp>
#include <Core/Gen5/States/EggState5.hpp>
#include <Core/Gen5/States/DreamRadarState.hpp>
#include <Core/Gen5/States/HiddenGrottoState.hpp>
#include <Core/Gen5/States/PickupState.hpp>
#include <Core/Gen5/States/PhenomenonState.hpp>
#include <Core/Parents/States/IDState.hpp>
#include <Core/Gen5/Tools/AdjacentSeedsCalculator.hpp>
#include <Core/Util/SearchOptimization.hpp>
#include <Core/Enum/DSType.hpp>
#include <Core/Enum/Language.hpp>
#include <QCoreApplication>
#undef slots
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <thread>
using json=nlohmann::json;
using Clock=std::chrono::steady_clock;
using IVs=std::array<u8,6>;
static std::map<std::string,std::pair<u64,u64>> totals;
static void require(bool pass,const std::string &message){if(!pass)throw std::runtime_error(message);}
static u64 seedAt(u64 i){i+=0x9e3779b97f4a7c15ULL;i=(i^(i>>30))*0xbf58476d1ce4e5b9ULL;i=(i^(i>>27))*0x94d049bb133111ebULL;return i^(i>>31);}
static void mode(int m){SearchOptimization::setPruningEnabled(m!=0);SearchOptimization::setGpuEnabled(m==2);}
static Profile5 profile(Game game=Game::Black2,bool memory=false,bool charm=false){return {"Coverage",game,12345,54321,"","",0x001122334455ULL,{true,false,false,false,false,false,false,false,false},0x60,6,6,false,0x1100,0x1100,memory,charm,DSType::DS,Language::English};}
struct Filters{
    IVs low{},high{31,31,31,31,31,31};std::array<bool,25> natures;std::array<bool,16> powers;std::array<bool,13> slots;
    u8 ability=255,gender=255,shiny=255,levelMin=1,levelMax=100;bool skip=false;
    Filters(){natures.fill(true);powers.fill(true);slots.fill(true);}
    StateFilter state()const{return {gender,ability,shiny,levelMin,levelMax,0,255,0,255,skip,low,high,natures,powers};}
    WildStateFilter wild()const{return {gender,ability,shiny,levelMin,levelMax,0,255,0,255,skip,low,high,natures,powers,slots};}
    void vary(int i){
        if(i%7==1)low.fill(15);
        if(i%7==2){powers.fill(false);powers[(i/7)%16]=true;}
        if(i%7==3){natures.fill(false);natures[i%25]=true;ability=i%3;gender=i%3;}
        if(i%7==4){low={0,31,0,25,0,31};high={10,31,31,31,7,31};}
        if(i%7==5){low.fill(31);powers.fill(false);natures.fill(false);skip=true;}
        if(i%7==6){shiny=3;levelMin=20;levelMax=30;}
    }
};
template<class S> static json record(const S&s){json j=json::object();
#define FIELD(method) if constexpr(requires{s.method();})j[#method]=s.method()
    FIELD(getEC);FIELD(getPID);FIELD(getStats);FIELD(getAbilityIndex);FIELD(getIVs);FIELD(getAbility);FIELD(getCharacteristic);
    FIELD(getGender);FIELD(getHiddenPower);FIELD(getHiddenPowerStrength);FIELD(getLevel);FIELD(getNature);FIELD(getShiny);
    FIELD(getAdvances);FIELD(getIVAdvances);FIELD(getPassPower);FIELD(getLead);FIELD(getLeadMask);FIELD(getChatot);FIELD(getNeedle);
    FIELD(getSynchronize);FIELD(getInheritance);FIELD(getEncounterSlot);FIELD(getSpecie);FIELD(getForm);FIELD(getItem);
    FIELD(getMovingTrigger);FIELD(getMovingSteps);FIELD(getPhenomenon);FIELD(getPhenomenonItem);FIELD(isValid);FIELD(getVariableNature);FIELD(getLeadRequired);
    FIELD(getGroup);FIELD(getSlot);FIELD(getAmount);FIELD(getItemAdvances);FIELD(getTID);FIELD(getSID);FIELD(getTSV);
    FIELD(getSeed);FIELD(getButtons);FIELD(getTimer0);FIELD(getIVAdvance);FIELD(getPIDAdvance);FIELD(isTarget);
#undef FIELD
    if constexpr(requires{s.getData();})
    {
        // The unchanged Phenomenon encounter row has no initialized item data.
        if constexpr(std::is_same_v<S,PhenomenonState>) { if(s.getItem())j["getData"]=s.getData(); }
        else j["getData"]=s.getData();
    }
    if constexpr(requires{s.getDateTime();})j["datetime"]=s.getDateTime().toString();
    if constexpr(std::is_same_v<S,WildState5>)j["baseValid"]=static_cast<const WildGeneratorState&>(s).isValid();
    if constexpr(std::is_same_v<S,AdjacentSeedsState>)for(u8 i=0;i<6;++i)j["ivs"].push_back(s.getIV(i));
    if constexpr(std::is_same_v<S,PickupState>){for(u8 i=0;i<6;++i){j["active"].push_back(s.getActive(i));j["items"].push_back(s.getItem(i));}j["wild"]=s.getWild()?record(*s.getWild()):json(nullptr);}
    return j;
}
template<class S>static json record(const SearcherState5<S>&s){return {{"seed",s.getInitialSeed()},{"date",s.getDateTime().toString()},{"timer",s.getTimer0()},{"buttons",s.getButtons()},{"state",record(s.getState())}};}
template<class S>static void same(const std::vector<S>&a,const std::vector<S>&b,const std::string&name){
    require(a.size()==b.size(),name+" count "+std::to_string(a.size())+" vs "+std::to_string(b.size()));
    for(size_t i=0;i<a.size();++i)require(record(a[i])==record(b[i]),name+" fields/order at "+std::to_string(i));
    ++totals[name].first;totals[name].second+=a.size();
}
template<class S,class F>static void modes(const std::string&name,const std::vector<S>&expected,F generate){for(int m=0;m<3;++m){mode(m);same(expected,generate(),name);}}
static Daycare daycare(int i){const u16 species[]={1,29,32,313,314,599,133};std::array<IVs,2> parents{IVs{31,0,31,0,31,0},IVs{0,31,0,31,0,31}};return {parents,{u8(i%3),u8((i/3)%3)},{u8(i%5==0?3:0),1},{u8(i%8),u8((i/8)%8)},{u8(i%25),u8((i/25)%25)},species[(i/5)%7],i%2==0};}
static Daycare benchmarkDaycare(){return {std::array<IVs,2>{IVs{31,31,31,31,31,31},IVs{31,31,31,31,31,31}},{0,2},{0,1},{0,0},{0,1},133,true};}
static HiddenGrottoFilter slotFilter(int i=0){std::array<bool,11> slots;slots.fill(true);if(i%3)slots[(i/3)%11]=false;return {slots,{true,i%4!=0},{true,true,i%5!=0,true}};}
static std::vector<StaticTemplate5> staticTemplates(){std::vector<StaticTemplate5> out;for(int type=0;type<9;++type){int n=0;auto ptr=Encounters5::getStaticEncounters(type,&n);for(int i=0;i<n;++i)out.push_back(ptr[i]);}return out;}

static void ivProof(){
    for(u32 start:{0u,1u,2u,7u,8u,15u,16u,31u,32u,63u,64u,127u,128u,192u,218u,219u,223u,224u,225u,623u,624u,1000u})
    for(u32 count:{0u,1u,2u,31u,64u})for(u32 stride:{1u,2u})for(u32 i=0;i<32;++i){
        Gen5::IVRNG fast(seedAt(i),start,u64(count)*stride+28,true);RNGList<u8,MT,32,Gen5::mtIV> baseline(seedAt(i),start);
        for(u32 frame=0;frame<=count;++frame,fast.advanceStates(stride),baseline.advanceStates(stride)){
            for(int k=0;k<6;++k)require(fast.next()==baseline.next(),"MT sliding window");
            fast.resetState();baseline.resetState();fast.advance(22);baseline.advance(22);
            for(int k=0;k<6;++k)require(fast.next()==baseline.next(),"MT cache lookahead");
        }
    }std::cout<<"MT boundary/stride/reset proof passed\n";
}
static void equivalence(){
    ivProof();const Game games[]={Game::Black,Game::White,Game::Black2,Game::White2};auto templates=staticTemplates();auto grottos=Encounters5::getHiddenGrottoEncounters();int radarCount=0;auto radar=Encounters5::getDreamRadarEncounters(&radarCount);
    const u32 ivStarts[]={0,1,2,7,31,63,127,216,218,219,224,623,624,1000};
    for(int i=0;i<336;++i){
        auto seed=seedAt(i);auto p=profile(games[i%4],i%3==0,i%5==0);auto p2=profile(games[2+i%2],i%3==0,i%5==0);Filters f;f.vary(i);
        u32 count=i%19,initial=i%13,offset=i%11,ivStart=ivStarts[(i/4)%std::size(ivStarts)],ivCount=i%4;const auto&t=templates[i%templates.size()];
        std::vector<Lead> leads=i%3?std::vector<Lead>{Lead::None}:std::vector<Lead>{Lead::None,Lead::Synchronize,Lead::CuteCharmM,Lead::CuteCharmF,static_cast<Lead>(24)};
        std::vector<u8> powers=i%2?std::vector<u8>{0}:std::vector<u8>{0,1,2,3};
        auto staticBase=[&](const Filters&v){return CoverageBaselineStaticGenerator5(initial,count,offset,Method::Method5,leads,powers,t,p,v.state()).generate(seed,ivStart,ivCount);};
        modes("Static",staticBase(f),[&]{return StaticGenerator5(initial,count,offset,Method::Method5,leads,powers,t,p,f.state()).generate(seed,ivStart,ivCount);});
        if(i%7==0){auto broad=staticBase(Filters{});require(!broad.empty(),"Static exact fixture");Filters exact;exact.low=exact.high=broad.front().getIVs();auto expected=staticBase(exact);require(!expected.empty(),"Static exact positive");
            modes("Static exact",expected,[&]{return StaticGenerator5(initial,count,offset,Method::Method5,leads,powers,t,p,exact.state()).generate(seed,ivStart,ivCount);});
            std::vector<std::pair<u32,IVs>> cached={{ivStart,exact.low},{ivStart,exact.low},{ivStart+2,exact.low}};
            auto oracle=CoverageBaselineStaticGenerator5(initial,count,offset,Method::Method5,leads,powers,t,p,f.state()).generate(seed,cached);
            modes("Static cached duplicates",oracle,[&]{return StaticGenerator5(initial,count,offset,Method::Method5,leads,powers,t,p,f.state()).generate(seed,cached);});}
        auto dc=daycare(i);auto eggBase=[&](const Filters&v){return CoverageBaselineEggGenerator5(initial,count,offset,dc,p,v.state()).generate(seed);};std::string eggName=i%4<2?"Egg BW":"Egg B2W2";
        modes(eggName,eggBase(f),[&]{return EggGenerator5(initial,count,offset,dc,p,f.state()).generate(seed);});
        if(i%7==0){auto broad=eggBase(Filters{});require(!broad.empty(),"Egg fixture");Filters exact;exact.low=exact.high=broad.front().getIVs();auto expected=eggBase(exact);require(!expected.empty(),"Egg inherited exact positive");modes(eggName+" exact",expected,[&]{return EggGenerator5(initial,count,offset,dc,p,exact.state()).generate(seed);});}
        std::vector<DreamRadarTemplate> slots;for(int j=0;j<=i%6;++j)slots.push_back(radar[(i+j)%radarCount]);u32 radarInitial=ivStarts[(i/3)%std::size(ivStarts)]/2,radarMax=i%11==0?128:count;
        auto radarBase=[&](const Filters&v){return CoverageBaselineDreamRadarGenerator(radarInitial,radarMax,i%9,slots,p2,v.state()).generate(seed);};
        modes("Dream Radar",radarBase(f),[&]{return DreamRadarGenerator(radarInitial,radarMax,i%9,slots,p2,f.state()).generate(seed);});
        if(i%7==0){auto broad=radarBase(Filters{});require(!broad.empty(),"Radar fixture");Filters exact;exact.low=exact.high=broad.back().getIVs();auto expected=radarBase(exact);require(!expected.empty(),"Radar later-frame positive");modes("Dream Radar exact",expected,[&]{return DreamRadarGenerator(radarInitial,radarMax,i%9,slots,p2,exact.state()).generate(seed);});}
        auto grotto=grottos[i%grottos.size()];auto slot=grotto.getPokemon(i%4,i%3,p2.getVersion());auto lead=i%2?Lead::None:Lead::Synchronize;
        auto grottoBase=[&](const Filters&v){return CoverageBaselineHiddenGrottoGenerator(initial,count,offset,lead,i%2,slot,p2,v.state()).generate(seed,ivStart,ivCount);};
        modes("Grotto Pokemon",grottoBase(f),[&]{return HiddenGrottoGenerator(initial,count,offset,lead,i%2,slot,p2,f.state()).generate(seed,ivStart,ivCount);});
        if(i%7==0){auto broad=grottoBase(Filters{});require(!broad.empty(),"Grotto fixture");Filters exact;exact.low=exact.high=broad.front().getIVs();auto expected=grottoBase(exact);require(!expected.empty(),"Grotto exact positive");modes("Grotto Pokemon exact",expected,[&]{return HiddenGrottoGenerator(initial,count,offset,lead,i%2,slot,p2,exact.state()).generate(seed,ivStart,ivCount);});
            std::vector<std::pair<u32,IVs>> cached={{ivStart,exact.low},{ivStart,exact.low},{ivStart+2,exact.low}};auto oracle=CoverageBaselineHiddenGrottoGenerator(initial,count,offset,lead,i%2,slot,p2,f.state()).generate(seed,cached);
            modes("Grotto cached duplicates",oracle,[&]{return HiddenGrottoGenerator(initial,count,offset,lead,i%2,slot,p2,f.state()).generate(seed,cached);});}
        std::vector<PassPower> grottoPowers={PassPower::None,PassPower::Level1,PassPower::Level2,PassPower::Level3,PassPower::LevelS};u16 item=i%2?0:grotto.getItem(i%4,0);u8 amount=i%3==0?2:1;
        auto slotBase=CoverageBaselineHiddenGrottoSlotGenerator(initial,127,offset,grottoPowers,grotto,p2,slotFilter(i),item,amount).generate(seed);
        modes("Grotto slots",slotBase,[&]{return HiddenGrottoSlotGenerator(initial,127,offset,grottoPowers,grotto,p2,slotFilter(i),item,amount).generate(seed);});
        auto areas=Encounters5::getEncounters(i%3==0?Encounter::GrassDark:Encounter::Grass,{false,u8(i%4)},&p);auto area=areas[i%areas.size()];std::array<PickupGenerator::Slot,6> pickups;std::array<CoverageBaselinePickupGenerator::Slot,6> basePickups;
        for(int j=0;j<6;++j){bool active=(i+j)%3!=0;u8 level=1+(i+j*17)%100;std::vector<u16> items=i%5==0?PickupGenerator::getLevelItems(level):std::vector<u16>{};pickups[j]={active,level,items};basePickups[j]={active,level,items};}
        auto pickupBase=CoverageBaselinePickupGenerator(initial,count,basePickups,area,p,f.wild(),i%2).generate(seed,ivStart);
        modes("Pickup",pickupBase,[&]{return PickupGenerator(initial,count,pickups,area,p,f.wild(),i%2).generate(seed,ivStart);});
        DateTime date(2026,6,15,12,0,0);auto adjacent=CoverageBaselineAdjacentSeedsCalculator::generate(ivStart,ivStart+ivCount,2,i%2,Buttons::None,date,f.low,f.high,p);
        modes("Adjacent seeds",adjacent,[&]{return AdjacentSeedsCalculator::generate(ivStart,ivStart+ivCount,2,i%2,Buttons::None,date,f.low,f.high,p);});
    }std::cout<<"Generator and cache-input equivalence passed\n";
}

template<class S>static auto collect(S&s,int threads=2){
    Date start(2026,6,15),end(2026,6,16);s.setMaxProgress(s.getMaxProgress(start,end));s.startSearch(threads,start,end);auto rows=s.getResults();auto deadline=Clock::now()+std::chrono::seconds(120);
    while(s.isSearching()){if(Clock::now()>deadline){s.cancelSearch();throw std::runtime_error("search timeout");}auto batch=s.getResults();rows.insert(rows.end(),batch.begin(),batch.end());std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    auto batch=s.getResults();rows.insert(rows.end(),batch.begin(),batch.end());require(s.getProgress()==100,"search domain complete");require(s.getWorkerCount()==size_t(std::min(threads,2)),"worker count");
    std::sort(rows.begin(),rows.end(),[](const auto&a,const auto&b){return std::tuple(a.getDateTime(),a.getTimer0(),a.getButtons(),a.getState().getAdvances())<std::tuple(b.getDateTime(),b.getTimer0(),b.getButtons(),b.getState().getAdvances());});return rows;
}
static void searches(){
    Filters f;f.low.fill(15);auto p=profile();auto t=*Encounters5::getStaticEncounter(0,0);mode(0);StaticGenerator5 b(0,0,0,Method::Method5,Lead::None,0,t,p,f.state());StaticSearcher5 base(0,0,b,p);auto expected=collect(base,1);require(!expected.empty(),"Static date positives");
    for(int m=1;m<3;++m){mode(m);StaticGenerator5 g(0,0,0,Method::Method5,Lead::None,0,t,p,f.state());StaticSearcher5 s(0,0,g,p);same(expected,collect(s),"Static date threads");}
    auto dc=benchmarkDaycare();for(Game game:{Game::Black,Game::Black2}){p=profile(game);mode(0);EggGenerator5 g(0,0,0,dc,p,f.state());Searcher5<EggGenerator5,EggState5> original(g,p);auto rows=collect(original,1);for(int m=1;m<3;++m){mode(m);EggGenerator5 opt(0,0,0,dc,p,f.state());Searcher5<EggGenerator5,EggState5>s(opt,p);same(rows,collect(s),"Egg date threads");}}
    p=profile();std::vector<DreamRadarTemplate> radar{*Encounters5::getDreamRadarEncounters(0)};mode(0);DreamRadarGenerator rg(0,0,8,radar,p,f.state());Searcher5<DreamRadarGenerator,DreamRadarState>rs(rg,p);auto radarRows=collect(rs,1);require(!radarRows.empty(),"Radar date positives");
    for(int m=1;m<3;++m){mode(m);DreamRadarGenerator g(0,0,8,radar,p,f.state());Searcher5<DreamRadarGenerator,DreamRadarState>s(g,p);same(radarRows,collect(s),"Radar date threads");}
    auto area=Encounters5::getHiddenGrottoEncounters().front();auto slot=area.getPokemon(0,0,p.getVersion());mode(0);HiddenGrottoGenerator hg(0,0,0,Lead::None,0,slot,p,f.state());HiddenGrottoIVSearcher hs(0,0,hg,p);auto grottoRows=collect(hs,1);require(!grottoRows.empty(),"Grotto date positives");
    for(int m=1;m<3;++m){mode(m);HiddenGrottoGenerator g(0,0,0,Lead::None,0,slot,p,f.state());HiddenGrottoIVSearcher s(0,0,g,p);same(grottoRows,collect(s),"Grotto date threads");}
    mode(2);StaticSearcher5 cancel(0,0,b,p);cancel.startSearch(2,Date(2000,1,1),Date(2099,12,31));cancel.cancelSearch();while(cancel.isSearching())std::this_thread::sleep_for(std::chrono::milliseconds(1));require(cancel.isCancelled(),"CPU cancellation");std::cout<<"Date search metadata/multiplicity, threads and cancellation passed\n";
}
template<class F,class G>static json benchmark(const std::string&name,F baseline,G smart,u32 seeds){
    for(u32 i=0;i<seeds;++i)same(baseline(seedAt(i)),smart(seedAt(i)),name+" timed inputs");
    std::array<double,5>a,b;u64 countA=0,countB=0;u32 rounds=1;auto measure=[&](auto&fn,u64&count){auto start=Clock::now();count=0;for(u32 round=0;round<rounds;++round)for(u32 i=0;i<seeds;++i)count+=fn(seedAt(i)).size();count/=rounds;return std::chrono::duration<double,std::milli>(Clock::now()-start).count()/rounds;};
    const double warm=measure(baseline,countA);measure(smart,countB);rounds=std::clamp<u32>(u32(50/std::max(0.001,warm)),1,512);for(int i=0;i<5;++i){if(i%2){b[i]=measure(smart,countB);a[i]=measure(baseline,countA);}else{a[i]=measure(baseline,countA);b[i]=measure(smart,countB);}}
    require(countA==countB,name+" timed counts");std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());json row={{"family",name},{"seeds",seeds},{"rounds_per_sample",rounds},{"results",countA},{"baseline_ms",a[2]},{"smart_ms",b[2]},{"speedup",a[2]/b[2]},{"baseline_samples_ms",a},{"smart_samples_ms",b}};std::cout<<row.dump()<<std::endl;return row;
}
static json benchmarks(){
    mode(1);json rows=json::array();Filters broad,iv,nature;iv.low.fill(15);nature.natures.fill(false);nature.natures[0]=true;auto p=profile();auto t=*Encounters5::getStaticEncounter(0,0);auto dc=benchmarkDaycare();
    for(const auto&entry:std::vector<std::pair<std::string,Filters>>{{"broad",broad},{"IV15",iv},{"nature",nature}}){auto f=entry.second;
        CoverageBaselineStaticGenerator5 sb(0,31,0,Method::Method5,Lead::None,0,t,p,f.state());StaticGenerator5 sg(0,31,0,Method::Method5,Lead::None,0,t,p,f.state());rows.push_back(benchmark("Static "+entry.first,[&](u64 s){return sb.generate(s,0,0);},[&](u64 s){return sg.generate(s,0,0);},1024));
        for(Game game:{Game::Black,Game::Black2}){auto ep=profile(game);CoverageBaselineEggGenerator5 eb(0,127,0,dc,ep,f.state());EggGenerator5 eg(0,127,0,dc,ep,f.state());rows.push_back(benchmark(std::string(game==Game::Black?"Egg BW ":"Egg B2W2 ")+entry.first,[&](u64 s){return eb.generate(s);},[&](u64 s){return eg.generate(s);},128));}
        std::vector<DreamRadarTemplate> radar{*Encounters5::getDreamRadarEncounters(0)};CoverageBaselineDreamRadarGenerator rb(0,63,8,radar,p,f.state());DreamRadarGenerator rg(0,63,8,radar,p,f.state());rows.push_back(benchmark("Dream Radar "+entry.first,[&](u64 s){return rb.generate(s);},[&](u64 s){return rg.generate(s);},256));
        auto slot=Encounters5::getHiddenGrottoEncounters().front().getPokemon(0,0,p.getVersion());CoverageBaselineHiddenGrottoGenerator gb(0,31,0,Lead::None,0,slot,p,f.state());HiddenGrottoGenerator gg(0,31,0,Lead::None,0,slot,p,f.state());rows.push_back(benchmark("Grotto Pokemon "+entry.first,[&](u64 s){return gb.generate(s,0,0);},[&](u64 s){return gg.generate(s,0,0);},1024));
        auto area=Encounters5::getEncounters(Encounter::Grass,{false,0},&p).front();std::array<PickupGenerator::Slot,6>slots;std::array<CoverageBaselinePickupGenerator::Slot,6>oldSlots;for(int j=0;j<6;++j){slots[j]={true,100,{}};oldSlots[j]={true,100,{}};}
        CoverageBaselinePickupGenerator pb(0,127,oldSlots,area,p,f.wild());PickupGenerator pg(0,127,slots,area,p,f.wild());rows.push_back(benchmark("Pickup "+entry.first,[&](u64 s){return pb.generate(s);},[&](u64 s){return pg.generate(s);},64));
    }
    auto ga=Encounters5::getHiddenGrottoEncounters().front();std::vector<PassPower>powers={PassPower::None,PassPower::Level1,PassPower::Level2,PassPower::Level3};CoverageBaselineHiddenGrottoSlotGenerator gb(0,1023,0,powers,ga,p,slotFilter());HiddenGrottoSlotGenerator gg(0,1023,0,powers,ga,p,slotFilter());rows.push_back(benchmark("Grotto slots",[&](u64 s){return gb.generate(s);},[&](u64 s){return gg.generate(s);},32));
    auto adjacent=[&](u64 seed,bool old){DateTime date(2026,6,15,12,0,seed%60);return old?CoverageBaselineAdjacentSeedsCalculator::generate(0,3,3,false,Buttons::None,date,broad.low,broad.high,p):AdjacentSeedsCalculator::generate(0,3,3,false,Buttons::None,date,broad.low,broad.high,p);};rows.push_back(benchmark("Adjacent seeds",[&](u64 s){return adjacent(s,true);},[&](u64 s){return adjacent(s,false);},128));return rows;
}
int main(int argc,char**argv){QCoreApplication app(argc,argv);try{
    require(!SearchOptimization::pruningEnabled()&&!SearchOptimization::gpuEnabled(),"defaults OFF/OFF");json report;
    if(argc>1&&std::string(argv[1])=="--benchmark")report["benchmarks"]=benchmarks();else{equivalence();searches();}
    for(const auto&[name,value]:totals)report["checks"][name]={{"comparisons",value.first},{"records",value.second}};report["passed"]=true;
    if(argc>2){std::ofstream output(argv[2]);output<<report.dump(2)<<'\n';}std::cout<<report.dump(2)<<"\nGEN5_COVERAGE_PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<"GEN5_COVERAGE_FAIL "<<e.what()<<'\n';return 1;}}
