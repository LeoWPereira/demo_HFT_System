#include "trading/strategy.h"
#include "common/timestamp.h"
#include "common/logger.h"
#include <cstring>
#include <cmath>

namespace hft {

// ============================================================================
// Market Making Strategy Implementation
// ============================================================================

MarketMakingStrategy::MarketMakingStrategy(TCPSender& order_sender, 
                                           const Parameters& params)
    : order_sender_(order_sender)
    , params_(params) {
}

void MarketMakingStrategy::on_order_book_update(const OrderBook& book) {
    // Get snapshot of order book
    auto snapshot = book.get_snapshot();
    
    // Check if we should update our quotes
    if (should_requote(snapshot)) {
        update_quotes(snapshot);
    }
}

void MarketMakingStrategy::on_timer() {
    // Periodic tasks (risk checks, position rebalancing, etc.)
    // For now, just a placeholder
}

bool MarketMakingStrategy::should_requote(const OrderBook::Snapshot& snapshot) {
    // Don't quote if spread is too wide (might indicate illiquid market)
    if (snapshot.spread_bps() > 10.0) {
        return false;
    }
    
    // Rate limiting: don't quote too frequently
    uint64_t now = Timestamp::now();
    uint64_t last_quote = last_quote_time_.load(std::memory_order_relaxed);
    
    // Minimum 100 microseconds between quotes
    constexpr uint64_t MIN_QUOTE_INTERVAL_NS = 100000;
    if (Timestamp::to_nanoseconds(now - last_quote) < MIN_QUOTE_INTERVAL_NS) {
        return false;
    }
    
    return true;
}

double MarketMakingStrategy::calculate_fair_value(double mid, double position) const {
    // Adjust fair value based on inventory
    // If we're long, shade our fair value down to encourage selling
    // If we're short, shade it up to encourage buying
    double skew = -position / params_.max_position * params_.skew_factor;
    return mid * (1.0 + skew);
}

void MarketMakingStrategy::update_quotes(const OrderBook::Snapshot& snapshot) {
    double mid = snapshot.mid_price();
    if (mid <= 0) {
        return;
    }
    
    double position = position_.load(std::memory_order_relaxed);
    
    // Check position limits
    if (std::abs(position) >= params_.max_position) {
        // Hit position limit - don't quote in the direction that increases position
        return;
    }
    
    // Calculate fair value with inventory skew
    double fair_value = calculate_fair_value(mid, position);
    
    // Calculate bid/ask prices with target spread
    double half_spread = fair_value * params_.spread_target / 2.0;
    double bid_price = fair_value - half_spread - params_.edge * fair_value;
    double ask_price = fair_value + half_spread + params_.edge * fair_value;
    
    // Create orders
    Order bid_order;
    std::memset(&bid_order, 0, sizeof(bid_order));
    std::strncpy(bid_order.symbol, snapshot.bids[0].price > 0 ? "SYMBOL" : "SYMBOL", 
                 sizeof(bid_order.symbol) - 1);
    bid_order.order_id = generate_order_id();
    bid_order.side = Order::Side::BUY;
    bid_order.type = Order::Type::LIMIT;
    bid_order.price = bid_price;
    bid_order.quantity = params_.quote_size;
    bid_order.timestamp = Timestamp::now();
    
    Order ask_order;
    std::memset(&ask_order, 0, sizeof(ask_order));
    std::strncpy(ask_order.symbol, "SYMBOL", sizeof(ask_order.symbol) - 1);
    ask_order.order_id = generate_order_id();
    ask_order.side = Order::Side::SELL;
    ask_order.type = Order::Type::LIMIT;
    ask_order.price = ask_price;
    ask_order.quantity = params_.quote_size;
    ask_order.timestamp = Timestamp::now();
    
    // Send orders (this is the critical path!)
    LatencyMeasure latency;
    
    if (position < params_.max_position * 0.8) {
        order_sender_.send_order(bid_order);
    }
    
    if (position > -params_.max_position * 0.8) {
        order_sender_.send_order(ask_order);
    }
    
    // Update timestamp
    last_quote_time_.store(Timestamp::now(), std::memory_order_relaxed);
    
    // Log latency (for monitoring)
    uint64_t order_latency_ns = latency.elapsed_ns();
    if (order_latency_ns > 10000) { // More than 10 microseconds
        LOG_WARN("High order latency detected");
    }
}

// ============================================================================
// Arbitrage Strategy Implementation (placeholder)
// ============================================================================

ArbitrageStrategy::ArbitrageStrategy(TCPSender& order_sender)
    : order_sender_(order_sender) {
}

void ArbitrageStrategy::on_order_book_update(const OrderBook& book) {
    // Would implement arbitrage logic here
    // Example: ETF vs constituent stocks, futures vs spot, etc.
    (void)book; // Suppress unused warning
}

void ArbitrageStrategy::on_timer() {
    // Periodic checks
}

} // namespace hft
