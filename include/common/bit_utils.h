#pragma once

#include <cstdint>
#include <type_traits>

namespace hft {

// Bit manipulation utilities for HFT
// Used for flags, compact data storage, fast operations

namespace bits {

// Count trailing zeros (CTZ) - hardware accelerated
inline int count_trailing_zeros(uint64_t x) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_ctzll(x);
#elif defined(_MSC_VER)
    unsigned long idx;
    _BitScanForward64(&idx, x);
    return static_cast<int>(idx);
#else
    // Fallback
    if (x == 0) return 64;
    int count = 0;
    while ((x & 1) == 0) {
        x >>= 1;
        count++;
    }
    return count;
#endif
}

// Count leading zeros (CLZ)
inline int count_leading_zeros(uint64_t x) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_clzll(x);
#elif defined(_MSC_VER)
    unsigned long idx;
    _BitScanReverse64(&idx, x);
    return static_cast<int>(63 - idx);
#else
    if (x == 0) return 64;
    int count = 0;
    while ((x & (1ULL << 63)) == 0) {
        x <<= 1;
        count++;
    }
    return count;
#endif
}

// Population count (number of set bits) - hardware accelerated
inline int popcount(uint64_t x) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_popcountll(x);
#elif defined(_MSC_VER)
    return static_cast<int>(__popcnt64(x));
#else
    // Fallback - Kernighan's method
    int count = 0;
    while (x) {
        x &= x - 1;
        count++;
    }
    return count;
#endif
}

// Fast log2 (floor)
inline int log2_floor(uint64_t x) {
    return 63 - count_leading_zeros(x);
}

// Fast log2 (ceil)
inline int log2_ceil(uint64_t x) {
    if (x <= 1) return 0;
    return 64 - count_leading_zeros(x - 1);
}

// Check if power of 2
inline bool is_power_of_2(uint64_t x) {
    return x != 0 && (x & (x - 1)) == 0;
}

// Next power of 2
inline uint64_t next_power_of_2(uint64_t x) {
    if (x == 0) return 1;
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;
    return x + 1;
}

// Bit set/clear/test (for flags)
template<typename T>
inline void set_bit(T& flags, int bit) {
    flags |= (T(1) << bit);
}

template<typename T>
inline void clear_bit(T& flags, int bit) {
    flags &= ~(T(1) << bit);
}

template<typename T>
inline bool test_bit(T flags, int bit) {
    return (flags & (T(1) << bit)) != 0;
}

template<typename T>
inline void toggle_bit(T& flags, int bit) {
    flags ^= (T(1) << bit);
}

// Extract bits [start, end)
inline uint64_t extract_bits(uint64_t value, int start, int len) {
    uint64_t mask = (1ULL << len) - 1;
    return (value >> start) & mask;
}

// Byte swap (endianness conversion)
inline uint64_t byte_swap_64(uint64_t x) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap64(x);
#elif defined(_MSC_VER)
    return _byteswap_uint64(x);
#else
    return ((x & 0xFF00000000000000ULL) >> 56) |
           ((x & 0x00FF000000000000ULL) >> 40) |
           ((x & 0x0000FF0000000000ULL) >> 24) |
           ((x & 0x000000FF00000000ULL) >>  8) |
           ((x & 0x00000000FF000000ULL) <<  8) |
           ((x & 0x0000000000FF0000ULL) << 24) |
           ((x & 0x000000000000FF00ULL) << 40) |
           ((x & 0x00000000000000FFULL) << 56);
#endif
}

inline uint32_t byte_swap_32(uint32_t x) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap32(x);
#elif defined(_MSC_VER)
    return _byteswap_ulong(x);
#else
    return ((x & 0xFF000000) >> 24) |
           ((x & 0x00FF0000) >>  8) |
           ((x & 0x0000FF00) <<  8) |
           ((x & 0x000000FF) << 24);
#endif
}

// Compact price representation (avoid floating point in some cases)
// Store price as integer ticks
struct CompactPrice {
    uint64_t ticks;  // Price in minimum tick increments
    
    static CompactPrice from_double(double price, double tick_size) {
        return {static_cast<uint64_t>(price / tick_size + 0.5)};
    }
    
    double to_double(double tick_size) const {
        return ticks * tick_size;
    }
    
    bool operator<(const CompactPrice& other) const {
        return ticks < other.ticks;
    }
    
    bool operator>(const CompactPrice& other) const {
        return ticks > other.ticks;
    }
};

// Bit-packed order flags (fits in single byte)
struct OrderFlags {
    uint8_t data = 0;
    
    enum Bits : uint8_t {
        IS_BUY        = 0,  // Buy vs Sell
        IS_IOC        = 1,  // Immediate or Cancel
        IS_POST_ONLY  = 2,  // Post-only (no taking)
        IS_REDUCE     = 3,  // Reduce-only
        IS_FILLED     = 4,  // Fully filled
        IS_CANCELLED  = 5,  // Cancelled
        RESERVED1     = 6,
        RESERVED2     = 7
    };
    
    void set(Bits bit) { set_bit(data, bit); }
    void clear(Bits bit) { clear_bit(data, bit); }
    bool test(Bits bit) const { return test_bit(data, bit); }
    void toggle(Bits bit) { toggle_bit(data, bit); }
};

} // namespace bits

} // namespace hft
