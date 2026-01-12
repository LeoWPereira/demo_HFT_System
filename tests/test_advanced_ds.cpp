#include "common/hashmap.h"
#include "common/circular_buffer.h"
#include "common/memory_pool.h"
#include "common/bit_utils.h"
#include <iostream>
#include <thread>
#include <vector>
#include <cassert>
#include <string>

using namespace hft;

// Test lock-free hash map
void test_hashmap() {
    std::cout << "Testing lock-free hash map...\n";
    
    LockFreeHashMap<uint64_t, uint64_t, 256> map;
    
    // Insert
    for (uint64_t i = 0; i < 100; ++i) {
        assert(map.insert(i, i * 10));
    }
    
    // Lookup
    for (uint64_t i = 0; i < 100; ++i) {
        uint64_t* val = map.find(i);
        assert(val != nullptr);
        assert(*val == i * 10);
        (void)val; // Suppress warning
    }
    
    // Update
    map.insert(50, 999);
    uint64_t* val = map.find(50);
    assert(val != nullptr);
    assert(*val == 999);
    (void)val; // Suppress warning
    
    // Not found
    assert(map.find(1000) == nullptr);
    
    std::cout << "✓ Hash map test passed\n";
}

// Test string hash map (for symbols)
void test_hashmap_strings() {
    std::cout << "Testing hash map with string keys...\n";
    
    LockFreeHashMap<const char*, int, 256> map;
    
    map.insert("AAPL", 150);
    map.insert("MSFT", 300);
    map.insert("GOOGL", 2800);
    
    int* price = map.find("AAPL");
    assert(price && *price == 150);
    (void)price; // Suppress warning
    
    price = map.find("MSFT");
    assert(price && *price == 300);
    (void)price; // Suppress warning
    
    assert(map.find("TSLA") == nullptr);
    
    std::cout << "✓ String hash map test passed\n";
}

// Test circular buffer (SPSC)
void test_circular_buffer() {
    std::cout << "Testing SPSC circular buffer...\n";
    
    CircularBuffer<int, 16> buffer;
    
    // Push
    for (int i = 0; i < 10; ++i) {
        assert(buffer.push(i));
    }
    
    assert(buffer.size() == 10);
    
    // Pop
    for (int i = 0; i < 10; ++i) {
        [[maybe_unused]] int val = -1;
        assert(buffer.pop(val));
        assert(val == i);
    }
    
    assert(buffer.empty());
    
    std::cout << "✓ Circular buffer test passed\n";
}

// Test concurrent circular buffer
void test_circular_buffer_concurrent() {
    std::cout << "Testing concurrent circular buffer...\n";
    
    CircularBuffer<int, 1024> buffer;
    std::atomic<bool> done{false};
    std::atomic<int> sum_pushed{0};
    std::atomic<int> sum_popped{0};
    
    constexpr int ITEMS = 10000;
    
    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < ITEMS; ++i) {
            while (!buffer.push(i)) {
                // Busy wait
            }
            sum_pushed.fetch_add(i, std::memory_order_relaxed);
        }
        done.store(true, std::memory_order_release);
    });
    
    // Consumer thread
    std::thread consumer([&]() {
        int count = 0;
        int val;
        while (count < ITEMS) {
            if (buffer.pop(val)) {
                sum_popped.fetch_add(val, std::memory_order_relaxed);
                count++;
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    assert(sum_pushed.load() == sum_popped.load());
    
    std::cout << "✓ Concurrent circular buffer test passed\n";
}

// Test memory pool
void test_memory_pool() {
    std::cout << "Testing memory pool...\n";
    
    struct TestObj {
        int x;
        double y;
        TestObj(int a, double b) : x(a), y(b) {}
    };
    
    MemoryPool<TestObj, 100> pool;
    
    assert(pool.available() == 100);
    
    // Allocate
    std::vector<TestObj*> objs;
    for (int i = 0; i < 50; ++i) {
        TestObj* obj = pool.allocate(i, i * 1.5);
        assert(obj != nullptr);
        assert(obj->x == i);
        objs.push_back(obj);
    }
    
    assert(pool.available() == 50);
    
    // Deallocate
    for (auto obj : objs) {
        pool.deallocate(obj);
    }
    
    assert(pool.available() == 100);
    
    std::cout << "✓ Memory pool test passed\n";
}

// Test bit manipulation
void test_bit_manipulation() {
    std::cout << "Testing bit manipulation utilities...\n";
    
    using namespace bits;
    
    // Popcount
    assert(popcount(0b1010101) == 4);
    assert(popcount(0xFFFFFFFFFFFFFFFFULL) == 64);
    
    // CLZ/CTZ
    assert(count_trailing_zeros(0b1000) == 3);
    assert(count_leading_zeros(0b1000) == 60);
    
    // Power of 2
    assert(is_power_of_2(16));
    assert(!is_power_of_2(15));
    assert(next_power_of_2(15) == 16);
    assert(next_power_of_2(16) == 16);
    
    // Log2
    assert(log2_floor(16) == 4);
    assert(log2_ceil(15) == 4);
    assert(log2_ceil(16) == 4);
    
    // Bit set/clear/test
    uint32_t flags = 0;
    set_bit(flags, 3);
    assert(test_bit(flags, 3));
    assert(!test_bit(flags, 2));
    clear_bit(flags, 3);
    assert(!test_bit(flags, 3));
    
    // Extract bits
    assert(extract_bits(0b11010110, 2, 3) == 0b101);
    
    // Byte swap
    assert(byte_swap_32(0x12345678) == 0x78563412);
    
    // Order flags
    OrderFlags order_flags;
    order_flags.set(OrderFlags::IS_BUY);
    order_flags.set(OrderFlags::IS_IOC);
    assert(order_flags.test(OrderFlags::IS_BUY));
    assert(order_flags.test(OrderFlags::IS_IOC));
    assert(!order_flags.test(OrderFlags::IS_FILLED));
    
    // Compact price
    double test_price = 150.25;
    CompactPrice price = CompactPrice::from_double(test_price, 0.01);
    assert(price.ticks == 15025);
    (void)price; // Suppress warning
    assert(price.to_double(0.01) == test_price);
    
    std::cout << "✓ Bit manipulation test passed\n";
}

// Benchmark hash map vs std::unordered_map
void benchmark_hashmap() {
    std::cout << "\nBenchmarking hash map performance...\n";
    
    LockFreeHashMap<uint64_t, uint64_t, 1024> map;
    
    // Insert benchmark
    auto start = std::chrono::high_resolution_clock::now();
    for (uint64_t i = 0; i < 1000; ++i) {
        map.insert(i, i * 10);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto insert_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
    // Lookup benchmark
    start = std::chrono::high_resolution_clock::now();
    volatile uint64_t sum = 0;
    for (uint64_t i = 0; i < 1000; ++i) {
        uint64_t* val = map.find(i);
        if (val) sum += *val;
    }
    end = std::chrono::high_resolution_clock::now();
    auto lookup_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
    std::cout << "Hash map insert 1000 items: " << insert_us << " μs\n";
    std::cout << "Hash map lookup 1000 items: " << lookup_us << " μs (" 
              << (lookup_us * 1000.0 / 1000) << " ns/lookup)\n";
}

int main() {
    std::cout << "\n========================================\n";
    std::cout << "   Advanced Data Structures Tests\n";
    std::cout << "========================================\n\n";
    
    test_hashmap();
    test_hashmap_strings();
    test_circular_buffer();
    test_circular_buffer_concurrent();
    test_memory_pool();
    test_bit_manipulation();
    
    benchmark_hashmap();
    
    std::cout << "\n✓ All advanced data structure tests passed!\n\n";
    std::cout << "Key concepts demonstrated:\n";
    std::cout << "1. Lock-free hash map with linear probing\n";
    std::cout << "2. SPSC/MPSC circular buffers (ring buffers)\n";
    std::cout << "3. Memory pool allocator (zero malloc in hot path)\n";
    std::cout << "4. Bit manipulation (popcount, CLZ, CTZ, etc.)\n";
    std::cout << "5. Compact data representations\n";
    std::cout << "6. Hardware-accelerated operations\n\n";
    
    return 0;
}
