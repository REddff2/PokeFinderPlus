#ifndef GEN5_IVRNG_HPP
#define GEN5_IVRNG_HPP

#include <Core/RNG/MT.hpp>
#include <Core/RNG/MTFast.hpp>
#include <Core/RNG/RNGList.hpp>
#include <array>
#include <optional>

namespace Gen5
{
    inline u8 mtIV(MT &rng) { return rng.next() >> 27; }

    // A bounded MT window, with exactly the same sliding/reset semantics as
    // RNGList. MTFast is valid only before output 227. Larger requests retain MT.
    // 'outputs' includes every lookahead (six IVs, or 28 for Entralink caches).
    class IVRNG
    {
    public:
        IVRNG(u32 seed, u32 start, u64 outputs, bool smart) : head(start), index(start)
        {
            const u64 end = u64(start) + outputs;
            if (!smart || end > values.size())
            {
                full.emplace(seed, start);
            }
            else if (end <= 8) fill<8>(seed, end);
            else if (end <= 16) fill<16>(seed, end);
            else if (end <= 32) fill<32>(seed, end);
            else if (end <= 64) fill<64>(seed, end);
            else if (end <= 128) fill<128>(seed, end);
            else fill<224>(seed, end);
        }

        u8 next() { return full ? full->next() : values[index++]; }
        void advance(u32 count) { if (full) full->advance(count); else index += count; }
        void resetState() { if (full) full->resetState(); else index = head; }
        void advanceState() { advanceStates(1); }
        void advanceStates(u32 count)
        {
            if (full) full->advanceStates(count);
            else { head += count; index = head; }
        }

    private:
        template <u16 Size> void fill(u32 seed, u32 end)
        {
            MTFast<Size, true> mt(seed);
            for (u32 i = 0; i < end; ++i) values[i] = mt.next();
        }
        std::optional<RNGList<u8, MT, 32, mtIV>> full;
        std::array<u8, 224> values;
        u32 head, index;
    };
}
#endif
