#ifndef SEARCHOPTIMIZATION_HPP
#define SEARCHOPTIMIZATION_HPP

namespace SearchOptimization
{
    inline constexpr bool DefaultPruning = false;
    void setPruningEnabled(bool enabled);
    bool pruningEnabled();
}

#endif // SEARCHOPTIMIZATION_HPP
