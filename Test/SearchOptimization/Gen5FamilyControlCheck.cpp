#define main coverageMain
#include "Gen5CoverageCheck.cpp"
#undef main
#include "BaselineEventGenerator5.hpp"
#include "BaselineWildGenerator5.hpp"
#include <Core/Gen5/Generators/EventGenerator5.hpp>
#include <Core/Gen5/Generators/WildGenerator5.hpp>
#include <Core/Gen5/Searchers/SHA1CacheSearcher.hpp>
#include <Core/Gen5/IVCache.hpp>
#include <Core/Gen5/PGF.hpp>
#include <Core/Gen5/States/EventState5.hpp>

using Family = SearchOptimization::Family;
static void preferences(unsigned mask)
{
    for (size_t i = 0; i < SearchOptimization::FamilyCount; ++i)
        SearchOptimization::setFamilyEnabled(static_cast<Family>(i), mask & (1u << i));
}
static void generators()
{
    auto p = profile();
    Filters f; f.low.fill(15);
    auto t = *Encounters5::getStaticEncounter(0,0);
    auto slot = Encounters5::getHiddenGrottoEncounters().front().getPokemon(0,0,p.getVersion());
    auto ga = Encounters5::getHiddenGrottoEncounters().front();
    auto area = Encounters5::getEncounters(Encounter::Grass,{false,0},&p).front();
    auto dc = benchmarkDaycare();
    std::vector<DreamRadarTemplate> radar{*Encounters5::getDreamRadarEncounters(0)};
    std::array<PickupGenerator::Slot,6> pickups;
    std::array<CoverageBaselinePickupGenerator::Slot,6> oldPickups;
    for (int i=0;i<6;++i) { pickups[i]={true,100,{}}; oldPickups[i]={true,100,{}}; }
    PGF gift(0,0,519,0,1,1,Shiny::Never,1,255,31,255,255,255,255,true);
    std::vector<unsigned> masks{0,511};
    for (unsigned i=0;i<9;++i) { masks.push_back(1u<<i); masks.push_back(511^(1u<<i)); }
    for (unsigned mask : masks) for (int m=0;m<3;++m)
    {
        preferences(mask);mode(m);
        auto enabled=[](Family family){return SearchOptimization::pruningEnabled(family);};
        StaticGenerator5 sg(0,15,0,Method::Method5,Lead::None,0,t,p,f.state());
        HiddenGrottoGenerator hg(0,15,0,Lead::None,0,slot,p,f.state());
        HiddenGrottoSlotGenerator gs(0,31,0,{PassPower::None,PassPower::Level1},ga,p,slotFilter());
        EggGenerator5 eg(0,15,0,dc,p,f.state());
        DreamRadarGenerator rg(0,15,8,radar,p,f.state());
        PickupGenerator pg(0,15,pickups,area,p,f.wild());
        EventGenerator5 event(0,15,0,gift,p,f.state(),true);
        WildGenerator5 wild(0,15,0,Method::Method5,std::vector<Lead>{Lead::None},std::vector<u8>{0},false,false,area,p,f.wild(),false,true,true);
        require(sg.optimizedPruningEnabled()==enabled(Family::Static),"Static family snapshot");
        require(hg.optimizedPruningEnabled()==enabled(Family::HiddenGrotto)&&gs.optimizedPruningEnabled()==enabled(Family::HiddenGrotto),"both Grotto paths share owner");
        require(eg.optimizedPruningEnabled()==enabled(Family::Eggs),"Eggs family snapshot");
        require(rg.optimizedPruningEnabled()==enabled(Family::DreamRadar),"Radar family snapshot");
        require(pg.optimizedPruningEnabled()==enabled(Family::Pickup),"Pickup family snapshot");
        require(event.optimizedPruningEnabled()==enabled(Family::Event),"Event explicit true cannot bypass policy");
        require(wild.optimizedPruningEnabled()==enabled(Family::Wild),"Wild explicit true cannot bypass policy");
        Searcher5<EventGenerator5,EventState5> es(event,p);
        Searcher5<EggGenerator5,EggState5> eggs(eg,p);
        Searcher5<HiddenGrottoSlotGenerator,HiddenGrottoState> slots(gs,p);
        IDGenerator5 id(0,0,0,false,false,p,IDFilter({},{},{},{},{},{}));
        Searcher5<IDGenerator5,IDState> ids(id,p);
        require(es.optimizedResultsEnabled()==enabled(Family::Event)&&eggs.optimizedResultsEnabled()==enabled(Family::Eggs)
            &&slots.optimizedResultsEnabled()==enabled(Family::HiddenGrotto),"shared result accumulation has family owner");
        require(!ids.optimizedResultsEnabled(),"unlisted ID family stays baseline");
        IVCacheSearcher cache(0,0);require(cache.optimizedPruningEnabled()==enabled(Family::CacheBuilders),"IV cache owner");
        for (u32 i=0;i<32;++i)
        {
            u64 seed=seedAt(i);
            same(CoverageBaselineStaticGenerator5(0,15,0,Method::Method5,Lead::None,0,t,p,f.state()).generate(seed,0,2),sg.generate(seed,0,2),"Static family result");
            same(CoverageBaselineHiddenGrottoGenerator(0,15,0,Lead::None,0,slot,p,f.state()).generate(seed,0,2),hg.generate(seed,0,2),"Grotto Pokemon family result");
            same(CoverageBaselineHiddenGrottoSlotGenerator(0,31,0,{PassPower::None,PassPower::Level1},ga,p,slotFilter()).generate(seed),gs.generate(seed),"Grotto slots family result");
            same(CoverageBaselineEggGenerator5(0,15,0,dc,p,f.state()).generate(seed),eg.generate(seed),"B2W2 Egg family result");
            auto bw=profile(Game::Black);EggGenerator5 bwEgg(0,15,0,dc,bw,f.state());
            require(bwEgg.optimizedPruningEnabled()==enabled(Family::Eggs),"BW Egg family snapshot");
            same(CoverageBaselineEggGenerator5(0,15,0,dc,bw,f.state()).generate(seed),bwEgg.generate(seed),"BW Egg family result");
            same(CoverageBaselineDreamRadarGenerator(0,15,8,radar,p,f.state()).generate(seed),rg.generate(seed),"Radar family result");
            same(CoverageBaselinePickupGenerator(0,15,oldPickups,area,p,f.wild()).generate(seed),pg.generate(seed),"Pickup family result");
            same(BaselineEventGenerator5(0,15,0,gift,p,f.state()).generate(seed),event.generate(seed),"Event family result");
            same(BaselineWildGenerator5(0,15,0,Method::Method5,std::vector<Lead>{Lead::None},std::vector<u8>{0},false,false,area,p,f.wild(),false,true).generate(seed,0,2),wild.generate(seed,0,2),"Wild family result");
        }
        DateTime dt(2026,6,15,12,0,0);
        same(CoverageBaselineAdjacentSeedsCalculator::generate(0,3,2,false,Buttons::None,dt,f.low,f.high,p),AdjacentSeedsCalculator::generate(0,3,2,false,Buttons::None,dt,f.low,f.high,p),"Adjacent family result");
    }
}
static void gpuDispatch()
{
    auto p=profile();Filters f;f.low.fill(25);
    auto t=*Encounters5::getStaticEncounter(0,0);
    auto slot=Encounters5::getHiddenGrottoEncounters().front().getPokemon(0,0,p.getVersion());
    auto area=Encounters5::getEncounters(Encounter::Grass,{false,0},&p).front();
    std::vector<DreamRadarTemplate> radar{*Encounters5::getDreamRadarEncounters(0)};
    std::array<PickupGenerator::Slot,6> slots;for(auto &s:slots)s={true,100,{}};
    for (unsigned off=0;off<=9;++off) for (int m=0;m<3;++m)
    {
        preferences(off==9?511:511^(1u<<off));mode(m);
        StaticGenerator5 sg(0,0,0,Method::Method5,Lead::None,0,t,p,f.state());
        HiddenGrottoGenerator hg(0,0,0,Lead::None,0,slot,p,f.state());
        WildGenerator5 wg(0,0,0,Method::Method5,Lead::None,0,false,false,area,p,f.wild());
        DreamRadarGenerator rg(0,0,8,radar,p,f.state());PickupGenerator pg(0,0,slots,area,p,f.wild());
        StaticSearcher5 ss(0,0,sg,p);HiddenGrottoIVSearcher hs(0,0,hg,p);IVSearcher5<WildGenerator5,WildState5> ws(0,0,wg,p);
        Searcher5<DreamRadarGenerator,DreamRadarState> rs(rg,p);Searcher5<PickupGenerator,PickupState> ps(pg,p);IVCacheSearcher cache(0,0);
        auto checkGpu=[](const auto &s,Family family){require(bool(s.getGpuSession())==SearchOptimization::gpuEnabled(family),"GPU dispatch requires only owning family");};
        checkGpu(ss,Family::Static);checkGpu(hs,Family::HiddenGrotto);checkGpu(ws,Family::Wild);checkGpu(rs,Family::DreamRadar);checkGpu(ps,Family::Pickup);checkGpu(cache,Family::CacheBuilders);
        StaticSearcher5 multi(0,1,sg,p);require(!multi.getGpuSession(),"existing multiple-IV guard retained");
    }
    // The transport checks its actual owner, including after a UI policy change.
    std::vector<u64> seeds(1<<20,seedAt(0));std::atomic<bool> cancel=false;
    for (Family family:{Family::Wild,Family::Static,Family::HiddenGrotto,Family::DreamRadar,Family::Pickup,Family::CacheBuilders})
    {
        preferences(511);mode(2);SearchOptimization::setFamilyEnabled(family,false);
        GpuWild::Options fault;fault.fault=L"no-device";GpuWild::Session session(fault);
        GpuWild::IVPlan plan{2,false};
        require(!session.filter(seeds,cancel,{f.low,f.high},plan,family),"disabled family returns baseline replay");
        require(json::parse(session.diagnostics())["initialization_attempts"]==0,"disabled family never initializes GPU");
        preferences(0); // No other family is required for this family's GPU.
        SearchOptimization::setFamilyEnabled(family,true);
        require(!session.filter(seeds,cancel,{f.low,f.high},plan,family),"failed GPU requests Smart CPU replay");
        require(json::parse(session.diagnostics())["initialization_attempts"]==1,"enabled owning family reaches guarded backend");
    }
}
static void cachePolicy(const std::filesystem::path &dir)
{
    auto bytes=[](const auto &p){std::ifstream in(p,std::ios::binary);return std::vector<char>{std::istreambuf_iterator<char>(in),{}};};
    std::vector<char> expected;
    for (int modeIndex=0;modeIndex<3;++modeIndex)
    {
        preferences(511);mode(modeIndex?2:0);SearchOptimization::setFamilyEnabled(Family::CacheBuilders,modeIndex==2);
        IVCacheSearcher cache(0,0);cache.startSearch(2,0,(1<<20)-1);
        while(cache.isSearching())std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto path=dir/("family-cache-"+std::to_string(modeIndex)+".bin");cache.writeResults(path.string());
        if(!modeIndex)expected=bytes(path);else require(bytes(path)==expected,"family-disabled/enabled IV cache exact bytes");
        IVCache loaded(path.string(),true);require(loaded.isValid(),"family cache readable");
        auto p=profile();SHA1CacheSearcher sha(loaded,p,Date(2026,6,15),Date(2026,6,15));
        require(sha.optimizedMembershipEnabled()==(modeIndex==2),"SHA membership belongs to Cache Builders");
    }
}
int main(int argc,char **argv)
{
    QCoreApplication app(argc,argv);
    try
    {
        require(argc==2,"evidence directory required");std::filesystem::path dir=argv[1];std::filesystem::create_directories(dir);
        require(!SearchOptimization::pruningEnabled()&&!SearchOptimization::gpuEnabled(),"clean master/GPU OFF");
        for(unsigned i=0;i<9;++i)require(SearchOptimization::familyEnabled(static_cast<Family>(i)),"clean family preferences ON");
        generators();gpuDispatch();cachePolicy(dir);
        preferences(0x154);mode(2);mode(0);mode(1);
        for(unsigned i=0;i<9;++i)require(SearchOptimization::familyEnabled(static_cast<Family>(i))==bool(0x154&(1u<<i)),"master cycle preserves preferences");
        json report={{"passed",true},{"policy_matrices",60},{"gpu_dispatch_matrices",30}};
        for(const auto &[name,count]:totals)report["comparisons"][name]={{"count",count.first},{"records",count.second}};
        std::ofstream(dir/"family-controls.json")<<report.dump(2)<<'\n';
        std::cout<<report.dump(2)<<"\nFAMILY_CONTROLS_PASS\n";return 0;
    }
    catch(const std::exception &e){std::cerr<<"FAMILY_CONTROLS_FAIL "<<e.what()<<'\n';return 1;}
}
