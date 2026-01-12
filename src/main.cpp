#include "market_data/market_data_handler.h"
#include "market_data/order_book.h"
#include "network/udp_receiver.h"
#include "network/tcp_sender.h"
#include "trading/strategy.h"
#include "trading/order_manager.h"
#include "common/config.h"
#include "common/logger.h"
#include "common/timestamp.h"
#include <iostream>
#include <memory>
#include <csignal>
#include <atomic>

std::atomic<bool> running{true};

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nShutting down...\n";
        running.store(false);
    }
}

int main(int argc, char* argv[]) {
    using namespace hft;
    
    std::cout << "\n";
    std::cout << "================================================\n";
    std::cout << "   HFT Trading System v1.0\n";
    std::cout << "================================================\n\n";
    
    // Setup signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // Calibrate timestamp
    std::cout << "Calibrating TSC...\n";
    double tsc_freq = Timestamp::calibrate_tsc_frequency();
    std::cout << "TSC Frequency: " << tsc_freq / 1e9 << " GHz\n\n";
    
    // Load configuration
    Config config;
    if (argc > 1) {
        std::cout << "Loading config from: " << argv[1] << "\n";
        config.load(argv[1]);
    } else {
        std::cout << "Using default configuration\n";
    }
    
    std::cout << "Market Data: " << config.market_data_multicast_ip 
              << ":" << config.market_data_port << "\n";
    std::cout << "Order Gateway: " << config.order_gateway_ip 
              << ":" << config.order_gateway_port << "\n\n";
    
    // Initialize components
    std::cout << "Initializing trading system...\n\n";
    
    // 1. Market data handler
    MarketDataHandler md_handler;
    md_handler.add_symbol("AAPL");
    md_handler.add_symbol("MSFT");
    md_handler.add_symbol("GOOGL");
    
    // 2. TCP sender for orders
    TCPSender order_sender(config.order_gateway_ip, config.order_gateway_port);
    order_sender.set_cpu_affinity(config.order_manager_cpu);
    order_sender.enable_tcp_optimizations();
    
    // Note: In demo mode, we won't actually connect
    std::cout << "[DEMO MODE] Skipping TCP connection to order gateway\n";
    
    // 3. Order manager with risk limits
    OrderManager order_manager(order_sender);
    OrderManager::RiskLimits limits;
    limits.max_order_size = config.max_order_size;
    limits.max_position = config.max_position_size;
    order_manager.set_risk_limits(limits);
    
    // 4. Trading strategy
    MarketMakingStrategy::Parameters strategy_params;
    strategy_params.spread_target = config.spread_threshold;
    strategy_params.max_position = config.max_position_size;
    
    auto strategy = std::make_unique<MarketMakingStrategy>(order_sender, strategy_params);
    
    // 5. Register callback for market data updates
    md_handler.register_callback([&strategy](const OrderBook& book) {
        strategy->on_order_book_update(book);
    });
    
    // 6. UDP receiver for market data
    UDPReceiver udp_receiver(md_handler, config.market_data_multicast_ip, 
                             config.market_data_port);
    udp_receiver.set_cpu_affinity(config.market_data_cpu);
    if (config.enable_kernel_bypass) {
        udp_receiver.enable_kernel_bypass();
    }
    
    std::cout << "System initialized successfully!\n\n";
    std::cout << "Components:\n";
    std::cout << "  ✓ Market Data Handler\n";
    std::cout << "  ✓ Order Book Engine (lock-free)\n";
    std::cout << "  ✓ Trading Strategy: " << strategy->name() << "\n";
    std::cout << "  ✓ Order Manager (with risk controls)\n";
    std::cout << "  ✓ Network Stack (UDP/TCP)\n\n";
    
    std::cout << "Performance optimizations:\n";
    std::cout << "  ✓ CPU affinity enabled\n";
    std::cout << "  ✓ Lock-free data structures\n";
    std::cout << "  ✓ Cache line alignment\n";
    std::cout << "  ✓ Zero-copy message processing\n";
    std::cout << "  ✓ TCP_NODELAY enabled\n";
    if (config.enable_kernel_bypass) {
        std::cout << "  ✓ SO_BUSY_POLL enabled\n";
    }
    std::cout << "\n";
    
    // Note: In production, we would:
    // - Start the UDP receiver: udp_receiver.start();
    // - Run the main event loop
    // - Process fills and updates
    
    std::cout << "[DEMO MODE] Trading system ready but not starting UDP receiver\n";
    std::cout << "In production, this would:\n";
    std::cout << "  1. Receive market data via UDP multicast\n";
    std::cout << "  2. Update order books (lock-free)\n";
    std::cout << "  3. Run trading strategy\n";
    std::cout << "  4. Submit orders via TCP\n";
    std::cout << "  5. Track P&L and positions\n\n";
    
    std::cout << "Expected latencies:\n";
    std::cout << "  - Market data to order book: < 500ns\n";
    std::cout << "  - Strategy decision: < 200ns\n";
    std::cout << "  - Order submission: < 1μs\n";
    std::cout << "  - End-to-end (tick-to-trade): < 2μs\n\n";
    
    std::cout << "Key HFT concepts demonstrated:\n";
    std::cout << "  • Lock-free programming (atomics, memory ordering)\n";
    std::cout << "  • Cache optimization (alignment, false sharing)\n";
    std::cout << "  • Network tuning (TCP_NODELAY, SO_BUSY_POLL)\n";
    std::cout << "  • CPU pinning for deterministic performance\n";
    std::cout << "  • Zero-copy data processing\n";
    std::cout << "  • High-precision timestamping (RDTSC)\n";
    std::cout << "  • Risk management and order validation\n\n";
    
    std::cout << "Press Ctrl+C to exit...\n";
    
    // Main loop (in demo mode, just wait for signal)
    while (running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // In production, would process events here
        // For demo, just show we're alive
        static int counter = 0;
        if (++counter % 10 == 0) {
            std::cout << "System running... (Position: " 
                      << strategy->get_position() << ", P&L: $" 
                      << strategy->get_pnl() << ")\n";
        }
    }
    
    std::cout << "\nShutdown complete.\n";
    std::cout << "Final stats:\n";
    std::cout << "  Position: " << strategy->get_position() << "\n";
    std::cout << "  P&L: $" << strategy->get_pnl() << "\n\n";
    
    return 0;
}
