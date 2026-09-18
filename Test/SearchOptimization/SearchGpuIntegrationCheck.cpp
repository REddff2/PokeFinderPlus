#include "WildFixtures.hpp"
#include "IVDomainCases.hpp"
#include <Core/Gen5/GPU/Session.hpp>
#include <Core/Gen5/Searchers/IVSearcher5.hpp>
#include <Core/Gen5/States/SearcherState5.hpp>
#include <Core/Enum/Buttons.hpp>
#include <Core/Util/SearchOptimization.hpp>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <thread>
static double now(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
static json record(const SearcherState5<WildState5>&r){
    const auto&s=r.getState();return {{"seed",r.getInitialSeed()},{"date",r.getDateTime().toString()},
        {"timer0",r.getTimer0()},{"buttons",toInt(r.getButtons())},{"ivs",s.getIVs()},{"stats",s.getStats()},
        {"fields",{s.getEC(),s.getPID(),s.getAbilityIndex(),s.getAbility(),s.getCharacteristic(),s.getGender(),s.getHiddenPower(),
        s.getHiddenPowerStrength(),s.getLevel(),s.getNature(),s.getShiny(),s.getAdvances(),s.getIVAdvances(),s.getEncounterSlot(),s.getSpecie(),
        s.getForm(),s.getItem(),toInt(s.getLead()),s.getLeadMask(),s.getMovingTrigger(),s.getMovingSteps(),s.getPhenomenon(),s.getPhenomenonItem(),
        s.isValid(),static_cast<const WildGeneratorState&>(s).isValid(),s.getPassPower(),s.getVariableNature(),s.getLeadRequired(),s.getChatot(),s.getNeedle()}}};
}
static void wait(IVSearcher5<WildGenerator5,WildState5>&s,double timeout=120){
    double end=now()+timeout;while(s.isSearching()&&now()<end)std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if(s.isSearching()){s.cancelSearch();while(s.isSearching())std::this_thread::sleep_for(std::chrono::milliseconds(5));throw std::runtime_error("Search timeout");}
}
int main(int argc,char**argv){
    try{
        SearchOptimization::setPruningEnabled(true);
        check(argc==3,"expected retained positive results and output JSON");
        std::ifstream f(argv[1]);json known=json::parse(f),report;Request r;GpuWild::IVBounds bounds{r.filter.low,r.filter.high};
        GpuWild::Options options;
        auto directory=std::filesystem::path(argv[0]).parent_path();
        options.helperPath=(directory/"PokeFinderGpuHelper.exe").wstring();options.kernelPath=(directory/"Gen5Wild.cl").wstring();
        std::atomic<bool> cancel{false};
        std::vector<u64> corpus(1<<20);for(size_t i=0;i<corpus.size();++i)corpus[i]=mix(i);
        size_t next=32;for(const auto&row:known)corpus[next++]=row["seed"].get<u64>();corpus[128]=corpus[32];corpus[129]=corpus[32];
        std::vector<u32> expected;auto optimized=r.current();
        for(u32 i=0;i<corpus.size();++i)if(!optimized.generate(corpus[i],0,0).empty())expected.push_back(i);
        check(expected.size()==36,"positive occurrences preserved");

        // Both policy states must prevent even helper/device initialization.
        for(bool smart:{false,true}){
            SearchOptimization::setPruningEnabled(smart);SearchOptimization::setGpuEnabled(false);
            auto session=std::make_shared<GpuWild::Session>(options);
            check(!session->filter(corpus,cancel,bounds),"GPU off returns CPU replay without initialization");
            check(json::parse(session->diagnostics())["initialization_attempts"]==0,"no OpenCL initialization when GPU off");
            Request request=r;request.optimized=smart;auto g=request.current();
            IVSearcher5<WildGenerator5,WildState5> searcher(0,0,g,request.profile.make(),session);
            check(!searcher.getGpuSession(),"ordinary search dispatch respects GPU off");
            if(!smart)check(!g.payloadFirstEnabled(0,0)&&!g.reducedIVsEnabled(0,0)&&!g.optimizedPruningEnabled(),"Smart off disables every added generator path");
        }
        SearchOptimization::setPruningEnabled(false);SearchOptimization::setGpuEnabled(true);
        check(!SearchOptimization::gpuEnabled(),"GPU cannot enable without Smart");
        SearchOptimization::setPruningEnabled(true);SearchOptimization::setGpuEnabled(true);
        for(int device=0;device<2;++device){
            options.device=device;GpuWild::Session session(options);auto indices=session.filter(corpus,cancel,bounds);
            check(indices&&*indices==expected,"promoted GPU session exact million-seed compaction");
            report["devices"].push_back(json::parse(session.diagnostics()));
        }
        options.device=0;
        std::vector<u64> faultSeeds(corpus.begin(),corpus.begin()+4096);std::vector<u32> faultExpected;
        for(u32 i=0;i<faultSeeds.size();++i)if(!optimized.generate(faultSeeds[i],0,0).empty())faultExpected.push_back(i);
        for(const wchar_t*fault:{L"no-device",L"init",L"build",L"allocation",L"execute",L"reset",L"timeout",L"invalid-output",L"after-one"}){
            auto failed=options;failed.fault=fault;failed.timeoutMs=250;
            GpuWild::Session session(failed);
            if(failed.fault==L"after-one"){auto first=session.filter(faultSeeds,cancel,bounds);check(first&&*first==faultExpected,"committed prefix before GPU failure");}
            std::atomic<unsigned> completed{0};std::vector<std::thread> threads;
            for(unsigned i=0;i<4;++i)threads.emplace_back([&]{auto actual=session.filter(faultSeeds,cancel,bounds);if(!actual){actual.emplace();
                for(u32 j=0;j<faultSeeds.size();++j)if(!optimized.generate(faultSeeds[j],0,0).empty())actual->push_back(j);}if(*actual==faultExpected)++completed;});
            for(auto&t:threads)t.join();check(completed==4,"GPU fault replays every uncommitted occurrence");
            auto diagnostic=json::parse(session.diagnostics());diagnostic["injected_fault"]=std::filesystem::path(fault).string();report["fallbacks"].push_back(diagnostic);
        }
        // Missing optional helper is a normal CPU fallback, including on machines without OpenCL.
        auto missing=options;missing.helperPath=L"C:/PokeFinderDev/no-such-optional-helper.exe";
        GpuWild::Session absent(missing);check(!absent.filter(faultSeeds,cancel,bounds),"missing helper CPU fallback");

        // Cancellation interrupts a stalled GPU within the 25ms polling interval.
        auto stalled=options;stalled.fault=L"timeout";
        GpuWild::Session pending(stalled);double started=now();
        std::thread cancelling([&]{std::this_thread::sleep_for(std::chrono::milliseconds(1500));cancel=true;});
        auto ignored=pending.filter(faultSeeds,cancel,bounds);cancelling.join();check(!ignored&&now()-started<4,"GPU cancellation remains responsive");cancel=false;

        // Unsupported requests cannot instantiate or enumerate GPU, even when enabled.
        Request unsupported;unsupported.profile.keys.fill(false);unsupported.profile.keys[0]=true;unsupported.filter.broad();
        auto untried=std::make_shared<GpuWild::Session>(options);auto g=unsupported.current();
        IVSearcher5<WildGenerator5,WildState5> fallback(0,0,g,unsupported.profile.make(),untried);
        check(!fallback.getGpuSession(),"unsupported workload uses ordinary CPU searcher");
        fallback.startSearch(1,Date(2026,6,15),Date(2026,6,15));wait(fallback);
        auto actual=fallback.getResults();check(actual.size()==86400,"unsupported CPU search full domain");
        SearchOptimization::setGpuEnabled(false);
        IVSearcher5<WildGenerator5,WildState5> cpu(0,0,g,unsupported.profile.make());cpu.startSearch(1,Date(2026,6,15),Date(2026,6,15));wait(cpu);
        auto expectedRows=cpu.getResults();check(actual.size()==expectedRows.size(),"unsupported fallback multiplicity");
        for(size_t i=0;i<actual.size();++i)check(record(actual[i])==record(expectedRows[i]),"unsupported fallback metadata and fields");
        check(json::parse(untried->diagnostics())["initialization_attempts"]==0,"unsupported search never initializes OpenCL");
        report["unsupported_rows"]=actual.size();

        // One representative real date only; no repeated 93-day domain.
        SearchOptimization::setGpuEnabled(true);
        auto realSession=std::make_shared<GpuWild::Session>(options);
        IVSearcher5<WildGenerator5,WildState5> real(0,0,optimized,r.profile.make(),realSession);
        check(real.getGpuSession()!=nullptr,"exact production guard dispatches GPU");
        Date day(2026,8,17);real.setMaxProgress(real.getMaxProgress(day,day));started=now();real.startSearch(15,day,day);wait(real);
        json realRows=json::array(),wanted=json::array();for(const auto&row:real.getResults())realRows.push_back(record(row));
        for(const auto&row:known)if(row["date"].get<std::string>().starts_with("2026-08-17 "))wanted.push_back(row);
        auto sort=[](json&rows){std::sort(rows.begin(),rows.end(),[](const auto&a,const auto&b){return a.dump()<b.dump();});};sort(realRows);sort(wanted);
        check(!wanted.empty()&&realRows==wanted,"real one-day search matches retained baseline in every field");
        auto realMetrics=json::parse(realSession->diagnostics());
        check(realMetrics["backend"]["candidates"]==185241600ULL&&realMetrics["backend"]["fallback_batches"]==0,"entire one-day domain used GPU");
        report["one_day"]={{"wall_s",now()-started},{"rows",realRows},{"gpu",realMetrics},{"workers",real.getWorkerCount()}};
        // Verify real Searcher-to-session bounds and result metadata for below-25
        // and mixed upper/lower bounds. One date only; fixed-seed baseline proof
        // is in WildIVDomainCheck. No full 93-day search is run.
        for(size_t caseIndex:{size_t(0),size_t(14)}) {
            const auto selected=ivDomainCases()[caseIndex];auto request=r;
            request.filter.low=selected.bounds.min;request.filter.high=selected.bounds.max;
            auto generator=request.current();SearchOptimization::setGpuEnabled(false);
            IVSearcher5<WildGenerator5,WildState5> cpuSearch(0,0,generator,request.profile.make());
            started=now();cpuSearch.startSearch(15,day,day);wait(cpuSearch);
            json expected=json::array();for(const auto&row:cpuSearch.getResults())expected.push_back(record(row));sort(expected);
            json matrix={{"name",selected.name},{"cpu_s",now()-started},{"results",expected.size()}};
            for(int device=0;device<2;++device) {
                SearchOptimization::setGpuEnabled(true);auto chosen=options;chosen.device=device;
                auto session=std::make_shared<GpuWild::Session>(chosen);
                IVSearcher5<WildGenerator5,WildState5> gpuSearch(0,0,generator,request.profile.make(),session);
                check(gpuSearch.getGpuSession()!=nullptr,"expanded range actual GPU dispatch");
                started=now();gpuSearch.startSearch(15,day,day);wait(gpuSearch);
                json actual=json::array();for(const auto&row:gpuSearch.getResults())actual.push_back(record(row));sort(actual);
                check(actual==expected,"broad/mixed actual Searcher fields and metadata match CPU");
                auto metrics=json::parse(session->diagnostics());
                check(metrics["backend"]["candidates"]==185241600ULL&&metrics["backend"]["fallback_batches"]==0,"expanded full one-day domain used GPU");
                matrix["devices"].push_back({{"device",device},{"wall_s",now()-started},{"metrics",metrics}});
            }
            report["expanded_one_day"].push_back(matrix);std::cout<<matrix.dump()<<std::endl;
        }
        report["pass"]=true;std::ofstream(argv[2])<<report.dump(2);std::cout<<"GPU_INTEGRATION_PASS rows="<<realRows.size()<<std::endl;return 0;
    }catch(const std::exception&e){std::cerr<<"GPU_INTEGRATION_FAIL "<<e.what()<<std::endl;return 1;}
}
