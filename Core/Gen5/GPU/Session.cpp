#include "Session.hpp"
#include <Core/Util/SearchOptimization.hpp>
#include <mutex>
#include <filesystem>
#include <nlohmann/json.hpp>
#ifdef _WIN32
#include "Backend.hpp"
#endif

namespace GpuWild
{
    struct Session::Impl
    {
        Options options;
        std::once_flag initialize;
        std::atomic<unsigned> attempts { 0 };
        mutable std::mutex mutex;
        std::string failure;
#ifdef _WIN32
        std::unique_ptr<Backend> backend;
#endif
        explicit Impl(Options options) : options(std::move(options)) {}
    };

    Session::Session(Options options) : impl(std::make_unique<Impl>(std::move(options))) {}
    Session::~Session() = default;

    std::optional<std::vector<u32>> Session::filter(const std::vector<u64> &seeds, const std::atomic<bool> &cancelled, const IVBounds &bounds)
    {
        if (!bounds.valid() || !SearchOptimization::gpuEnabled() || cancelled.load(std::memory_order_relaxed)) return std::nullopt;
        try
        {
            std::call_once(impl->initialize, [&] {
                // Check again after acquiring initialization ownership.
                if (!SearchOptimization::gpuEnabled() || cancelled.load(std::memory_order_relaxed)) return;
                ++impl->attempts;
                std::lock_guard lock(impl->mutex);
                try
                {
#ifdef _WIN32
                    wchar_t path[32768];
                    const auto count = GetModuleFileNameW(nullptr, path, 32768);
                    if (!count || count == 32768) throw std::runtime_error("Cannot locate GPU helper directory");
                    const auto directory = std::filesystem::path(path).parent_path();
                    const auto &options = impl->options;
                    auto helper = options.helperPath.empty() ? (directory / L"PokeFinderGpuHelper.exe").wstring() : options.helperPath;
                    auto kernel = options.kernelPath.empty() ? (directory / L"Gen5Wild.cl").wstring() : options.kernelPath;
                    impl->backend = std::make_unique<Backend>(options.device, helper, kernel, options.fault,
                                                              options.queues, options.timeoutMs, &cancelled);
#else
                    impl->failure = "GPU helper unavailable on this platform; CPU fallback";
#endif
                }
                catch (const std::exception &error) { impl->failure = error.what(); }
            });
#ifdef _WIN32
            if (impl->backend) return impl->backend->execute(seeds, bounds, false, &cancelled);
#endif
        }
        catch (const std::exception &error)
        {
            std::lock_guard lock(impl->mutex);
            impl->failure = error.what();
        }
        return std::nullopt;
    }

    std::string Session::diagnostics() const
    {
        std::lock_guard lock(impl->mutex);
        nlohmann::json result { { "initialization_attempts", impl->attempts.load() }, { "failure", impl->failure } };
#ifdef _WIN32
        if (impl->backend) result["backend"] = impl->backend->report();
#endif
        return result.dump();
    }
}
