#include "common/timestamp.h"
#include <thread>

namespace hft {

double Timestamp::calibrate_tsc_frequency() {
    // Measure TSC frequency by comparing with wall clock
    auto wall_start = std::chrono::high_resolution_clock::now();
    auto tsc_start = now();
    
    // Sleep for a short duration
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto tsc_end = now();
    auto wall_end = std::chrono::high_resolution_clock::now();
    
    auto wall_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
        wall_end - wall_start).count();
    auto tsc_duration = tsc_end - tsc_start;
    
    tsc_frequency_ = static_cast<double>(tsc_duration) / wall_duration * 1e9;
    return tsc_frequency_;
}

} // namespace hft
