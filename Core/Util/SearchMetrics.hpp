#ifndef SEARCHMETRICS_HPP
#define SEARCHMETRICS_HPP

// Only the isolated instrumentation executable defines this flag. Normal app
// builds contain no counters, increments, or instrumentation runtime branches.
#ifdef POKEFINDER_SEARCH_INSTRUMENTATION
#include <atomic>
#include <cstdint>
namespace SearchMetrics
{
    inline std::atomic<std::uint64_t> frames { 0 }, ivChecks { 0 }, earlyIVRejected { 0 },
        earlyHiddenPowerRejected { 0 }, statesConstructed { 0 }, accepted { 0 };
    inline std::atomic<std::uint64_t> wildIVCandidates { 0 }, wildPIDFrames { 0 },
        wildEarlyFrameRejected { 0 }, wildResultReallocations { 0 }, wildExistingIVRejected { 0 };
    inline std::atomic<std::uint64_t> wildSeedCandidates { 0 }, wildPreMTRejected { 0 },
        wildMTInitializations { 0 }, wildReducedMTInitializations { 0 }, wildIVSurvivors { 0 };
    inline void reset()
    {
        frames = 0; ivChecks = 0; earlyIVRejected = 0;
        earlyHiddenPowerRejected = 0; statesConstructed = 0; accepted = 0;
        wildIVCandidates = 0; wildPIDFrames = 0; wildEarlyFrameRejected = 0; wildResultReallocations = 0; wildExistingIVRejected = 0;
        wildSeedCandidates = 0; wildPreMTRejected = 0; wildMTInitializations = 0;
        wildReducedMTInitializations = 0; wildIVSurvivors = 0;
    }
}
#define POKEFINDER_SEARCH_COUNT(name) SearchMetrics::name.fetch_add(1, std::memory_order_relaxed)
#else
#define POKEFINDER_SEARCH_COUNT(name) ((void)0)
#endif

#endif
