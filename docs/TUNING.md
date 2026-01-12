# System Tuning Guide for HFT Trading System

This guide covers system-level optimizations for running HFT applications.

## Linux Kernel Parameters

### 1. CPU Isolation

Isolate CPUs from the scheduler to reduce jitter:

```bash
# Add to kernel boot parameters (e.g., /etc/default/grub)
GRUB_CMDLINE_LINUX="isolcpus=1,2,3 nohz_full=1,2,3 rcu_nocbs=1,2,3"

# Update grub
sudo update-grub
sudo reboot
```

### 2. Disable CPU Frequency Scaling

```bash
# Set CPU governor to performance mode
for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    echo performance | sudo tee $cpu
done

# Disable turbo boost (for consistency)
echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo
```

### 3. Network Tuning

```bash
# Increase network buffer sizes
sudo sysctl -w net.core.rmem_max=134217728
sudo sysctl -w net.core.wmem_max=134217728
sudo sysctl -w net.core.rmem_default=16777216
sudo sysctl -w net.core.wmem_default=16777216

# Enable busy polling
sudo sysctl -w net.core.busy_poll=50
sudo sysctl -w net.core.busy_read=50

# Increase netdev budget
sudo sysctl -w net.core.netdev_budget=50000
sudo sysctl -w net.core.netdev_budget_usecs=5000

# TCP tuning
sudo sysctl -w net.ipv4.tcp_low_latency=1
sudo sysctl -w net.ipv4.tcp_timestamps=1

# Disable TCP slow start after idle
sudo sysctl -w net.ipv4.tcp_slow_start_after_idle=0
```

### 4. Disable Unnecessary Services

```bash
# Disable unnecessary IRQ balance
sudo systemctl stop irqbalance
sudo systemctl disable irqbalance

# Pin network card IRQs to specific CPUs
# (check your NIC's IRQ with: cat /proc/interrupts)
sudo sh -c "echo 1 > /proc/irq/YOUR_NIC_IRQ/smp_affinity_list"
```

### 5. Huge Pages

```bash
# Enable transparent huge pages
echo always | sudo tee /sys/kernel/mm/transparent_hugepage/enabled

# Or allocate static huge pages
sudo sysctl -w vm.nr_hugepages=1024
```

## Network Interface Configuration

### 1. Increase Ring Buffer Size

```bash
# Check current settings
ethtool -g eth0

# Increase to maximum
sudo ethtool -G eth0 rx 4096 tx 4096
```

### 2. Disable Interrupt Coalescing

```bash
# Disable to reduce latency
sudo ethtool -C eth0 rx-usecs 0 tx-usecs 0
```

### 3. Enable Hardware Timestamping

```bash
# Check if supported
ethtool -T eth0

# Enable RX hardware timestamps
sudo ethtool -K eth0 rx-all on
```

## Building with Maximum Optimization

```bash
mkdir build && cd build

# Use these flags for maximum performance
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_FLAGS="-O3 -march=native -mtune=native -flto" \
      ..

make -j$(nproc)
```

## Running the Application

```bash
# Set CPU affinity
taskset -c 1 ./hft_trading --config ../config/trading.conf

# Or with higher priority
sudo nice -n -20 taskset -c 1 ./hft_trading --config ../config/trading.conf

# Monitor performance
perf stat -e cache-misses,cache-references,instructions,cycles ./hft_trading
```

## Monitoring Tools

### 1. Latency Monitoring

```bash
# Measure context switches
sudo perf record -e sched:sched_switch -a sleep 10
sudo perf report

# Check for cache misses
sudo perf stat -e L1-dcache-load-misses,L1-dcache-loads ./benchmark

# Monitor network latency
sudo tcpdump -i eth0 -tttt -n port 9000
```

### 2. System Latency

```bash
# Install cyclictest for jitter measurement
sudo apt-get install rt-tests

# Run latency test
sudo cyclictest -p 99 -m -c 1 -i 100 -d 0
```

## DPDK Setup (Advanced)

For sub-microsecond latency, consider DPDK:

```bash
# Install DPDK
sudo apt-get install dpdk dpdk-dev

# Bind NIC to DPDK
sudo dpdk-devbind.py --bind=uio_pci_generic eth0

# Allocate huge pages
echo 1024 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
```

## Verification

After tuning, verify with benchmarks:

```bash
./benchmark

# Expected results:
# - RDTSC overhead: < 30 cycles
# - Order book update: < 100ns
# - Memory access: < 10ns (L1 cache)
```

## Common Issues

### High Latency Spikes

1. Check for CPU frequency scaling
2. Verify CPUs are isolated
3. Check for other processes on same CPU
4. Monitor for thermal throttling

### Network Drops

1. Increase ring buffer sizes
2. Check for NIC overruns: `ethtool -S eth0 | grep drop`
3. Verify multicast routing
4. Check firewall rules

### Cache Misses

1. Verify structure alignment
2. Check false sharing with `perf c2c`
3. Review memory access patterns
4. Consider prefetching

## Production Checklist

- [ ] CPU isolation configured
- [ ] Frequency scaling disabled
- [ ] Network tuned (buffers, busy poll)
- [ ] Huge pages enabled
- [ ] IRQ affinity set
- [ ] Application CPU affinity set
- [ ] Benchmarks passing
- [ ] Latency targets met
- [ ] No dropped packets under load
- [ ] Context switches < 10/sec per core
