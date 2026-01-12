#pragma once

#include <atomic>
#include <array>
#include <cstdint>
#include <limits>
#include "common/timestamp.h"

namespace hft {

// Price level in the order book
// Aligned to cache line to avoid false sharing
struct alignas(64) PriceLevel {
    double price = 0.0;
    double quantity = 0.0;
    uint32_t order_count = 0;
    uint32_t _padding; // Ensure 64-byte alignment
    
    void reset() {
        price = 0.0;
        quantity = 0.0;
        order_count = 0;
    }
};

// Lock-free order book implementation
// Uses atomic operations for thread-safe updates without locks
// This is critical for HFT - locks introduce too much latency
class OrderBook {
public:
    static constexpr size_t MAX_DEPTH = 10;
    
    enum class Side : uint8_t {
        BID = 0,
        ASK = 1
    };
    
    struct Book {
        alignas(64) std::array<PriceLevel, MAX_DEPTH> levels;
        alignas(64) std::atomic<uint32_t> depth{0};
        alignas(64) std::atomic<uint64_t> sequence{0};
    };
    
    OrderBook(const std::string& symbol);
    
    // Update order book (called from market data thread)
    void update_bid(size_t level, double price, double quantity);
    void update_ask(size_t level, double price, double quantity);
    
    // Snapshot access (called from strategy thread)
    // Returns copy to avoid locking - small enough to copy efficiently
    struct Snapshot {
        std::array<PriceLevel, MAX_DEPTH> bids;
        std::array<PriceLevel, MAX_DEPTH> asks;
        uint32_t bid_depth;
        uint32_t ask_depth;
        uint64_t bid_sequence;
        uint64_t ask_sequence;
        uint64_t timestamp;
        
        double best_bid() const { return bid_depth > 0 ? bids[0].price : 0.0; }
        double best_ask() const { return ask_depth > 0 ? asks[0].price : std::numeric_limits<double>::max(); }
        double mid_price() const { return (best_bid() + best_ask()) / 2.0; }
        double spread() const { return best_ask() - best_bid(); }
        double spread_bps() const { 
            double mid = mid_price();
            return mid > 0 ? (spread() / mid) * 10000.0 : 0.0;
        }
    };
    
    Snapshot get_snapshot() const;
    
    // Get top of book (most common operation - highly optimized)
    inline double best_bid() const noexcept {
        return bids_.levels[0].price;
    }
    
    inline double best_ask() const noexcept {
        return asks_.levels[0].price;
    }
    
    inline double mid_price() const noexcept {
        return (best_bid() + best_ask()) / 2.0;
    }
    
    const std::string& symbol() const { return symbol_; }
    
private:
    std::string symbol_;
    alignas(64) Book bids_;
    alignas(64) Book asks_;
    
    // Helper to update a level
    void update_level(Book& book, size_t level, double price, double quantity);
};

} // namespace hft
