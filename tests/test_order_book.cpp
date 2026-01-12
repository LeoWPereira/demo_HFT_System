#include "market_data/order_book.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <vector>

using namespace hft;

void test_order_book_basic() {
    std::cout << "Testing basic order book operations...\n";
    
    OrderBook book("TEST");
    
    // Add some levels
    book.update_bid(0, 100.00, 500.0);
    book.update_bid(1, 99.99, 300.0);
    book.update_ask(0, 100.01, 400.0);
    book.update_ask(1, 100.02, 200.0);
    
    // Check values
    assert(book.best_bid() == 100.00);
    assert(book.best_ask() == 100.01);
    assert(book.mid_price() == 100.005);
    
    auto snapshot = book.get_snapshot();
    assert(snapshot.bid_depth == 2);
    assert(snapshot.ask_depth == 2);
    assert(snapshot.spread() == 0.01);
    (void)snapshot; // Suppress unused warning
    
    std::cout << "✓ Basic order book test passed\n";
}

void test_order_book_concurrent() {
    std::cout << "Testing concurrent order book access...\n";
    
    OrderBook book("TEST");
    
    // Writer thread
    std::thread writer([&book]() {
        for (int i = 0; i < 10000; ++i) {
            book.update_bid(0, 100.00 + i * 0.001, 100.0);
            book.update_ask(0, 100.01 + i * 0.001, 100.0);
        }
    });
    
    // Reader threads
    std::vector<std::thread> readers;
    for (int t = 0; t < 4; ++t) {
        readers.emplace_back([&book]() {
            for (int i = 0; i < 10000; ++i) {
                auto snapshot = book.get_snapshot();
                // Verify invariants
                assert(snapshot.best_ask() >= snapshot.best_bid());
                (void)snapshot; // Suppress unused warning
            }
        });
    }
    
    writer.join();
    for (auto& reader : readers) {
        reader.join();
    }
    
    std::cout << "✓ Concurrent access test passed\n";
}

void test_order_book_sequence() {
    std::cout << "Testing order book sequence numbers...\n";
    
    OrderBook book("TEST");
    
    auto snap1 = book.get_snapshot();
    book.update_bid(0, 100.00, 100.0);
    auto snap2 = book.get_snapshot();
    
    // Sequence should increment
    assert(snap2.bid_sequence > snap1.bid_sequence);
    (void)snap1; (void)snap2; // Suppress unused warnings
    
    std::cout << "✓ Sequence number test passed\n";
}

int main() {
    std::cout << "\n========================================\n";
    std::cout << "   Order Book Unit Tests\n";
    std::cout << "========================================\n\n";
    
    test_order_book_basic();
    test_order_book_concurrent();
    test_order_book_sequence();
    
    std::cout << "\n✓ All tests passed!\n\n";
    
    return 0;
}
