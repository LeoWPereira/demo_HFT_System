#pragma once

#include "market_data/order_book.h"
#include "common/hashmap.h"
#include "common/circular_buffer.h"
#include "common/memory_pool.h"
#include <memory>
#include <functional>

namespace hft {

// Callback for order book updates
using OrderBookCallback = std::function<void(const OrderBook&)>;

// Market data handler - processes incoming market data
// In production, this would parse FIX, ITCH, or proprietary protocols
class MarketDataHandler {
public:
    MarketDataHandler();
    ~MarketDataHandler();
    
    // Register callback for order book updates
    void register_callback(OrderBookCallback callback);
    
    // Get order book for a symbol (using lock-free hash map)
    OrderBook* get_order_book(const char* symbol);
    OrderBook* get_order_book(const std::string& symbol);
    
    // Process market data message
    // This is the hot path - must be extremely fast
    void process_message(const char* data, size_t len);
    
    // Add a symbol to track
    void add_symbol(const std::string& symbol);
    
private:
    // Lock-free hash map for O(1) symbol lookup (replaces std::unordered_map)
    LockFreeHashMap<const char*, OrderBook*, 256> order_book_map_;
    
    // Memory pool for order books (avoid heap fragmentation)
    MemoryPool<OrderBook, 256> order_book_pool_;
    
    // Circular buffer for incoming messages (lock-free pipeline)
    CircularBuffer<const char*, 4096> message_queue_;
    
    OrderBookCallback callback_;
    
    // Message parsing (simplified for demo)
    struct MarketDataMessage {
        char symbol[16];
        uint8_t side; // 0=bid, 1=ask
        uint8_t level;
        double price;
        double quantity;
        uint64_t timestamp;
    } __attribute__((packed));
    
    void parse_and_update(const MarketDataMessage* msg);
};

} // namespace hft
