#pragma once

#include "market_data/market_data_handler.h"
#include <atomic>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>

namespace hft {

// UDP receiver for market data
// Uses kernel bypass techniques for low latency
class UDPReceiver {
public:
    UDPReceiver(MarketDataHandler& handler, 
                const std::string& multicast_ip,
                uint16_t port);
    ~UDPReceiver();
    
    // Start receiving (spawns dedicated thread)
    void start();
    
    // Stop receiving
    void stop();
    
    // Set CPU affinity for receiver thread
    void set_cpu_affinity(int cpu);
    
    // Enable kernel bypass optimizations
    void enable_kernel_bypass();
    
private:
    MarketDataHandler& handler_;
    std::string multicast_ip_;
    uint16_t port_;
    int socket_fd_ = -1;
    std::atomic<bool> running_{false};
    std::thread receiver_thread_;
    int cpu_affinity_ = -1;
    bool kernel_bypass_enabled_ = false;
    
    // Main receive loop (runs in dedicated thread)
    void receive_loop();
    
    // Setup socket with low-latency options
    bool setup_socket();
    
    // Apply socket optimizations
    void optimize_socket();
};

} // namespace hft
