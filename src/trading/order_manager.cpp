#include "trading/order_manager.h"
#include "common/timestamp.h"
#include "common/logger.h"
#include <cmath>

namespace hft {

OrderManager::OrderManager(TCPSender& order_sender)
    : order_sender_(order_sender) {
}

void OrderManager::set_risk_limits(const RiskLimits& limits) {
    limits_ = limits;
}

bool OrderManager::submit_order(const Order& order) {
    // Pre-trade risk checks (must be FAST!)
    // These checks typically take < 100ns
    
    // 1. Check order size
    if (!check_order_size(order.quantity)) {
        LOG_ERROR("Order size exceeds limit");
        return false;
    }
    
    // 2. Check position limit
    if (!check_position_limit(order.side, order.quantity)) {
        LOG_ERROR("Order would exceed position limit");
        return false;
    }
    
    // 3. Check rate limit
    if (!check_rate_limit()) {
        LOG_ERROR("Rate limit exceeded");
        return false;
    }
    
    // 4. Check notional limit
    if (!check_notional_limit(order.price, order.quantity)) {
        LOG_ERROR("Notional limit exceeded");
        return false;
    }
    
    // All checks passed - submit order
    bool success = order_sender_.send_order(order);
    
    if (success) {
        // Update internal state (would be more sophisticated in production)
        // For now, assume immediate fill (unrealistic but for demo)
        double position = position_.load(std::memory_order_relaxed);
        if (order.side == Order::Side::BUY) {
            position_.store(position + order.quantity, std::memory_order_relaxed);
        } else {
            position_.store(position - order.quantity, std::memory_order_relaxed);
        }
    }
    
    return success;
}

bool OrderManager::cancel_order(uint64_t order_id) {
    // Send cancel request
    // In production, this would be a separate message type
    (void)order_id;
    return true;
}

bool OrderManager::check_order_size(double size) {
    return size > 0 && size <= limits_.max_order_size;
}

bool OrderManager::check_position_limit(Order::Side side, double size) {
    double current_position = position_.load(std::memory_order_relaxed);
    double new_position = current_position;
    
    if (side == Order::Side::BUY) {
        new_position += size;
    } else {
        new_position -= size;
    }
    
    return std::abs(new_position) <= limits_.max_position;
}

bool OrderManager::check_rate_limit() {
    update_rate_limiter();
    
    uint32_t count = orders_this_second_.load(std::memory_order_relaxed);
    if (count >= limits_.max_orders_per_second) {
        return false;
    }
    
    orders_this_second_.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool OrderManager::check_notional_limit(double price, double size) {
    double order_notional = price * size;
    double current_notional = notional_.load(std::memory_order_relaxed);
    
    return (current_notional + order_notional) <= limits_.max_notional;
}

void OrderManager::update_rate_limiter() {
    uint64_t now_ns = Timestamp::wall_clock_ns();
    uint64_t last_ns = last_second_timestamp_.load(std::memory_order_relaxed);
    
    // Check if we've moved to a new second
    constexpr uint64_t ONE_SECOND_NS = 1000000000ULL;
    if (now_ns - last_ns > ONE_SECOND_NS) {
        // Reset counter for new second
        orders_this_second_.store(0, std::memory_order_relaxed);
        last_second_timestamp_.store(now_ns, std::memory_order_relaxed);
    }
}

} // namespace hft
