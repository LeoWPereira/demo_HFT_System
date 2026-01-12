#pragma once

#include "trading/strategy.h"
#include "network/tcp_sender.h"
#include <memory>
#include <vector>
#include <atomic>

namespace hft {

// Order manager - handles order lifecycle and risk management
class OrderManager {
public:
    OrderManager(TCPSender& order_sender);
    
    // Submit order with risk checks
    bool submit_order(const Order& order);
    
    // Cancel order
    bool cancel_order(uint64_t order_id);
    
    // Risk limits
    struct RiskLimits {
        double max_order_size = 1000.0;
        double max_position = 10000.0;
        double max_notional = 1000000.0;
        uint32_t max_orders_per_second = 100;
    };
    
    void set_risk_limits(const RiskLimits& limits);
    
    // Get current position
    double get_position() const { return position_.load(std::memory_order_relaxed); }
    
    // Order tracking
    struct OrderInfo {
        uint64_t order_id;
        Order::Side side;
        double price;
        double quantity;
        uint64_t submit_time;
        bool filled;
    };
    
private:
    TCPSender& order_sender_;
    RiskLimits limits_;
    
    // Position tracking
    alignas(64) std::atomic<double> position_{0.0};
    alignas(64) std::atomic<double> notional_{0.0};
    
    // Order rate limiting
    alignas(64) std::atomic<uint32_t> orders_this_second_{0};
    alignas(64) std::atomic<uint64_t> last_second_timestamp_{0};
    
    // Risk checks
    bool check_order_size(double size);
    bool check_position_limit(Order::Side side, double size);
    bool check_rate_limit();
    bool check_notional_limit(double price, double size);
    
    // Update rate limiter
    void update_rate_limiter();
};

} // namespace hft
