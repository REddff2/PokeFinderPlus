#pragma once
// Phase 3 equivalence against the frozen, pre-change implementation.
#include <Core/Gen5/Generators/WildGenerator5.hpp>
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
};
