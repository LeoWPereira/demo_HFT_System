#include <atomic>
#include <thread>
#include <vector>
#include <iostream>
#include <cassert>

// Test lock-free queue (SPSC - Single Producer Single Consumer)
template<typename T, size_t Size>
class LockFreeQueue {
public:
    LockFreeQueue() : head_(0), tail_(0) {}
    
    bool push(const T& item) {
        size_t current_tail = tail_.load(std::memory_order_relaxed);
        size_t next_tail = (current_tail + 1) % Size;
        
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false; // Queue full
        }
        
        buffer_[current_tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }
    
    bool pop(T& item) {
        size_t current_head = head_.load(std::memory_order_relaxed);
        
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false; // Queue empty
        }
        
        item = buffer_[current_head];
        head_.store((current_head + 1) % Size, std::memory_order_release);
        return true;
    }
    
private:
    alignas(64) std::atomic<size_t> head_;
    alignas(64) std::atomic<size_t> tail_;
    T buffer_[Size];
};

void test_spsc_queue() {
    std::cout << "Testing SPSC lock-free queue...\n";
    
    LockFreeQueue<int, 1024> queue;
    
    constexpr int ITEMS = 100000;
    std::atomic<int> sum_pushed{0};
    std::atomic<int> sum_popped{0};
    
    // Producer thread
    std::thread producer([&queue, &sum_pushed]() {
        for (int i = 0; i < ITEMS; ++i) {
            while (!queue.push(i)) {
                // Busy wait
            }
            sum_pushed.fetch_add(i, std::memory_order_relaxed);
        }
    });
    
    // Consumer thread
    std::thread consumer([&queue, &sum_popped]() {
        int item;
        int count = 0;
        while (count < ITEMS) {
            if (queue.pop(item)) {
                sum_popped.fetch_add(item, std::memory_order_relaxed);
                count++;
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    // Verify all items were transferred correctly
    assert(sum_pushed.load() == sum_popped.load());
    
    std::cout << "✓ SPSC queue test passed\n";
}

void test_atomic_operations() {
    std::cout << "Testing atomic operations and memory ordering...\n";
    
    std::atomic<int> counter{0};
    constexpr int ITERATIONS = 100000;
    constexpr int THREADS = 4;
    
    std::vector<std::thread> threads;
    
    for (int t = 0; t < THREADS; ++t) {
        threads.emplace_back([&counter]() {
            for (int i = 0; i < ITERATIONS; ++i) {
                counter.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    assert(counter.load() == ITERATIONS * THREADS);
    
    std::cout << "✓ Atomic operations test passed\n";
}

void test_memory_ordering() {
    std::cout << "Testing memory ordering (acquire/release)...\n";
    
    std::atomic<int> data{0};
    std::atomic<bool> ready{false};
    
    std::thread writer([&]() {
        data.store(42, std::memory_order_relaxed);
        ready.store(true, std::memory_order_release);
    });
    
    std::thread reader([&]() {
        while (!ready.load(std::memory_order_acquire)) {
            // Wait
        }
        assert(data.load(std::memory_order_relaxed) == 42);
    });
    
    writer.join();
    reader.join();
    
    std::cout << "✓ Memory ordering test passed\n";
}

int main() {
    std::cout << "\n========================================\n";
    std::cout << "   Lock-Free Data Structure Tests\n";
    std::cout << "========================================\n\n";
    
    test_spsc_queue();
    test_atomic_operations();
    test_memory_ordering();
    
    std::cout << "\n✓ All lock-free tests passed!\n\n";
    std::cout << "Key concepts demonstrated:\n";
    std::cout << "1. Lock-free SPSC queue using atomics\n";
    std::cout << "2. Memory ordering (acquire/release semantics)\n";
    std::cout << "3. Cache line alignment (false sharing prevention)\n";
    std::cout << "4. Wait-free operations where possible\n\n";
    
    return 0;
}
