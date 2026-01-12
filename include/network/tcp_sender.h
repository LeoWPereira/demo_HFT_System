#pragma once

#include <string>
#include <cstdint>
#include <atomic>
#include <thread>
#include <queue>
#include <mutex>
#include <sys/socket.h>

namespace hft {

// Order message structure
struct Order {
    enum class Side : uint8_t {
        BUY = 0,
        SELL = 1
    };
    
    enum class Type : uint8_t {
        LIMIT = 0,
        MARKET = 1,
        IOC = 2  // Immediate or Cancel
    };
    
    char symbol[16];
    uint64_t order_id;
    Side side;
    Type type;
    double price;
    double quantity;
    uint64_t timestamp;
} __attribute__((packed));

// TCP sender for order submission
// Uses optimizations for low-latency order placement
class TCPSender {
public:
    TCPSender(const std::string& host, uint16_t port);
    ~TCPSender();
    
    // Connect to order gateway
    bool connect();
    
    // Disconnect
    void disconnect();
    
    // Send order (critical path - must be fast!)
    bool send_order(const Order& order);
    
    // Enable TCP optimizations
    void enable_tcp_optimizations();
    
    // Set CPU affinity
    void set_cpu_affinity(int cpu);
    
    // Get connection status
    bool is_connected() const { return connected_.load(std::memory_order_acquire); }
    
private:
    std::string host_;
    uint16_t port_;
    int socket_fd_ = -1;
    std::atomic<bool> connected_{false};
    int cpu_affinity_ = -1;
    
    // Setup socket with low-latency options
    void optimize_socket();
};

} // namespace hft
