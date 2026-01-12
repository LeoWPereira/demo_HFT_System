#include "market_data/market_data_handler.h"
#include "common/logger.h"
#include <cstring>

namespace hft {

MarketDataHandler::MarketDataHandler() = default;

MarketDataHandler::~MarketDataHandler() = default;

void MarketDataHandler::register_callback(OrderBookCallback callback) {
    callback_ = std::move(callback);
}

OrderBook* MarketDataHandler::get_order_book(const char* symbol) {
    // Lock-free hash map lookup - O(1), cache-friendly
    OrderBook** book_ptr = order_book_map_.find(symbol);
    return book_ptr ? *book_ptr : nullptr;
}

OrderBook* MarketDataHandler::get_order_book(const std::string& symbol) {
    return get_order_book(symbol.c_str());
}

void MarketDataHandler::add_symbol(const std::string& symbol) {
    if (!get_order_book(symbol.c_str())) {
        // Allocate from memory pool (no heap fragmentation)
        OrderBook* book = order_book_pool_.allocate(symbol);
        if (book) {
            order_book_map_.insert(symbol.c_str(), book);
        }
    }
}

void MarketDataHandler::process_message(const char* data, size_t len) {
    // In production, this would parse binary protocols like ITCH, PITCH, etc.
    // For now, we assume a simple binary format
    
    if (len < sizeof(MarketDataMessage)) {
        return;
    }
    
    // Zero-copy: cast directly to message struct
    // This avoids any memory allocation or copying
    const auto* msg = reinterpret_cast<const MarketDataMessage*>(data);
    
    // Fast hash map lookup (lock-free, cache-friendly)
    OrderBook* book = get_order_book(msg->symbol);
    if (!book) {
        return; // Unknown symbol
    }
    
    // Update the order book
    // This is lock-free and extremely fast (< 100ns typical)
    if (msg->side == 0) {
        book->update_bid(msg->level, msg->price, msg->quantity);
    } else {
        book->update_ask(msg->level, msg->price, msg->quantity);
    }
    
    // Notify callback if registered
    if (callback_) {
        callback_(*book);
    }
}

} // namespace hft
