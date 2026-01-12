#pragma once

#include <string>
#include <unordered_map>
#include <cstdint>

namespace hft {

// Configuration for the trading system
struct Config {
    // Network settings
    std::string market_data_multicast_ip = "239.1.1.1";
    uint16_t market_data_port = 9000;
    std::string order_gateway_ip = "127.0.0.1";
    uint16_t order_gateway_port = 8000;
    
    // CPU affinity
    int market_data_cpu = 1;
    int strategy_cpu = 2;
    int order_manager_cpu = 3;
    
    // Trading parameters
    double max_position_size = 1000.0;
    double max_order_size = 100.0;
    double spread_threshold = 0.0001; // 1 bps
    
    // Performance
    size_t order_book_depth = 10;
    bool enable_kernel_bypass = false;
    
    // Load config from file
    bool load(const std::string& filename);
    
    // Get parameter
    template<typename T>
    T get(const std::string& key) const;
    
private:
    std::unordered_map<std::string, std::string> params_;
};

} // namespace hft
