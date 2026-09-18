#ifndef SEARCHOPTIMIZATION_HPP
#define SEARCHOPTIMIZATION_HPP

#include <array>
#include <cstddef>

// Optional Gen 5 paths only. Normal PokeFinder Threads controls all workers.
namespace SearchOptimization
{
    enum class Family { Wild, Static, Event, Eggs, DreamRadar, HiddenGrotto, Pickup, AdjacentSeeds, CacheBuilders, None };
    inline constexpr size_t FamilyCount = static_cast<size_t>(Family::None);
    inline constexpr std::array<const char *, FamilyCount> FamilyKeys = {
        "plusSearchWild", "plusSearchStatic", "plusSearchEvent", "plusSearchEggs", "plusSearchDreamRadar",
        "plusSearchHiddenGrotto", "plusSearchPickup", "plusSearchAdjacentSeeds", "plusSearchCacheBuilders"
    };
    inline constexpr bool DefaultPruning = false;
    inline constexpr bool DefaultFamily = true;
    inline constexpr bool DefaultGpu = false;

    void setPruningEnabled(bool enabled);
    bool pruningEnabled();
    void setFamilyEnabled(Family family, bool enabled);
    bool familyEnabled(Family family); // Saved preference, independent of master.
    bool pruningEnabled(Family family); // Effective CPU policy.
    void setGpuEnabled(bool enabled);
    bool gpuEnabled();
    bool gpuEnabled(Family family); // Workload support/guards still apply.
}

#endif
