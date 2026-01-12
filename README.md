# High-Frequency Trading System

A production-grade, low-latency HFT trading system demonstrating concepts used at HFT firms.

## Key Features

### 🚀 Ultra-Low Latency Design
- **Lock-free data structures** for concurrent access
- **Cache-optimized memory layout** with proper alignment
- **Zero-copy message processing** where possible
- **Custom memory allocators** to avoid heap fragmentation
- **CPU pinning** for consistent performance

### 🌐 Network Stack
- **UDP multicast** for market data ingestion (low latency)
- **TCP** for order placement with proper error handling
- **Kernel bypass concepts** (SO_BUSY_POLL, raw sockets)
- **Timestamping** at hardware level when available

### 📊 Core Components
1. **Order Book Engine** - Lock-free, sub-microsecond updates
2. **Market Data Handler** - UDP receiver with zero-copy parsing
3. **Trading Strategy** - Market making example with inventory management
4. **Order Manager** - Risk checks and order lifecycle
5. **Performance Monitor** - Latency histograms and metrics

## Architecture

```
┌─────────────────┐
│  Market Data    │ UDP Multicast (Market Data)
│  Feed (UDP)     │────────┐
└─────────────────┘        │
                           ▼
                    ┌──────────────┐
                    │  Order Book  │ Lock-free, Cache-optimized
                    │    Engine    │
                    └──────────────┘
                           │
                           ▼
                    ┌──────────────┐
                    │   Strategy   │ Market Making/Arbitrage
                    │    Engine    │
                    └──────────────┘
                           │
                           ▼
                    ┌──────────────┐
                    │    Order     │ Risk Management
                    │   Manager    │
                    └──────────────┘
                           │
                           ▼
┌─────────────────┐
│  Order Gateway  │ TCP (Order Submission)
│     (TCP)       │
└─────────────────┘
```

## Building

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

## Running

```bash
# Run the trading system
./hft_trading --config ../config/trading.conf

# Run benchmarks
./benchmark

# Run tests
./tests
```

## Performance Optimizations

### 1. Memory Layout
- Structure packing and alignment to avoid false sharing
- Pre-allocated memory pools for critical paths
- NUMA-aware memory allocation

### 2. CPU Optimization
- Thread affinity to specific cores
- Isolated CPUs (via kernel boot params)
- Busy-polling instead of interrupts

### 3. Network Tuning
- SO_BUSY_POLL for lower latency
- Large ring buffers
- Interrupt coalescing disabled
- Direct cache access (DCA) when available

### 4. Lock-Free Programming
- Atomic operations for order book updates
- Memory ordering guarantees (acquire/release semantics)
- SPSC/MPSC queue implementations

## Latency Targets

- **Market Data Processing**: < 500ns (from NIC to order book)
- **Strategy Decision**: < 200ns
- **Order Submission**: < 1μs (from signal to wire)
- **End-to-End**: < 2μs (tick-to-trade)

## Learning Resources

- **Books**: 
  - "C++ Concurrency in Action" by Anthony Williams
  - "The Art of Multiprocessor Programming" by Herlihy & Shavit
  
- **Papers**:
  - Intel® 64 and IA-32 Architectures Optimization Reference Manual
  - "What Every Programmer Should Know About Memory" by Ulrich Drepper
