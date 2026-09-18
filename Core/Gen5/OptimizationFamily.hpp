#ifndef GEN5_OPTIMIZATIONFAMILY_HPP
#define GEN5_OPTIMIZATIONFAMILY_HPP

#include <Core/Util/SearchOptimization.hpp>

class WildGenerator5;
class StaticGenerator5;
class EventGenerator5;
class EggGenerator5;
class DreamRadarGenerator;
class HiddenGrottoGenerator;
class HiddenGrottoSlotGenerator;
class PickupGenerator;

namespace Gen5
{
    // Shared search/storage code must have an explicit owner. Unlisted
    // generators (including IDs) keep baseline behavior.
    template <class Generator>
    inline constexpr auto optimizationFamily = SearchOptimization::Family::None;

    template <> inline constexpr auto optimizationFamily<WildGenerator5> = SearchOptimization::Family::Wild;
    template <> inline constexpr auto optimizationFamily<StaticGenerator5> = SearchOptimization::Family::Static;
    template <> inline constexpr auto optimizationFamily<EventGenerator5> = SearchOptimization::Family::Event;
    template <> inline constexpr auto optimizationFamily<EggGenerator5> = SearchOptimization::Family::Eggs;
    template <> inline constexpr auto optimizationFamily<DreamRadarGenerator> = SearchOptimization::Family::DreamRadar;
    template <> inline constexpr auto optimizationFamily<HiddenGrottoGenerator> = SearchOptimization::Family::HiddenGrotto;
    template <> inline constexpr auto optimizationFamily<HiddenGrottoSlotGenerator> = SearchOptimization::Family::HiddenGrotto;
    template <> inline constexpr auto optimizationFamily<PickupGenerator> = SearchOptimization::Family::Pickup;
}

#endif
