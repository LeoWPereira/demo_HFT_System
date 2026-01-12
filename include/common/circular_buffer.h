#pragma once

#include <atomic>
#include <cstdint>
#include <array>

namespace hft {

// Lock-free SPSC circular buffer (ring buffer)
// Single Producer Single Consumer - extremely fast
// Used for market data pipeline
template<typename T, size_t Capacity>
class CircularBuffer {
public:
    static constexpr size_t SIZE = Capacity;
    static_assert((SIZE & (SIZE - 1)) == 0, "Capacity must be power of 2");
    
    CircularBuffer() : head_(0), tail_(0) {}
    
    // Producer: Push (non-blocking)
    bool push(const T& item) {
        size_t current_tail = tail_.load(std::memory_order_relaxed);
        size_t next_tail = (current_tail + 1) & (SIZE - 1);
        
        // Check if full
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;
        }
        
        buffer_[current_tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }
    
    // Consumer: Pop (non-blocking)
    bool pop(T& item) {
        size_t current_head = head_.load(std::memory_order_relaxed);
        
        // Check if empty
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false;
        }
        
        item = buffer_[current_head];
        head_.store((current_head + 1) & (SIZE - 1), std::memory_order_release);
        return true;
    }
    
    // Check if empty (approximate)
    bool empty() const {
        return head_.load(std::memory_order_acquire) == 
               tail_.load(std::memory_order_acquire);
    }
    
    // Size (approximate)
    size_t size() const {
        size_t h = head_.load(std::memory_order_acquire);
        size_t t = tail_.load(std::memory_order_acquire);
        return (t - h) & (SIZE - 1);
    }
    
private:
    alignas(64) std::atomic<size_t> head_;
    alignas(64) std::atomic<size_t> tail_;
    std::array<T, SIZE> buffer_;
};

// Lock-free MPSC circular buffer (Multi Producer Single Consumer)
// Used when multiple threads feed data to one consumer
template<typename T, size_t Capacity>
class MPSCCircularBuffer {
public:
    static constexpr size_t SIZE = Capacity;
    static_assert((SIZE & (SIZE - 1)) == 0, "Capacity must be power of 2");
    
    MPSCCircularBuffer() : head_(0), tail_(0) {
        for (auto& entry : buffer_) {
            entry.sequence.store(0, std::memory_order_relaxed);
        }
    }
    
    // Producers: Push (wait-free for single producer, lock-free for multi)
    bool push(const T& item) {
        size_t tail = tail_.load(std::memory_order_relaxed);
        
        for (;;) {
            size_t pos = tail & (SIZE - 1);
            size_t seq = buffer_[pos].sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(tail);
            
            if (diff == 0) {
                // Slot available, try to claim it
                if (tail_.compare_exchange_weak(tail, tail + 1, 
                    std::memory_order_relaxed)) {
                    buffer_[pos].data = item;
                    buffer_[pos].sequence.store(tail + 1, std::memory_order_release);
                    return true;
                }
            } else if (diff < 0) {
                // Buffer full
                return false;
            } else {
                // Another producer claimed this slot, retry
                tail = tail_.load(std::memory_order_relaxed);
            }
        }
    }
    
    // Consumer: Pop
    bool pop(T& item) {
        size_t head = head_.load(std::memory_order_relaxed);
        
        for (;;) {
            size_t pos = head & (SIZE - 1);
            size_t seq = buffer_[pos].sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(head + 1);
            
            if (diff == 0) {
                // Item available
                if (head_.compare_exchange_weak(head, head + 1,
                    std::memory_order_relaxed)) {
                    item = buffer_[pos].data;
                    buffer_[pos].sequence.store(head + SIZE, std::memory_order_release);
                    return true;
                }
            } else if (diff < 0) {
                // Buffer empty
                return false;
            } else {
                // Item being written, retry
                head = head_.load(std::memory_order_relaxed);
            }
        }
    }
    
private:
    struct Entry {
        alignas(64) std::atomic<size_t> sequence;
        T data;
    };
    
    alignas(64) std::atomic<size_t> head_;
    alignas(64) std::atomic<size_t> tail_;
    std::array<Entry, SIZE> buffer_;
};

} // namespace hft
