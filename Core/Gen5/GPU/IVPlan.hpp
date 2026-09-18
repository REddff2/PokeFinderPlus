#ifndef GEN5_GPU_IVPLAN_HPP
#define GEN5_GPU_IVPLAN_HPP
#include <Core/Global.hpp>
#include <Core/Parents/Filters/StateFilter.hpp>
#include <optional>
namespace GpuWild
{
    // Independent MT IV prefilter, not the Wild PID/payload kernel. Only one
    // final IV window is currently accelerated. CPU always finalizes survivors.
    struct IVPlan
    {
        u32 offset;
        bool roamer;
        u32 cacheFrames = 0; // Nonzero: union of cache predicates, not IV bounds.
        bool valid() const { return cacheFrames ? cacheFrames <= 8 && u64(offset) + cacheFrames + 31 <= 224 : offset <= 218; }
    };

    inline std::optional<IVPlan> ivPlan(bool smart, u64 offset, u32 extraFrames, bool roamer, const StateFilter &filter)
    {
        if (!smart || extraFrames != 0 || offset > 218) return std::nullopt;
        const auto &low = filter.getMinIVs(); const auto &high = filter.getMaxIVs();
        double density = 1;
        for (u8 i = 0; i < 6; ++i)
        {
            if (low[i] > high[i] || high[i] > 31) return std::nullopt;
            density *= (high[i] - low[i] + 1) / 32.0;
        }
        // Broad/disabled filters don't repay compaction and host finalization.
        if (density > 0.01 || (filter.compareIV({0,0,0,0,0,0}) && filter.compareIV({31,31,31,31,31,31}))) return std::nullopt;
        return IVPlan { u32(offset), roamer };
    }
}
#endif
