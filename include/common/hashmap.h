#pragma once

#include <cstdint>
#include <cstring>
#include <array>
#include <atomic>

namespace hft {

// Lock-free hash map with linear probing
// Used for symbol->order book lookups in hot path
// Cache-friendly, no allocations after initialization
template<typename K, typename V, size_t Capacity>
class LockFreeHashMap {
public:
    static constexpr size_t SIZE = Capacity;
    static_assert((SIZE & (SIZE - 1)) == 0, "Capacity must be power of 2");
    
    struct Entry {
        alignas(64) std::atomic<uint64_t> hash{0};
        alignas(64) K key{};
        alignas(64) V value{};
        
        static constexpr uint64_t EMPTY = 0;
        static constexpr uint64_t TOMBSTONE = 1;
    };
    
    LockFreeHashMap() {
        for (auto& entry : entries_) {
            entry.hash.store(Entry::EMPTY, std::memory_order_relaxed);
        }
    }
    
    // Insert or update
    bool insert(const K& key, const V& value) {
        uint64_t h = hash(key);
        if (h == Entry::EMPTY || h == Entry::TOMBSTONE) {
            h = 2; // Start from 2
        }
        
        size_t idx = h & (SIZE - 1);
        
        // Linear probing
        for (size_t i = 0; i < SIZE; ++i) {
            uint64_t expected = Entry::EMPTY;
            
            // Try to claim empty slot
            if (entries_[idx].hash.compare_exchange_strong(
                expected, h, std::memory_order_acq_rel)) {
                entries_[idx].key = key;
                entries_[idx].value = value;
                return true;
            }
            
            // Check if key already exists
            if (entries_[idx].hash.load(std::memory_order_acquire) == h) {
                if (keys_equal(entries_[idx].key, key)) {
                    entries_[idx].value = value; // Update
                    return true;
                }
            }
            
            // Linear probe
            idx = (idx + 1) & (SIZE - 1);
        }
        
        return false; // Table full
    }
    
    // Lookup (most critical path)
    V* find(const K& key) {
        uint64_t h = hash(key);
        if (h == Entry::EMPTY || h == Entry::TOMBSTONE) {
            h = 2;
        }
        
        size_t idx = h & (SIZE - 1);
        
        for (size_t i = 0; i < SIZE; ++i) {
            uint64_t entry_hash = entries_[idx].hash.load(std::memory_order_acquire);
            
            if (entry_hash == Entry::EMPTY) {
                return nullptr;
            }
            
            if (entry_hash == h && keys_equal(entries_[idx].key, key)) {
                return &entries_[idx].value;
            }
            
            idx = (idx + 1) & (SIZE - 1);
        }
        
        return nullptr;
    }
    
private:
    std::array<Entry, SIZE> entries_;
    
    // FNV-1a hash
    static uint64_t hash(const K& key) {
        constexpr uint64_t FNV_OFFSET = 14695981039346656037ULL;
        constexpr uint64_t FNV_PRIME = 1099511628211ULL;
        
        uint64_t hash = FNV_OFFSET;
        const char* data = reinterpret_cast<const char*>(&key);
        
        for (size_t i = 0; i < sizeof(K); ++i) {
            hash ^= static_cast<uint64_t>(data[i]);
            hash *= FNV_PRIME;
        }
        
        return hash;
    }
    
    static bool keys_equal(const K& a, const K& b) {
        return std::memcmp(&a, &b, sizeof(K)) == 0;
    }
};

// Specialization for C-string keys (symbol names)
template<typename V, size_t Capacity>
class LockFreeHashMap<const char*, V, Capacity> {
public:
    static constexpr size_t SIZE = Capacity;
    static constexpr size_t MAX_KEY_LEN = 16;
    
    struct Entry {
        alignas(64) std::atomic<uint64_t> hash{0};
        alignas(64) char key[MAX_KEY_LEN]{};
        alignas(64) V value{};
        
        static constexpr uint64_t EMPTY = 0;
        static constexpr uint64_t TOMBSTONE = 1;
    };
    
    LockFreeHashMap() {
        for (auto& entry : entries_) {
            entry.hash.store(Entry::EMPTY, std::memory_order_relaxed);
        }
    }
    
    bool insert(const char* key, const V& value) {
        uint64_t h = hash_string(key);
        if (h == Entry::EMPTY || h == Entry::TOMBSTONE) {
            h = 2;
        }
        
        size_t idx = h & (SIZE - 1);
        
        for (size_t i = 0; i < SIZE; ++i) {
            uint64_t expected = Entry::EMPTY;
            
            if (entries_[idx].hash.compare_exchange_strong(
                expected, h, std::memory_order_acq_rel)) {
                std::strncpy(entries_[idx].key, key, MAX_KEY_LEN - 1);
                entries_[idx].value = value;
                return true;
            }
            
            if (entries_[idx].hash.load(std::memory_order_acquire) == h) {
                if (std::strcmp(entries_[idx].key, key) == 0) {
                    entries_[idx].value = value;
                    return true;
                }
            }
            
            idx = (idx + 1) & (SIZE - 1);
        }
        
        return false;
    }
    
    V* find(const char* key) {
        uint64_t h = hash_string(key);
        if (h == Entry::EMPTY || h == Entry::TOMBSTONE) {
            h = 2;
        }
        
        size_t idx = h & (SIZE - 1);
        
        for (size_t i = 0; i < SIZE; ++i) {
            uint64_t entry_hash = entries_[idx].hash.load(std::memory_order_acquire);
            
            if (entry_hash == Entry::EMPTY) {
                return nullptr;
            }
            
            if (entry_hash == h && std::strcmp(entries_[idx].key, key) == 0) {
                return &entries_[idx].value;
            }
            
            idx = (idx + 1) & (SIZE - 1);
        }
        
        return nullptr;
    }
    
private:
    std::array<Entry, SIZE> entries_;
    
    static uint64_t hash_string(const char* str) {
        constexpr uint64_t FNV_OFFSET = 14695981039346656037ULL;
        constexpr uint64_t FNV_PRIME = 1099511628211ULL;
        
        uint64_t hash = FNV_OFFSET;
        while (*str) {
            hash ^= static_cast<uint64_t>(*str++);
            hash *= FNV_PRIME;
        }
        
        return hash;
    }
};

} // namespace hft
