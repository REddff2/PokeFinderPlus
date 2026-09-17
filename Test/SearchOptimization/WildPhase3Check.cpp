// Phase 3 equivalence against the frozen, pre-change implementation.
#include "Phase2WildGenerator5.hpp"
#include <Core/Enum/DSType.hpp>
#include <Core/Enum/Encounter.hpp>
#include <Core/Enum/Game.hpp>
#include <Core/Enum/Language.hpp>
#include <Core/Enum/Method.hpp>
#include <Core/Gen5/Encounters5.hpp>
#include <Core/Gen5/States/WildState5.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <tuple>
using json = nlohmann::json;
static void check(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}
static u64 mix(u64 x)
{
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}
static auto fields(const WildState5 &s)
{
    return std::tuple(s.getEC(), s.getPID(), s.getStats(), s.getAbilityIndex(), s.getIVs(), s.getAbility(), s.getCharacteristic(),
        s.getGender(), s.getHiddenPower(), s.getHiddenPowerStrength(), s.getLevel(), s.getNature(), s.getShiny(), s.getAdvances(),
        s.getIVAdvances(), s.getEncounterSlot(), s.getSpecie(), s.getForm(), s.getItem(), s.getLead(), s.getLeadMask(),
        s.getMovingTrigger(), s.getMovingSteps(), s.getPhenomenon(), s.getPhenomenonItem(), s.isValid(),
        static_cast<const WildGeneratorState &>(s).isValid(), s.getPassPower(), s.getVariableNature(), s.getLeadRequired(),
        s.getChatot(), s.getNeedle());
}
static u64 comparedRows = 0, comparisons = 0;
static void equal(const std::vector<WildState5> &a, const std::vector<WildState5> &b)
{
    check(a.size() == b.size(), "result multiplicity mismatch");
    for (size_t i = 0; i < a.size(); ++i) check(fields(a[i]) == fields(b[i]), "result field/order mismatch");
    comparedRows += a.size(); ++comparisons;
}
struct ProfileInput
{
    Game version = Game::White;
    u16 tid = 69, sid = 58008, timerMin = 0xc7f, timerMax = 0xc7f;
    u64 mac = 0x1656a634e5ULL;
    u8 vcount = 0x60, gxstat = 6, vframe = 8;
    bool skipLR = false, memory = false, charm = false, released = false;
    DSType ds = DSType::DS;
    Language language = Language::English;
    std::array<bool, 9> keys { true, true, true, true, true, true, true, true, true };
    Profile5 make() const
    {
        return Profile5("W1 DS Lite phase 3", version, tid, sid, "", "", mac, keys, vcount, gxstat, vframe, skipLR,
            timerMin, timerMax, memory, charm, ds, language, released);
    }
};
struct FilterInput
{
    u8 gender = 255, ability = 255, shiny = 3, minLevel = 20, maxLevel = 20;
    u8 minHeight = 0, maxHeight = 255, minWeight = 0, maxWeight = 255;
    bool skip = false;
    std::array<u8, 6> low {25,25,25,25,25,25}, high {31,31,31,31,31,31};
    std::array<bool, 25> natures;
    std::array<bool, 16> hp;
    std::array<bool, 13> slots {};
    FilterInput() { natures.fill(true); hp.fill(true); slots[5] = slots[12] = true; }
    WildStateFilter make() const
    {
        return WildStateFilter(gender, ability, shiny, minLevel, maxLevel, minHeight, maxHeight, minWeight, maxWeight,
            skip, low, high, natures, hp, slots);
    }
    void broad() { low.fill(0); minLevel=1; maxLevel=100; shiny=255; slots.fill(true); }
};
static EncounterArea5 findArea(const Profile5 &profile, Encounter encounter, int location = -1)
{
    auto areas = Encounters5::getEncounters(encounter, {false,0}, &profile);
    auto it = std::ranges::find_if(areas, [&](const auto &a) { return location < 0 || a.getLocation() == location; });
    check(it != areas.end(), "encounter fixture unavailable");
    return *it;
}
struct Request
{
    ProfileInput profile;
    FilterInput filter;
    EncounterArea5 area = findArea(profile.make(), Encounter::Grass, 8);
    u32 initialPID=0, maxPID=0, offset=0, initialIV=0, maxIV=0;
    Method method=Method::Method5;
    std::vector<Lead> leads {Lead::None};
    std::vector<u8> powers {0};
    bool moving=false, required=false, requirePowerIV=true, requiredLeads=true, optimized=true;
    EncounterSettings5 settings {false,0};
    WildGenerator5 current() const
    {
        return WildGenerator5(initialPID,maxPID,offset,method,leads,powers,moving,required,area,profile.make(),filter.make(),
            requirePowerIV,requiredLeads,optimized,settings);
    }
    Phase2WildGenerator5 reference() const
    {
        return Phase2WildGenerator5(initialPID,maxPID,offset,method,leads,powers,moving,required,area,profile.make(),filter.make(),
            requirePowerIV,requiredLeads,optimized);
    }
};
int main(int argc, char **argv)
{
    try
    {
        check(argc == 2, "expected retained native result JSON path");
        std::ifstream file(argv[1]); const auto known=json::parse(file);
        check(known.size()==34,"retained positives must contain 34 rows");
        constexpr size_t count=1<<20;
        std::vector<u64> seeds(count);
        for(size_t i=0;i<count;++i) seeds[i]=mix(i);
        const u32 edges[]={0,1,0xffffffffu,0x80000000u,5489,0x7fffffffu,0x12345678u};
        for(size_t i=0;i<std::size(edges);++i) seeds[i]=(u64(edges[i])<<32)|i;
        size_t next=32; for(const auto &row:known) seeds[next++]=row["seed"].get<u64>();
        seeds[128]=seeds[32]; seeds[129]=seeds[32];
        Request request;
        auto combined=request.current(); auto frozen=request.reference();
        check(combined.payloadFirstEnabled(0,0) && combined.reducedIVsEnabled(0,0),"exact request must enable both CPU improvements");
        Request reducedRequest=request; reducedRequest.settings={true,255};
        auto reduced=reducedRequest.current();
        check(!reduced.payloadFirstEnabled(0,0) && reduced.reducedIVsEnabled(0,0),"unknown context must use reduced-only fallback");
        Request disabled=request; disabled.optimized=false;
        auto off=disabled.current(); auto oldOff=disabled.reference();
        check(!off.payloadFirstEnabled(0,0) && !off.reducedIVsEnabled(0,0),"master toggle disables both");
        u64 accepted=0;
        for(u64 seed:seeds)
        {
            const auto expected=frozen.generate(seed,0,0);
            equal(expected,combined.generate(seed,0,0));
            equal(expected,reduced.generate(seed,0,0));
            equal(oldOff.generate(seed,0,0),off.generate(seed,0,0));
            accepted+=expected.size();
        }
        check(accepted==36,"all 34 known positives and both duplicate occurrences preserved");
        std::cout<<"PASS corpus_seeds="<<count<<" accepted_occurrences="<<accepted<<std::endl;

        using Mutation=std::pair<const char *,std::function<void(Request &)>>;
        std::vector<Mutation> mutations {
            {"toggle",[](auto&r){r.optimized=false;}}, {"PID initial",[](auto&r){r.initialPID=1;}},
            {"PID max",[](auto&r){r.maxPID=1;}}, {"offset",[](auto&r){r.offset=1;}},
            {"method",[](auto&r){r.method=Method::None;}}, {"IV initial",[](auto&r){r.initialIV=1;}},
            {"IV max",[](auto&r){r.maxIV=1;}}, {"lead",[](auto&r){r.leads={Lead::Pressure};}},
            {"multiple leads",[](auto&r){r.leads={Lead::None,Lead::Pressure};}},
            {"duplicate leads",[](auto&r){r.leads={Lead::None,Lead::None};}}, {"empty leads",[](auto&r){r.leads={};}},
            {"power",[](auto&r){r.powers={1};}}, {"encounter power",[](auto&r){r.powers={16};}},
            {"multiple powers",[](auto&r){r.powers={0,1};}}, {"empty powers",[](auto&r){r.powers={};}},
            {"moving",[](auto&r){r.moving=true;}}, {"required trigger",[](auto&r){r.required=true;}},
            {"required lead policy",[](auto&r){r.requiredLeads=false;}}, {"swarm",[](auto&r){r.settings.swarm=true;}},
            {"summer",[](auto&r){r.settings.season=1;}}, {"autumn",[](auto&r){r.settings.season=2;}},
            {"winter",[](auto&r){r.settings.season=3;}}, {"unknown season",[](auto&r){r.settings.season=255;}},
            {"Black",[](auto&r){r.profile.version=Game::Black;}}, {"White2",[](auto&r){r.profile.version=Game::White2;}},
            {"TID",[](auto&r){++r.profile.tid;}}, {"SID",[](auto&r){++r.profile.sid;}},
            {"MAC",[](auto&r){++r.profile.mac;}}, {"Timer0 min",[](auto&r){--r.profile.timerMin;}},
            {"Timer0 max",[](auto&r){++r.profile.timerMax;}}, {"VCount",[](auto&r){++r.profile.vcount;}},
            {"VFrame",[](auto&r){++r.profile.vframe;}}, {"GxStat",[](auto&r){++r.profile.gxstat;}},
            {"DS type",[](auto&r){r.profile.ds=DSType::DSi;}}, {"language",[](auto&r){r.profile.language=Language::Japanese;}},
            {"skip LR",[](auto&r){r.profile.skipLR=true;}}, {"Memory Link",[](auto&r){r.profile.memory=true;}},
            {"Shiny Charm",[](auto&r){r.profile.charm=true;}}, {"N release",[](auto&r){r.profile.released=true;}},
            {"skip filters",[](auto&r){r.filter.skip=true;}}, {"ability",[](auto&r){r.filter.ability=0;}},
            {"gender",[](auto&r){r.filter.gender=0;}}, {"shiny Any",[](auto&r){r.filter.shiny=255;}},
            {"star only",[](auto&r){r.filter.shiny=1;}}, {"square only",[](auto&r){r.filter.shiny=2;}},
            {"level min",[](auto&r){r.filter.minLevel=19;}}, {"level max",[](auto&r){r.filter.maxLevel=21;}},
            {"height",[](auto&r){r.filter.maxHeight=254;}}, {"weight",[](auto&r){r.filter.maxWeight=254;}},
            {"slot 5",[](auto&r){r.filter.slots[5]=false;}}, {"extra slot",[](auto&r){r.filter.slots[0]=true;}},
            {"swarm slot flag",[](auto&r){r.filter.slots[12]=false;}},
            {"location",[](auto&r){r.area=findArea(r.profile.make(),Encounter::Grass,41);}},
            {"Surfing",[](auto&r){r.area=findArea(r.profile.make(),Encounter::Surfing);}},
            {"Super Rod",[](auto&r){r.area=findArea(r.profile.make(),Encounter::SuperRod);}},
            {"changed table",[](auto&r){std::array<Slot,13> slots;for(u8 i=0;i<13;++i)slots[i]=r.area.getPokemon(i);
                slots[0]=slots[5];r.area=EncounterArea5(8,r.area.getRate(),r.area.getSeason(),Encounter::Grass,slots);}}
        };
        for(size_t i=0;i<9;++i) mutations.emplace_back("keypress count",[i](auto&r){r.profile.keys[i]=false;});
        for(size_t i=0;i<6;++i)
        {
            mutations.emplace_back("invalid IV minimum",[i](auto&r){r.filter.low[i]=32;});
            mutations.emplace_back("IV maximum outside supported range",[i](auto&r){r.filter.high[i]=32;});
        }
        for(size_t i=0;i<25;++i) mutations.emplace_back("nature",[i](auto&r){r.filter.natures[i]=false;});
        for(size_t i=0;i<16;++i) mutations.emplace_back("HP type",[i](auto&r){r.filter.hp[i]=false;});
        for(const auto &[name,mutate]:mutations)
        {
            Request changed=request; mutate(changed); auto actual=changed.current(); auto expected=changed.reference();
            if(actual.payloadFirstEnabled(changed.initialIV,changed.maxIV)) throw std::runtime_error(std::string("guard admitted ")+name);
            for(size_t i=0;i<160;++i) equal(expected.generate(seeds[i],changed.initialIV,changed.maxIV),
                actual.generate(seeds[i],changed.initialIV,changed.maxIV));
        }
        std::cout<<"PASS negative_guard_cases="<<mutations.size()<<std::endl;

        // The reduced prefix changes IV arithmetic only, including all existing
        // special-branch collection/filter behavior. Compare ordered full rows.
        u64 branchCases=0;
        for(Game version:{Game::Black,Game::White,Game::Black2,Game::White2})
        for(Encounter encounter:{Encounter::Grass,Encounter::GrassDark,Encounter::Surfing,Encounter::SuperRod,
            Encounter::SuperRodRippling,Encounter::DustCloud,Encounter::FlyingShadow})
        for(Lead lead:{Lead::None,Lead::Pressure,Lead::CuteCharmF,Lead::Static})
        for(bool moving:{false,true})
        {
            Request branch;branch.profile.version=version;branch.area=findArea(branch.profile.make(),encounter);
            branch.filter.broad();branch.leads={lead};branch.moving=moving;branch.maxPID=7;branch.settings={false,0};
            auto actual=branch.current();auto expected=branch.reference();
            check(actual.reducedIVsEnabled(0,0) && !actual.payloadFirstEnabled(0,0),"branch reduced/fallback policy");
            for(size_t i=0;i<128;++i) equal(expected.generate(seeds[i],0,0),actual.generate(seeds[i],0,0));
            ++branchCases;
        }
        // Caller-supplied cache IV lists retain their old ordering/merge semantics.
        const std::vector<std::pair<u32,std::array<u8,6>>> ivs {{1,{31,31,31,31,31,31}},
            {0,{31,31,31,31,31,31}},{1,{31,31,31,31,31,31}}};
        for(size_t i=0;i<160;++i) equal(frozen.generate(seeds[i],ivs),combined.generate(seeds[i],ivs));
        std::cout<<"PASS reduced_branch_cases="<<branchCases<<" comparisons="<<comparisons<<" compared_rows="<<comparedRows<<'\n';
        std::cout<<"PHASE3_EQUIVALENCE_PASS exit=0"<<std::endl;
        return 0;
    }
    catch(const std::exception &error) {std::cerr<<"FAIL "<<error.what()<<std::endl;return 1;}
}
