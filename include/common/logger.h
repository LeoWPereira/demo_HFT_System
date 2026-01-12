#pragma once

#include <atomic>
#include <cstdint>
#include <array>
#include "common/timestamp.h"

namespace hft {

// Lock-free, low-latency logger
// Uses ring buffer to avoid allocations in critical path
class Logger {
public:
    enum class Level : uint8_t {
        DEBUG = 0,
        INFO = 1,
        WARNING = 2,
        ERROR = 3,
        CRITICAL = 4
    };
    
    struct LogEntry {
        uint64_t timestamp;
        Level level;
        char message[256];
    };
    
    static Logger& instance() {
        static Logger logger;
        return logger;
    }
    
    void log(Level level, const char* message);
    
    void set_level(Level level) { min_level_.store(level, std::memory_order_relaxed); }
    
    void flush();
    
private:
    Logger();
    ~Logger();
    
    static constexpr size_t BUFFER_SIZE = 1024 * 16;
    std::array<LogEntry, BUFFER_SIZE> buffer_;
    std::atomic<size_t> write_index_{0};
    std::atomic<Level> min_level_{Level::INFO};
    
    // Background thread for flushing logs
    void flush_worker();
    std::atomic<bool> running_{true};
};

// Convenience macros
#define LOG_DEBUG(msg) hft::Logger::instance().log(hft::Logger::Level::DEBUG, msg)
#define LOG_INFO(msg) hft::Logger::instance().log(hft::Logger::Level::INFO, msg)
#define LOG_WARN(msg) hft::Logger::instance().log(hft::Logger::Level::WARNING, msg)
#define LOG_ERROR(msg) hft::Logger::instance().log(hft::Logger::Level::ERROR, msg)

} // namespace hft
