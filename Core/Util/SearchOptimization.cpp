#include "SearchOptimization.hpp"
#include <atomic>

namespace
{
    constexpr unsigned Smart = 1, Gpu = 2;
    std::atomic<unsigned> policy { 0 };
}

void SearchOptimization::setPruningEnabled(bool enabled)
{
    if (enabled) policy.fetch_or(Smart, std::memory_order_relaxed);
    else policy.store(0, std::memory_order_relaxed); // Turning Smart off also forces GPU off.
}

bool SearchOptimization::pruningEnabled()
{
    return (policy.load(std::memory_order_relaxed) & Smart) != 0;
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
    return policy.load(std::memory_order_relaxed) == (Smart | Gpu);
}
