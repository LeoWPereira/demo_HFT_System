#pragma once

#include <cstdint>
#include <chrono>

// Platform-specific includes
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
    #include <x86intrin.h>
    #define HFT_X86
#elif defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
    #define HFT_ARM
#endif

namespace hft {

// High-precision timestamp using hardware counters
// This is critical for HFT - we need nanosecond precision
class Timestamp {
public:
    using value_type = uint64_t;
    
    // Get current timestamp using hardware counter
    // x86: RDTSC (< 25 CPU cycles)
    // ARM: CNTVCT_EL0 virtual counter (< 20 CPU cycles)
    static inline value_type now() noexcept {
#if defined(HFT_X86)
        // Use RDTSCP to prevent instruction reordering on x86
        unsigned int aux;
        return __rdtscp(&aux);
#elif defined(HFT_ARM)
        // Use ARM performance counter
        uint64_t val;
        asm volatile("mrs %0, cntvct_el0" : "=r"(val));
        return val;
#else
        // Fallback to chrono (slower but portable)
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count();
#endif
    }
    
    // Convert TSC to nanoseconds
    static inline uint64_t to_nanoseconds(value_type tsc) noexcept {
        // This would be calibrated at startup
        // For now, assume 3.0 GHz CPU
        constexpr double tsc_freq = 3.0e9;
        return static_cast<uint64_t>(tsc / tsc_freq * 1e9);
    }
    
    // Get wall clock time (slower but needed for logging)
    static inline uint64_t wall_clock_ns() noexcept {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count();
    }
    
    // Calibrate TSC frequency (called once at startup)
    static double calibrate_tsc_frequency();
    
private:
    static inline double tsc_frequency_ = 0.0;
};

// Measures latency of a code block
class LatencyMeasure {
public:
    LatencyMeasure() : start_(Timestamp::now()) {}
    
    ~LatencyMeasure() = default;
    
    uint64_t elapsed_tsc() const noexcept {
        return Timestamp::now() - start_;
    }
    
    uint64_t elapsed_ns() const noexcept {
        return Timestamp::to_nanoseconds(elapsed_tsc());
    }
    
private:
    Timestamp::value_type start_;
};

} // namespace hft
