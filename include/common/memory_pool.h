#pragma once

#include <atomic>
#include <cstdint>
#include <new>
#include <array>

namespace hft {

// Lock-free memory pool allocator
// Pre-allocates memory to avoid malloc/free in hot path
// Uses lock-free stack for recycling
template<typename T, size_t PoolSize>
class MemoryPool {
public:
    MemoryPool() {
        // Initialize free list
        for (size_t i = 0; i < PoolSize; ++i) {
            free_list_[i].store(i, std::memory_order_relaxed);
        }
        free_count_.store(PoolSize, std::memory_order_relaxed);
    }
    
    ~MemoryPool() {
        // Destroy all allocated objects
        for (size_t i = 0; i < PoolSize; ++i) {
            if (allocated_[i]) {
                reinterpret_cast<T*>(&storage_[i])->~T();
            }
        }
    }
    
    // Allocate object (construct in-place)
    template<typename... Args>
    T* allocate(Args&&... args) {
        size_t count = free_count_.load(std::memory_order_acquire);
        
        while (count > 0) {
            if (free_count_.compare_exchange_weak(count, count - 1,
                std::memory_order_acq_rel)) {
                
                // Pop from free list
                size_t idx = free_list_[count - 1].load(std::memory_order_acquire);
                allocated_[idx] = true;
                
                // Construct object in-place
                T* ptr = reinterpret_cast<T*>(&storage_[idx]);
                new (ptr) T(std::forward<Args>(args)...);
                
                return ptr;
            }
            count = free_count_.load(std::memory_order_acquire);
        }
        
        return nullptr; // Pool exhausted
    }
    
    // Deallocate object
    void deallocate(T* ptr) {
        if (!ptr) return;
        
        // Destroy object
        ptr->~T();
        
        // Calculate index
        size_t idx = (reinterpret_cast<char*>(ptr) - 
                     reinterpret_cast<char*>(storage_)) / sizeof(Storage);
        
        allocated_[idx] = false;
        
        // Push back to free list
        size_t count = free_count_.load(std::memory_order_acquire);
        free_list_[count].store(idx, std::memory_order_release);
        free_count_.fetch_add(1, std::memory_order_release);
    }
    
    // Get available count
    size_t available() const {
        return free_count_.load(std::memory_order_acquire);
    }
    
    // Check if pointer belongs to this pool
    bool owns(T* ptr) const {
        return ptr >= reinterpret_cast<const T*>(storage_) &&
               ptr < reinterpret_cast<const T*>(storage_ + PoolSize);
    }
    
private:
    // Storage with proper alignment
    struct alignas(alignof(T)) Storage {
        unsigned char data[sizeof(T)];
    };
    
    Storage storage_[PoolSize];
    bool allocated_[PoolSize]{};
    
    // Lock-free free list
    alignas(64) std::atomic<size_t> free_count_;
    std::array<std::atomic<size_t>, PoolSize> free_list_;
};

// RAII wrapper for pool-allocated objects
template<typename T, size_t PoolSize>
class PoolPtr {
public:
    PoolPtr() : ptr_(nullptr), pool_(nullptr) {}
    
    PoolPtr(T* ptr, MemoryPool<T, PoolSize>* pool) 
        : ptr_(ptr), pool_(pool) {}
    
    ~PoolPtr() {
        if (ptr_ && pool_) {
            pool_->deallocate(ptr_);
        }
    }
    
    // Move semantics
    PoolPtr(PoolPtr&& other) noexcept 
        : ptr_(other.ptr_), pool_(other.pool_) {
        other.ptr_ = nullptr;
        other.pool_ = nullptr;
    }
    
    PoolPtr& operator=(PoolPtr&& other) noexcept {
        if (this != &other) {
            if (ptr_ && pool_) {
                pool_->deallocate(ptr_);
            }
            ptr_ = other.ptr_;
            pool_ = other.pool_;
            other.ptr_ = nullptr;
            other.pool_ = nullptr;
        }
        return *this;
    }
    
    // Delete copy
    PoolPtr(const PoolPtr&) = delete;
    PoolPtr& operator=(const PoolPtr&) = delete;
    
    T* get() const { return ptr_; }
    T* operator->() const { return ptr_; }
    T& operator*() const { return *ptr_; }
    
    explicit operator bool() const { return ptr_ != nullptr; }
    
private:
    T* ptr_;
    MemoryPool<T, PoolSize>* pool_;
};

} // namespace hft
