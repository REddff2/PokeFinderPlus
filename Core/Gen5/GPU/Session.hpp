#ifndef GEN5_GPU_SESSION_HPP
#define GEN5_GPU_SESSION_HPP

#include <Core/Global.hpp>
#include "IVBounds.hpp"
#include "IVPlan.hpp"
#include <Core/Util/SearchOptimization.hpp>
#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace GpuWild
{
    // Test overrides are constructor inputs, never persistent application settings.
    struct Options
    {
        std::wstring helperPath, kernelPath, fault;
        int device = 0;
        unsigned queues = 1;
        unsigned timeoutMs = 15000;
    };

    // One session per search. Construction does not load/enumerate OpenCL.
    class Session
    {
    public:
        static constexpr u32 BatchSize = 1048576;
        explicit Session(Options options = {});
        ~Session();
        Session(const Session &) = delete;

        // nullopt: replay the complete uncommitted batch on CPU.
        std::optional<std::vector<u32>> filter(const std::vector<u64> &seeds, const std::atomic<bool> &cancelled, const IVBounds &bounds, std::optional<IVPlan> plan = {}, SearchOptimization::Family family = SearchOptimization::Family::Wild);
        std::string diagnostics() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
}
#endif
