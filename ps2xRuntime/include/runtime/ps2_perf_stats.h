#pragma once

#include <atomic>
#include <cstdint>

// Opt-in runtime counters. With PS2X_PERF_STATS=1 the runtime logs and resets
// them every 10 seconds ([perf] lines on stderr), which is enough to compare
// frame rate and VU1 cost between builds without a profiler.
namespace ps2x::perf
{
    inline std::atomic<bool> enabled{false};
    inline std::atomic<uint64_t> vif1Nanoseconds{0}; // time inside processVIF1Data (includes VU1 and GS work it triggers)
    inline std::atomic<uint64_t> vu1Nanoseconds{0};  // time inside VU1 microprograms started by MSCAL
    inline std::atomic<uint64_t> vu1Programs{0};     // MSCAL/MSCALF count
    inline std::atomic<uint64_t> gsFinishes{0};      // GS FINISH events, about one per game frame
}
