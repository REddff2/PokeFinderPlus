#ifndef GEN5_SMARTFILTER_HPP
#define GEN5_SMARTFILTER_HPP

#include <Core/Parents/Filters/StateFilter.hpp>

namespace Gen5
{
    inline bool finalIVsPass(const StateFilter &filter, const std::array<u8, 6> &ivs)
    {
        if (!filter.compareIV(ivs)) return false;
        // State::updateStats uses HP, Atk, Def, Spe, SpA, SpD bit order.
        const u8 bits = (ivs[0] & 1) | ((ivs[1] & 1) << 1) | ((ivs[2] & 1) << 2)
            | ((ivs[5] & 1) << 3) | ((ivs[3] & 1) << 4) | ((ivs[4] & 1) << 5);
        return filter.compareHiddenPower(bits * 15 / 63);
    }

    inline bool constrainedIVs(const StateFilter &filter)
    {
        if (!filter.compareIV({ 0, 0, 0, 0, 0, 0 }) || !filter.compareIV({ 31, 31, 31, 31, 31, 31 })) return true;
        for (u8 i = 0; i < 16; ++i) if (!filter.compareHiddenPower(i)) return true;
        return false;
    }

    inline bool payloadPass(const StateFilter &filter, u8 ability, u8 gender, u8 nature, u8 shiny)
    {
        return filter.compareAbility(ability) && filter.compareGender(gender)
            && filter.compareNature(nature) && filter.compareShiny(shiny);
    }
}
#endif
