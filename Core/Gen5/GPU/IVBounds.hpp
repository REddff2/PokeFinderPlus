#pragma once
#include <array>
#include <cstdint>

namespace GpuWild
{
    // CPU IV-array order: HP, Atk, Def, SpA, SpD, Spe.
    struct IVBounds
    {
        std::array<uint8_t, 6> min {};
        std::array<uint8_t, 6> max { 31, 31, 31, 31, 31, 31 };

        bool valid() const
        {
            for (unsigned i = 0; i < min.size(); ++i)
                if (min[i] > max[i] || max[i] > 31) return false;
            return true;
        }
        uint32_t packedMin() const { return pack(min); }
        uint32_t packedMax() const { return pack(max); }

    private:
        static uint32_t pack(const std::array<uint8_t, 6> &ivs)
        {
            uint32_t result = 0;
            for (unsigned i = 0; i < ivs.size(); ++i) result |= uint32_t(ivs[i]) << (5 * i);
            return result;
        }
    };
}
