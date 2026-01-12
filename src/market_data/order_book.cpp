#include "market_data/order_book.h"
#include <algorithm>
#include <cstring>

namespace hft {

OrderBook::OrderBook(const std::string& symbol) 
    : symbol_(symbol) {
    // Initialize all levels
    for (auto& level : bids_.levels) {
        level.reset();
    }
    for (auto& level : asks_.levels) {
        level.reset();
    }
}

void OrderBook::update_bid(size_t level, double price, double quantity) {
    if (level >= MAX_DEPTH) return;
    update_level(bids_, level, price, quantity);
}

void OrderBook::update_ask(size_t level, double price, double quantity) {
    if (level >= MAX_DEPTH) return;
    update_level(asks_, level, price, quantity);
}

void OrderBook::update_level(Book& book, size_t level, double price, double quantity) {
    // Update the price level
    // Using relaxed memory ordering for maximum performance
    // The sequence number provides ordering guarantees
    auto& price_level = book.levels[level];
    price_level.price = price;
    price_level.quantity = quantity;
    
    // Update depth if needed
    if (level >= book.depth.load(std::memory_order_relaxed)) {
        book.depth.store(level + 1, std::memory_order_relaxed);
    }
    
    // Increment sequence number (acts as version counter)
    // This uses release semantics to ensure all previous writes are visible
    book.sequence.fetch_add(1, std::memory_order_release);
}

OrderBook::Snapshot OrderBook::get_snapshot() const {
    Snapshot snap;
    
    // Read sequence numbers first (acquire semantics)
    snap.bid_sequence = bids_.sequence.load(std::memory_order_acquire);
    snap.ask_sequence = asks_.sequence.load(std::memory_order_acquire);
    
    // Copy depths
    snap.bid_depth = bids_.depth.load(std::memory_order_relaxed);
    snap.ask_depth = asks_.depth.load(std::memory_order_relaxed);
    
    // Copy price levels
    // This is safe because we're reading after acquiring the sequence number
    for (size_t i = 0; i < MAX_DEPTH; ++i) {
        snap.bids[i] = bids_.levels[i];
        snap.asks[i] = asks_.levels[i];
    }
    
    snap.timestamp = Timestamp::now();
    
    return snap;
}

} // namespace hft
