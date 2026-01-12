#pragma once

#include "market_data/order_book.h"
#include "network/tcp_sender.h"
#include <atomic>
#include <memory>

namespace hft {

// Base trading strategy interface
class Strategy {
public:
    virtual ~Strategy() = default;
    
    // Called when order book is updated
    virtual void on_order_book_update(const OrderBook& book) = 0;
    
    // Called periodically (e.g., every millisecond)
    virtual void on_timer() = 0;
    
    // Strategy name
    virtual const char* name() const = 0;
};

// Market making strategy
// Provides liquidity by quoting both bid and ask
class MarketMakingStrategy : public Strategy {
public:
    struct Parameters {
        double spread_target = 0.0002;  // 2 bps target spread
        double quote_size = 100.0;       // Size to quote
        double max_position = 1000.0;    // Max inventory
        double skew_factor = 0.5;        // How much to skew quotes based on position
        double edge = 0.0001;            // Edge to take (1 bp)
    };
    
    MarketMakingStrategy(TCPSender& order_sender, const Parameters& params);
    
    void on_order_book_update(const OrderBook& book) override;
    void on_timer() override;
    const char* name() const override { return "MarketMaking"; }
    
    // Get current position
    double get_position() const { return position_.load(std::memory_order_relaxed); }
    
    // Get P&L
    double get_pnl() const { return pnl_.load(std::memory_order_relaxed); }
    
private:
    TCPSender& order_sender_;
    Parameters params_;
    
    // State tracking
    alignas(64) std::atomic<double> position_{0.0};
    alignas(64) std::atomic<double> pnl_{0.0};
    alignas(64) std::atomic<uint64_t> last_quote_time_{0};
    
    // Order IDs
    std::atomic<uint64_t> next_order_id_{1};
    
    // Quote management
    void update_quotes(const OrderBook::Snapshot& snapshot);
    bool should_requote(const OrderBook::Snapshot& snapshot);
    
    // Calculate fair value with inventory skew
    double calculate_fair_value(double mid, double position) const;
    
    // Generate order ID
    uint64_t generate_order_id() {
        return next_order_id_.fetch_add(1, std::memory_order_relaxed);
    }
};

// Arbitrage strategy (example)
// Looks for price discrepancies between correlated instruments
class ArbitrageStrategy : public Strategy {
public:
    ArbitrageStrategy(TCPSender& order_sender);
    
    void on_order_book_update(const OrderBook& book) override;
    void on_timer() override;
    const char* name() const override { return "Arbitrage"; }
    
private:
    [[maybe_unused]] TCPSender& order_sender_;
    // Would track multiple order books and look for mispricings
};

} // namespace hft
