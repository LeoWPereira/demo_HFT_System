#include "market_data/order_book.h"
#include "common/timestamp.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>

namespace hft {

// Latency histogram for performance tracking
class LatencyHistogram {
public:
    void record(uint64_t latency_ns) {
        samples_.push_back(latency_ns);
    }
    
    void print_stats() const {
        if (samples_.empty()) {
            std::cout << "No samples recorded\n";
            return;
        }
        
        auto sorted = samples_;
        std::sort(sorted.begin(), sorted.end());
        
        size_t n = sorted.size();
        double mean = std::accumulate(sorted.begin(), sorted.end(), 0.0) / n;
        
        std::cout << "\n=== Latency Statistics ===\n";
        std::cout << "Samples: " << n << "\n";
        std::cout << "Min:     " << sorted[0] << " ns\n";
        std::cout << "Max:     " << sorted[n-1] << " ns\n";
        std::cout << "Mean:    " << mean << " ns\n";
        std::cout << "Median:  " << sorted[n/2] << " ns\n";
        std::cout << "P95:     " << sorted[size_t(n * 0.95)] << " ns\n";
        std::cout << "P99:     " << sorted[size_t(n * 0.99)] << " ns\n";
        std::cout << "P99.9:   " << sorted[size_t(n * 0.999)] << " ns\n";
        std::cout << "========================\n\n";
    }
    
private:
    std::vector<uint64_t> samples_;
};

} // namespace hft

// Benchmark order book operations
void benchmark_order_book() {
    using namespace hft;
    
    std::cout << "Benchmarking Order Book Operations...\n\n";
    
    OrderBook book("AAPL");
    LatencyHistogram update_latency;
    LatencyHistogram snapshot_latency;
    
    constexpr int ITERATIONS = 100000;
    
    // Benchmark updates
    std::cout << "Running " << ITERATIONS << " order book updates...\n";
    for (int i = 0; i < ITERATIONS; ++i) {
        auto start = Timestamp::now();
        
        book.update_bid(0, 150.00 + i * 0.01, 100.0);
        book.update_ask(0, 150.01 + i * 0.01, 100.0);
        
        auto end = Timestamp::now();
        update_latency.record(Timestamp::to_nanoseconds(end - start));
    }
    
    std::cout << "Order Book Update Latency:\n";
    update_latency.print_stats();
    
    // Benchmark snapshots
    std::cout << "Running " << ITERATIONS << " order book snapshots...\n";
    for (int i = 0; i < ITERATIONS; ++i) {
        auto start = Timestamp::now();
        
        auto snapshot = book.get_snapshot();
        
        auto end = Timestamp::now();
        snapshot_latency.record(Timestamp::to_nanoseconds(end - start));
        
        // Use the snapshot to prevent optimization
        volatile double mid = snapshot.mid_price();
        (void)mid;
    }
    
    std::cout << "Order Book Snapshot Latency:\n";
    snapshot_latency.print_stats();
}

// Benchmark timestamp/RDTSC
void benchmark_timestamp() {
    using namespace hft;
    
    std::cout << "Benchmarking Timestamp Operations...\n\n";
    
    constexpr int ITERATIONS = 1000000;
    LatencyHistogram rdtsc_latency;
    
    std::cout << "Running " << ITERATIONS << " RDTSC calls...\n";
    
    for (int i = 0; i < ITERATIONS; ++i) {
        uint64_t start = Timestamp::now();
        uint64_t end = Timestamp::now();
        
        rdtsc_latency.record(end - start);
    }
    
    std::cout << "RDTSC Overhead (CPU cycles):\n";
    rdtsc_latency.print_stats();
}

// Benchmark cache effects
void benchmark_cache_effects() {
    std::cout << "\nBenchmarking Cache Effects...\n\n";
    
    // This demonstrates why cache line alignment matters
    struct UnalignedData {
        double value1;
        double value2;
    };
    
    struct alignas(64) AlignedData {
        double value1;
        char padding[64 - sizeof(double)];
        double value2;
    };
    
    constexpr int ITERATIONS = 10000000;
    
    // Unaligned access
    UnalignedData unaligned[2] = {{1.0, 2.0}, {3.0, 4.0}};
    auto start = hft::Timestamp::now();
    for (int i = 0; i < ITERATIONS; ++i) {
        unaligned[0].value1 += 1.0;
        unaligned[1].value2 += 1.0;
    }
    auto end = hft::Timestamp::now();
    uint64_t unaligned_time = hft::Timestamp::to_nanoseconds(end - start);
    
    // Aligned access
    AlignedData aligned[2] = {};
    aligned[0].value1 = 1.0;
    aligned[1].value2 = 3.0;
    start = hft::Timestamp::now();
    for (int i = 0; i < ITERATIONS; ++i) {
        aligned[0].value1 += 1.0;
        aligned[1].value2 += 1.0;
    }
    end = hft::Timestamp::now();
    uint64_t aligned_time = hft::Timestamp::to_nanoseconds(end - start);
    
    std::cout << "Unaligned (false sharing): " << unaligned_time << " ns\n";
    std::cout << "Aligned (no false sharing): " << aligned_time << " ns\n";
    std::cout << "Speedup: " << (double)unaligned_time / aligned_time << "x\n\n";
}

int main() {
    std::cout << "\n";
    std::cout << "================================================\n";
    std::cout << "   HFT Trading System Performance Benchmarks   \n";
    std::cout << "================================================\n\n";
    
    // Calibrate TSC
    std::cout << "Calibrating TSC frequency...\n";
    double tsc_freq = hft::Timestamp::calibrate_tsc_frequency();
    std::cout << "TSC Frequency: " << tsc_freq / 1e9 << " GHz\n\n";
    
    benchmark_timestamp();
    benchmark_order_book();
    benchmark_cache_effects();
    
    std::cout << "\nBenchmarks complete!\n\n";
    std::cout << "Key Takeaways for HFT:\n";
    std::cout << "1. RDTSC overhead is typically < 50 CPU cycles\n";
    std::cout << "2. Order book updates should be < 100ns\n";
    std::cout << "3. Cache line alignment prevents false sharing\n";
    std::cout << "4. Lock-free > locks for critical paths\n";
    std::cout << "5. Every nanosecond counts!\n\n";
    
    return 0;
}
