#include "BaselineEventGenerator5.hpp"
#include <Core/Enum/DSType.hpp>
#include <Core/Enum/Game.hpp>
#include <Core/Enum/Language.hpp>
#include <Core/Enum/Shiny.hpp>
#include <Core/Gen5/Generators/EventGenerator5.hpp>
#include <Core/Gen5/Searchers/EventSearcher5.hpp>
#include <Core/Gen5/States/EventState5.hpp>
#include <Core/Gen5/States/SearcherState5.hpp>
#include <Core/Util/DateTime.hpp>
#include <Core/Util/SearchMetrics.hpp>
#include <Core/Util/SearchOptimization.hpp>
#include <QCoreApplication>
#include <QThread>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <tuple>

using json = nlohmann::json;
using Clock = std::chrono::steady_clock;
using IVs = std::array<u8, 6>;

static void require(bool ok, const std::string &message)
{
    if (!ok) throw std::runtime_error(message);
}

struct FilterSpec
{
    IVs min {}, max {31,31,31,31,31,31};
    std::array<bool,25> natures;
    std::array<bool,16> powers;
    u8 ability = 255, gender = 255, shiny = 255, levelMin = 1, levelMax = 100;
    bool skip = false;
    FilterSpec() { natures.fill(true); powers.fill(true); }
    StateFilter make() const
    {
        return StateFilter(gender, ability, shiny, levelMin, levelMax, 0, 255, 0, 255, skip, min, max, natures, powers);
    }
};

static Profile5 profile(Game game, bool memory = false, bool charm = false)
{
    return Profile5("Search optimization verification", game, 12345, 54321, "", "", 0x001122334455ULL,
        {true,false,false,false,false,false,false,false,false}, 0x60, 6, 6, false, 0x1100, 0x1100,
        memory, charm, DSType::DS, Language::English);
}

static auto fields(const EventState5 &s)
{
    // All fields of EventState5, GeneratorState and State, excluding padding.
    return std::tuple(s.getEC(), s.getPID(), s.getStats(), s.getAbilityIndex(), s.getIVs(), s.getAbility(),
        s.getCharacteristic(), s.getGender(), s.getHiddenPower(), s.getHiddenPowerStrength(), s.getLevel(),
        s.getNature(), s.getShiny(), s.getAdvances(), s.getLead(), s.getLeadMask(), s.getChatot(), s.getNeedle());
}

static u64 compared = 0, cases = 0;
static void same(const std::vector<EventState5> &baseline, const std::vector<EventState5> &actual, const std::string &context)
{
    require(baseline.size() == actual.size(), context + ": result count");
    for (size_t i = 0; i < baseline.size(); ++i)
        require(fields(baseline[i]) == fields(actual[i]), context + ": field/order mismatch at " + std::to_string(i));
    compared += baseline.size();
    ++cases;
}

static void check(u64 seed, u32 initial, u32 count, u32 offset, const PGF &pgf, const Profile5 &p, const FilterSpec &spec)
{
    auto f = spec.make();
    auto expected = BaselineEventGenerator5(initial, count, offset, pgf, p, f).generate(seed);
    same(expected, EventGenerator5(initial, count, offset, pgf, p, f, true).generate(seed), "optimized");
    same(expected, EventGenerator5(initial, count, offset, pgf, p, f, false).generate(seed), "disabled");
}

static Game game(const std::string &s)
{
    if (s == "Black") return Game::Black;
    if (s == "White") return Game::White;
    if (s == "Black2") return Game::Black2;
    if (s == "White2") return Game::White2;
    throw std::runtime_error("Unexpected fixture version: " + s);
}
static Shiny shiny(const std::string &s)
{
    if (s == "Never") return Shiny::Never;
    if (s == "Always") return Shiny::Always;
    if (s == "Random") return Shiny::Random;
    throw std::runtime_error("Unexpected fixture shiny: " + s);
}

static void fixtures()
{
    std::ifstream file(std::string(POKEFINDER_SOURCE_DIR) + "/Test/Gen5/event5.json");
    auto data = json::parse(file);
    for (const auto &d : data.at("generate"))
    {
        PGF pgf(d["tid"],d["sid"],d["specie"],d["nature"],d["gender"],d["ability"],shiny(d["shiny"]),d["level"],
            d["hp"],d["atk"],d["def"],d["spa"],d["spd"],d["spe"],d["egg"]);
        auto p = profile(game(d["version"]));
        FilterSpec spec;
        auto expected = BaselineEventGenerator5(0,9,0,pgf,p,spec.make()).generate(d["seed"]);
        require(expected.size() == d["results"].size(), "fixture size");
        for (size_t i = 0; i < expected.size(); ++i)
        {
            const auto &s = expected[i];
            const auto &r = d["results"][i];
            require(s.getPID() == r["pid"] && s.getIVs() == r["ivs"].get<IVs>() && s.getStats() == r["stats"].get<std::array<u16,6>>()
                && s.getAbility() == r["ability"] && s.getAbilityIndex() == r["abilityIndex"] && s.getCharacteristic() == r["characteristic"]
                && s.getGender() == r["gender"] && s.getHiddenPower() == r["hiddenPower"] && s.getHiddenPowerStrength() == r["hiddenPowerStrength"]
                && s.getLevel() == r["level"] && s.getNature() == r["nature"] && s.getShiny() == r["shiny"]
                && s.getAdvances() == r["advances"] && s.getChatot() == r["chatot"], "stored fixture fields");
        }
        check(d["seed"],0,9,0,pgf,p,spec);
        for (u8 power = 0; power < 16; ++power)
        {
            FilterSpec hidden;
            hidden.powers.fill(false); hidden.powers[power] = true;
            check(d["seed"],0,255,0,pgf,p,hidden);
        }
        for (const auto &s : expected)
        {
            FilterSpec exact; exact.min = exact.max = s.getIVs();
            check(d["seed"],0,255,0,pgf,p,exact);
        }
    }
    std::cout << "FIXTURES templates=" << data["generate"].size() << " passed\n";
}

static void equivalence()
{
    fixtures();
    u64 sequence = 0;
    for (int i = 0; i < 1200; ++i)
    {
        sequence = sequence * 6364136223846793005ULL + 1442695040888963407ULL;
        const Game games[] = {Game::Black, Game::White, Game::Black2, Game::White2};
        const Shiny modes[] = {Shiny::Never, Shiny::Random, Shiny::Always};
        const u16 species[] = {519,25,150,488,81,29};
        IVs fixed;
        for (int j = 0; j < 6; ++j) fixed[j] = (i + j) % 3 ? 255 : ((sequence >> (j * 5)) & 31);
        PGF pgf(i,65535-i,species[i%6],i%2 ? 255 : i%25,i%3 == 0 ? 255 : i%2,
            i%4 == 3 ? 255 : i%3,modes[i%3],1+i%100,fixed[0],fixed[1],fixed[2],fixed[3],fixed[4],fixed[5],i%2);
        auto p = profile(games[i%4],i%3 == 0,i%5 == 0);
        FilterSpec f;
        switch (i % 10)
        {
        case 0: break;
        case 1: f.min.fill(10); break;
        case 2: f.min.fill(31); break;
        case 3: f.max.fill(10); break;
        case 4: f.powers.fill(false); f.powers[i%16] = true; break;
        case 5: f.min.fill(31); f.powers.fill(false); f.natures.fill(false); f.skip = true; break;
        case 6: f.gender=i%3; f.ability=i%3; f.shiny=1u << (i%3); break;
        case 7: f.natures.fill(false); f.natures[i%25]=true; f.min[i%6]=12; break;
        case 8: f.powers.fill(false); break;
        case 9: f.levelMin=20; f.levelMax=60; f.min[0]=5; f.max[5]=20; break;
        }
        check(sequence,i%41,511,i%19,pgf,p,f);
    }
    // Settings are snapshotted when the generator is created, not per frame.
    FilterSpec f;
    PGF pgf(0,0,519,0,1,1,Shiny::Never,1,255,31,255,255,255,255,true);
    auto p = profile(Game::Black);
    SearchOptimization::setPruningEnabled(false);
    EventGenerator5 old(0,31,0,pgf,p,f.make());
    SearchOptimization::setPruningEnabled(true);
    same(old.generate(0),EventGenerator5(0,31,0,pgf,p,f.make()).generate(0),"settings snapshot");
    std::cout << "EQUIVALENCE comparisons=" << cases << " result_records=" << compared << " all_fields_order_multiplicity=PASS\n";
}

static void workers()
{
    FilterSpec f; f.min.fill(20);
    auto p = profile(Game::Black);
    PGF pgf(0,0,519,0,1,1,Shiny::Never,1,255,31,255,255,255,255,true);
    // Run the actual date searcher over an identical four-day SHA1 domain.
    // Normal user thread counts must change concurrency without changing results.
    auto run = [&](int workers, bool optimize) {

        EventGenerator5 generator(0,0,0,pgf,p,f.make(),optimize);
        EventSearcher5 searcher(generator,p);
        Date start(2012,10,7), end(2012,10,10);
        searcher.setMaxProgress(searcher.getMaxProgress(start,end));
        auto began = Clock::now();
        searcher.startSearch(workers,start,end);
        require(searcher.getWorkerCount() == static_cast<size_t>(workers),"actual worker creation");
        while (searcher.isSearching()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        require(searcher.getProgress() == 100,"complete domain progress");
        auto rows=searcher.getResults();
        std::sort(rows.begin(),rows.end(),[](const auto &a,const auto &b) {
            return std::tuple(a.getInitialSeed(),a.getTimer0(),a.getButtons(),a.getDateTime(),fields(a.getState()))
                 < std::tuple(b.getInitialSeed(),b.getTimer0(),b.getButtons(),b.getDateTime(),fields(b.getState()));
        });
        std::cout << "DATE_SEARCH workers=" << searcher.getWorkerCount() << " seeds=345600 optimized=" << optimize
                  << " accepted=" << rows.size() << " ms=" << std::chrono::duration<double,std::milli>(Clock::now()-began).count() << '\n';
        return rows;
    };
    auto baseline=run(4,false), optimized=run(2,true);
    require(!baseline.empty(),"nonempty date search comparison");
    require(baseline.size()==optimized.size(),"date search size");
    for(size_t i=0;i<baseline.size();++i)
        require(baseline[i].getDateTime()==optimized[i].getDateTime() && baseline[i].getInitialSeed()==optimized[i].getInitialSeed()
            && baseline[i].getTimer0()==optimized[i].getTimer0() && baseline[i].getButtons()==optimized[i].getButtons()
            && fields(baseline[i].getState())==fields(optimized[i].getState()),"date search all fields");
    // One day must use one worker; cancellation still terminates promptly.

    EventGenerator5 generator(0,100000,0,pgf,p,f.make(),true);
    EventSearcher5 cancelled(generator,p);
    Date day(2012,10,7);
    cancelled.startSearch(100,day,day);
    require(cancelled.getWorkerCount()==1,"one-day worker clamp");
    cancelled.cancelSearch();
    auto deadline=Clock::now()+std::chrono::seconds(5);
    while(cancelled.isSearching() && Clock::now()<deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    require(!cancelled.isSearching(),"cancel timeout");
    EventSearcher5 fullThreads(generator,p);
    fullThreads.startSearch(QThread::idealThreadCount(),day,day+QThread::idealThreadCount());
    require(fullThreads.getWorkerCount()==static_cast<size_t>(QThread::idealThreadCount()),"normal Threads uses all requested workers");
    fullThreads.cancelSearch();
    deadline=Clock::now()+std::chrono::seconds(5);
    while(fullThreads.isSearching() && Clock::now()<deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    require(!fullThreads.isSearching(),"normal Threads cancellation");
    std::cout << "WORKERS domain_equivalence=PASS cancellation=PASS actual_logical=" << QThread::idealThreadCount()
              << " normal_threads=" << QThread::idealThreadCount() << '\n';
}

template<class Generator> static u64 batch(const Generator &generator)
{
    u64 checksum=0, seed=0;
    for(int i=0;i<64;++i)
    {
        const auto states=generator.generate(seed);
        checksum += states.size();
        if(!states.empty()) checksum ^= (u64(states.front().getPID())<<32) | states.back().getPID();
        seed=seed*6364136223846793005ULL+1442695040888963407ULL;
    }
    return checksum;
}

static void benchmark()
{
    auto p=profile(Game::Black);
    PGF pgf(0,0,519,0,1,1,Shiny::Never,1,255,31,255,255,255,255,true);
    std::cout << "BENCHMARK fixture=Secret_Egg_Pidove seeds=64 frames_per_seed=32768 workers=1\n";
    for(const std::string name : {"broad","moderate","exact31"})
    {
        FilterSpec spec;
        if(name=="moderate") spec.min.fill(10);
        if(name=="exact31") spec.min.fill(31);
        auto f=spec.make();
        BaselineEventGenerator5 base(0,32767,0,pgf,p,f);
        EventGenerator5 opt(0,32767,0,pgf,p,f,true);
#ifdef POKEFINDER_SEARCH_INSTRUMENTATION
        for(bool optimized : {false,true})
        {
            SearchMetrics::reset();
            auto checksum=optimized ? batch(opt) : batch(base);
            std::cout << "COUNTS case=" << name << " optimized=" << optimized << " frames=" << SearchMetrics::frames
                << " iv_checks=" << SearchMetrics::ivChecks << " early_iv_rejected=" << SearchMetrics::earlyIVRejected
                << " early_hp_rejected=" << SearchMetrics::earlyHiddenPowerRejected << " full_states=" << SearchMetrics::statesConstructed
                << " accepted=" << SearchMetrics::accepted << " checksum=" << checksum << '\n';
        }
#else
        require(batch(base)==batch(opt),"benchmark warmup checksum");
        std::vector<double> baselineTimes, optimizedTimes;
        u64 sink=0;
        for(int repeat=0;repeat<7;++repeat)
        {
            auto time=[&](const auto &g) {
                auto start=Clock::now(); sink ^= batch(g);
                return std::chrono::duration<double,std::milli>(Clock::now()-start).count();
            };
            if(repeat%2) { optimizedTimes.push_back(time(opt)); baselineTimes.push_back(time(base)); }
            else { baselineTimes.push_back(time(base)); optimizedTimes.push_back(time(opt)); }
        }
        std::sort(baselineTimes.begin(),baselineTimes.end()); std::sort(optimizedTimes.begin(),optimizedTimes.end());
        std::cout << std::fixed << std::setprecision(3) << "TIMING case=" << name << " baseline_ms=" << baselineTimes[3]
            << " optimized_ms=" << optimizedTimes[3] << " speedup=" << baselineTimes[3]/optimizedTimes[3]
            << " baseline_range_ms=" << baselineTimes.front() << ':' << baselineTimes.back()
            << " optimized_range_ms=" << optimizedTimes.front() << ':' << optimizedTimes.back() << " sink=" << sink << '\n';
        // Full field/order comparison on the exact timed workload, outside timing.
        u64 seed=0;
        for(int i=0;i<64;++i)
        {
            same(base.generate(seed),opt.generate(seed),"benchmark workload");
            seed=seed*6364136223846793005ULL+1442695040888963407ULL;
        }
#endif
    }
    std::cout << "BENCHMARK_EQUIVALENCE comparisons=" << cases << " result_records=" << compared << '\n';
}

int main(int argc,char **argv)
{
    QCoreApplication app(argc,argv);
    SearchOptimization::setPruningEnabled(true);
    try
    {
        std::string mode=argc>1?argv[1]:"--verify";
        if(mode=="--benchmark") benchmark();
        else { equivalence(); workers(); }
        std::cout << "PASS\n";
        return 0;
    }
    catch(const std::exception &e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
