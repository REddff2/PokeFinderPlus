#include "SearchOptimization.hpp"
#include <atomic>

namespace
{
    std::atomic<bool> enabled { SearchOptimization::DefaultPruning };
}

void SearchOptimization::setPruningEnabled(bool value)
{
    enabled.store(value, std::memory_order_relaxed);
}

bool SearchOptimization::pruningEnabled()
{
    return enabled.load(std::memory_order_relaxed);
}
