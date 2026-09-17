#ifndef SEARCHOPTIMIZATION_HPP
#define SEARCHOPTIMIZATION_HPP

// Optional Gen 5 paths only. Normal PokeFinder Threads controls all workers.
namespace SearchOptimization
{
    inline constexpr bool DefaultPruning = false;
    inline constexpr bool DefaultGpu = false;

    void setPruningEnabled(bool enabled);
    bool pruningEnabled();
    void setGpuEnabled(bool enabled);
    bool gpuEnabled();
}

#endif
