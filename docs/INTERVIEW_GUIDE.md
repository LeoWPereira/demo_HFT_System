# Interview Preparation Guide

This guide covers key concepts and questions for HFT/trading firms like Jump Trading.

## Core HFT Concepts

### 1. **Low Latency Programming**

#### Question: How do you measure latency in C++?

**Answer:**
```cpp
// Use RDTSC (Read Time-Stamp Counter) for nanosecond precision
uint64_t start = __rdtscp(&aux);
// ... code to measure ...
uint64_t end = __rdtscp(&aux);
uint64_t cycles = end - start;
```

Why RDTSC over `std::chrono`?
- Direct CPU instruction (~25 cycles overhead)
- No system calls
- Nanosecond precision
- Deterministic

#### Question: What causes latency in trading systems?

1. **System calls** - Avoid in hot path (1-10 μs)
2. **Context switches** - Pin threads to CPUs
3. **Cache misses** - L1: 4 cycles, L2: 12 cycles, L3: 30 cycles, RAM: 100+ cycles
4. **Lock contention** - Use lock-free data structures
5. **Memory allocation** - Pre-allocate, use pools
6. **Network stack** - Kernel bypass (DPDK, Solarflare)
7. **Branch mispredictions** - Profile and optimize hot paths

### 2. **Lock-Free Programming**

#### Question: Explain memory ordering in C++

```cpp
// Release-Acquire ordering example
std::atomic<bool> ready{false};
std::atomic<int> data{0};

// Writer thread
data.store(42, std::memory_order_relaxed);
ready.store(true, std::memory_order_release);  // Everything before is visible

// Reader thread
while (!ready.load(std::memory_order_acquire)); // Synchronizes with release
assert(data.load(std::memory_order_relaxed) == 42);
```

Memory orderings:
- **relaxed**: No ordering guarantees, just atomicity
- **acquire**: Reads before this can't move after
- **release**: Writes after this can't move before
- **seq_cst**: Total ordering (expensive!)

#### Question: What is the ABA problem?

Problem: Value changes from A→B→A, CAS thinks nothing changed

```cpp
// Thread 1
Node* old_head = head.load();
// ... preempted ...
head.compare_exchange_strong(old_head, old_head->next);  // Succeeds but wrong!

// Thread 2 (while Thread 1 preempted)
pop(); pop(); push(new_node_at_same_address_as_old_head);
```

Solutions:
- Use tagged pointers (include version counter)
- Hazard pointers
- Epoch-based reclamation

### 3. **Cache Optimization**

#### Question: What is false sharing and how do you prevent it?

**False Sharing:** Multiple threads accessing different data on same cache line

```cpp
// BAD - False sharing
struct Data {
    std::atomic<int> counter1;  // Same cache line
    std::atomic<int> counter2;  // causes thrashing
};

// GOOD - Properly aligned
struct alignas(64) Data {
    std::atomic<int> counter1;
    char padding[64 - sizeof(std::atomic<int>)];
    std::atomic<int> counter2;
};
```

Cache line size: 64 bytes on x86

#### Question: How do you optimize for cache?

1. **Spatial locality** - Access contiguous memory
2. **Temporal locality** - Reuse recently accessed data
3. **Prefetching** - `__builtin_prefetch(addr)`
4. **Structure packing** - Put hot fields together
5. **Avoid indirection** - Each pointer dereference = potential cache miss

### 4. **Network Programming**

#### Question: TCP vs UDP for market data?

**UDP for market data:**
- Lower latency (no handshake, no retransmission)
- Multicast support (one-to-many)
- Acceptable packet loss (stale data anyway)
- Typical: 50-100 μs latency

**TCP for orders:**
- Reliability required (can't lose orders!)
- Guaranteed delivery and ordering
- Use TCP_NODELAY (disable Nagle's)
- Typical: 100-500 μs latency

#### Question: What is kernel bypass?

**Problem:** Kernel network stack adds 10-50 μs latency

**Solutions:**
1. **DPDK** - Poll mode drivers, run in userspace
2. **Solarflare OpenOnload** - Kernel bypass for Solarflare NICs
3. **AF_XDP** - Linux kernel bypass
4. **RDMA** - Direct memory access

Benefits: 1-5 μs latency

### 5. **Data Structures**

#### Question: Design a low-latency order book

Requirements:
- Fast insertion/deletion (< 100ns)
- Fast top-of-book access (< 10ns)
- Thread-safe (ideally lock-free)

```cpp
class OrderBook {
    // Array-based for cache locality
    std::array<PriceLevel, MAX_DEPTH> bids_;
    std::array<PriceLevel, MAX_DEPTH> asks_;
    
    // Lock-free updates using atomics
    std::atomic<uint64_t> sequence_;
    
    // For full book: std::map or custom tree
    // For HFT: Usually only need top 10 levels
};
```

Why not `std::map`?
- Heap allocations
- Poor cache locality (tree traversal)
- Lock contention

#### Question: Design a lock-free hash map

**Requirements:**
- O(1) lookups for symbol→order book
- No allocations after initialization
- Cache-friendly

**Implementation:**
```cpp
template<typename K, typename V, size_t Capacity>
class LockFreeHashMap {
    // Linear probing for cache locality
    // Power-of-2 capacity for fast modulo (x & (SIZE-1))
    // FNV-1a hash function
    // Atomic operations for lock-free access
};
```

**Why linear probing?**
- Better cache locality than chaining
- No pointer chasing
- Predictable memory layout

#### Question: Circular buffer vs std::queue?

**Circular Buffer (Ring Buffer):**
```cpp
template<typename T, size_t Capacity>
class CircularBuffer {
    std::array<T, SIZE> buffer_;
    std::atomic<size_t> head_, tail_;
    
    // SPSC: Single Producer Single Consumer
    // No locks, just atomics with proper ordering
};
```

**Advantages:**
- Pre-allocated memory (no malloc)
- Cache-friendly (contiguous memory)
- Lock-free for SPSC
- Constant time operations

**Use cases:**
- Market data pipeline
- Inter-thread message passing
- Event queues

#### Question: Memory pool allocator - why and how?

**Problem:** `malloc`/`new` in hot path is slow (100-1000ns) and causes fragmentation

**Solution:**
```cpp
template<typename T, size_t PoolSize>
class MemoryPool {
    Storage storage_[PoolSize];  // Pre-allocated
    std::atomic<size_t> free_count_;
    std::array<std::atomic<size_t>, PoolSize> free_list_;
    
    T* allocate() {
        // Pop from free list (lock-free)
        // Placement new
        // Return pointer
    }
};
```

**Benefits:**
- Allocation: ~10ns vs ~100ns for malloc
- No fragmentation
- Deterministic performance
- Lock-free recycling

#### Question: Bit manipulation - real use cases in HFT

**Hardware-accelerated operations:**
```cpp
// Population count (number of set bits)
int popcount(uint64_t x) {
    return __builtin_popcountll(x);  // Single instruction!
}

// Count trailing zeros (find first set bit)
int ctz(uint64_t x) {
    return __builtin_ctzll(x);  // Used for fast log2
}

// Count leading zeros
int clz(uint64_t x) {
    return __builtin_clzll(x);
}
```

**Use cases:**
1. **Compact order flags** - Pack 8 booleans in 1 byte
   ```cpp
   struct OrderFlags {
       uint8_t data;
       // IS_BUY, IS_IOC, IS_POST_ONLY, etc.
   };
   ```

2. **Fast log2** - For price level indexing
   ```cpp
   int log2_floor(uint64_t x) {
       return 63 - __builtin_clzll(x);
   }
   ```

3. **Bit sets** - Track active orders
   ```cpp
   uint64_t active_orders = 0;
   set_bit(active_orders, order_id % 64);
   ```

4. **Compact price representation** - Avoid floating point
   ```cpp
   struct CompactPrice {
       uint64_t ticks;  // Price in minimum tick increments
   };
   ```

5. **Network byte order conversion**
   ```cpp
   uint64_t network_order = __builtin_bswap64(host_order);
   ```

#### Question: SPSC vs MPSC queue?

**SPSC (Single Producer Single Consumer):**
- Lock-free with just atomics
- ~10-20ns latency
- Use for thread-to-thread communication

**MPMC (Multi Producer Multi Consumer):**
- Requires more synchronization
- ~50-100ns latency
- Use Boost lockfree or folly

### 6. **System Design**

#### Question: Design a HFT system architecture

```
Exchange → NIC → UDP Receiver → Order Book → Strategy → Order Manager → TCP Sender → Exchange
              |                      ↓           ↓
              └──────────────→ Risk Checks  Latency Monitor
```

**Key decisions:**
1. **Threading model** - Dedicated thread per component, CPU pinned
2. **Communication** - Lock-free queues, shared memory
3. **Risk management** - Pre-trade checks (< 100ns)
4. **Observability** - Non-blocking logging, metrics

## Common Interview Questions

### C++ Specific

1. **Move semantics** - When to use `std::move`?
2. **Smart pointers** - `unique_ptr` vs `shared_ptr` (avoid in hot path!)
3. **Template metaprogramming** - Compile-time optimization
4. **constexpr** - Compute at compile time
5. **Branch prediction** - `[[likely]]`, `[[unlikely]]`

### System Design

1. **Design a market maker** - See our `MarketMakingStrategy`
2. **Handle market data gaps** - Sequence numbers, request retransmission
3. **Risk management** - Pre-trade checks, position limits, kill switches
4. **Failover** - Hot standby, state replication
5. **Testing** - Unit tests, fuzz testing, replay harness

### Behavioral

1. **Time under pressure** - Describe a critical production issue
2. **Optimization mindset** - Show profile → optimize → measure cycle
3. **Attention to detail** - One bug = millions lost
4. **Teamwork** - Cross-functional (quants, traders, ops)

## Coding Exercises

### Exercise 1: Implement lock-free SPSC queue

See `tests/test_lockfree.cpp` for implementation.

### Exercise 2: Optimize order book

Given:
```cpp
void update_level(int level, double price, double qty);
```

Optimize for:
- Cache locality
- Lock-free access
- Minimal latency

### Exercise 3: Calculate VWAP efficiently

```cpp
double calculate_vwap(const vector<Trade>& trades);
```

Considerations:
- Avoid divisions in loop
- Use FMA (fused multiply-add)
- SIMD vectorization

## Resources

### Books
1. **"The Art of Multiprocessor Programming"** - Herlihy & Shavit
2. **"C++ Concurrency in Action"** - Anthony Williams
3. **"Systems Performance"** - Brendan Gregg

### Papers
1. **"What Every Programmer Should Know About Memory"** - Ulrich Drepper
2. **"Intel 64 and IA-32 Architectures Optimization Reference Manual"**

### Tools
1. **perf** - CPU profiling, cache analysis
2. **valgrind --tool=cachegrind** - Cache simulation
3. **Intel VTune** - Advanced profiling
4. **gdb with TSC** - Time-travel debugging

## Red Flags to Avoid

❌ Using `std::mutex` in hot path  
❌ `malloc`/`new` in critical path  
❌ Virtual functions in latency-sensitive code  
❌ Exception handling in hot path  
❌ Not measuring performance  
❌ Premature optimization (but know when to optimize!)  
❌ Ignoring memory ordering  

## Good Practices

✅ Profile before optimizing  
✅ Measure everything (TSC, perf counters)  
✅ CPU pinning for deterministic performance  
✅ Lock-free > locks for hot path  
✅ Cache-friendly data structures  
✅ Zero-copy where possible  
✅ Understand hardware (cache, NUMA, NIC)  
✅ Test under load  

## Sample Interview Timeline

1. **Phone Screen (45 min)** - Coding basics, system design overview
2. **Onsite Round 1 (60 min)** - Low-latency C++ coding
3. **Onsite Round 2 (60 min)** - System design (HFT system)
4. **Onsite Round 3 (60 min)** - Behavioral + architecture
5. **Onsite Round 4 (45 min)** - Trade/quant discussion

## Final Tips

1. **Know your code** - Be able to explain every optimization
2. **Show measurement** - "I measured X, optimized Y, got Z improvement"
3. **Trade-offs** - No solution is perfect, discuss pros/cons
4. **Production mindset** - Think about monitoring, debugging, failover
5. **Ask questions** - Show curiosity about their tech stack

Good luck! 🚀
