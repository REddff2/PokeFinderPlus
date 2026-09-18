#include "SearchOptimization.hpp"
#include <atomic>

namespace
{
    constexpr unsigned Smart = 1, Gpu = 2;
    constexpr unsigned AllFamilies = ((1u << SearchOptimization::FamilyCount) - 1) << 2;
    std::atomic<unsigned> policy { AllFamilies };

    unsigned familyBit(SearchOptimization::Family family)
    {
        const auto index = static_cast<unsigned>(family);
        return index < SearchOptimization::FamilyCount ? 1u << (index + 2) : 0;
    }
}

void SearchOptimization::setPruningEnabled(bool enabled)
{
    if (enabled) policy.fetch_or(Smart, std::memory_order_relaxed);
    else policy.fetch_and(~(Smart | Gpu), std::memory_order_relaxed);
}

bool SearchOptimization::pruningEnabled()
{
    return (policy.load(std::memory_order_relaxed) & Smart) != 0;
}

void SearchOptimization::setFamilyEnabled(Family family, bool enabled)
{
    const unsigned bit = familyBit(family);
    if (enabled) policy.fetch_or(bit, std::memory_order_relaxed);
    else policy.fetch_and(~bit, std::memory_order_relaxed);
}

bool SearchOptimization::familyEnabled(Family family)
{
    return (policy.load(std::memory_order_relaxed) & familyBit(family)) != 0;
}

bool SearchOptimization::pruningEnabled(Family family)
{
    const unsigned bit = familyBit(family), required = Smart | bit;
    return bit && (policy.load(std::memory_order_relaxed) & required) == required;
}

void SearchOptimization::setGpuEnabled(bool enabled)
{
    unsigned current = policy.load(std::memory_order_relaxed);
    unsigned desired;
    do
    {
        desired = enabled && (current & Smart) ? current | Gpu : current & ~Gpu;
    } while (!policy.compare_exchange_weak(current, desired, std::memory_order_relaxed));
}

bool SearchOptimization::gpuEnabled()
{
    return (policy.load(std::memory_order_relaxed) & (Smart | Gpu)) == (Smart | Gpu);
}

bool SearchOptimization::gpuEnabled(Family family)
{
    const unsigned bit = familyBit(family), required = Smart | Gpu | bit;
    return bit && (policy.load(std::memory_order_relaxed) & required) == required;
}
