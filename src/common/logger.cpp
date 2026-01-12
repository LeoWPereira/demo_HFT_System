#include "common/logger.h"
#include <cstring>
#include <iostream>
#include <thread>

namespace hft {

Logger::Logger() {
    // Start background flush thread
    std::thread flush_thread(&Logger::flush_worker, this);
    flush_thread.detach();
}

Logger::~Logger() {
    running_.store(false, std::memory_order_release);
    flush();
}

void Logger::log(Level level, const char* message) {
    if (level < min_level_.load(std::memory_order_relaxed)) {
        return;
    }
    
    // Get next write position (lock-free)
    size_t index = write_index_.fetch_add(1, std::memory_order_acq_rel) % BUFFER_SIZE;
    
    auto& entry = buffer_[index];
    entry.timestamp = Timestamp::wall_clock_ns();
    entry.level = level;
    std::strncpy(entry.message, message, sizeof(entry.message) - 1);
    entry.message[sizeof(entry.message) - 1] = '\0';
}

void Logger::flush() {
    // In production, this would write to disk or network
    // For now, just print to stdout
    for (size_t i = 0; i < BUFFER_SIZE; ++i) {
        const auto& entry = buffer_[i];
        if (entry.timestamp > 0) {
            std::cout << entry.timestamp << " [" << static_cast<int>(entry.level) 
                      << "] " << entry.message << std::endl;
        }
    }
}

void Logger::flush_worker() {
    while (running_.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        // In production, would flush to disk here
    }
}

} // namespace hft
