#include "network/udp_receiver.h"
#include "common/logger.h"
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#ifdef __linux__
#include <sched.h>
#endif

namespace hft {

UDPReceiver::UDPReceiver(MarketDataHandler& handler,
                         const std::string& multicast_ip,
                         uint16_t port)
    : handler_(handler)
    , multicast_ip_(multicast_ip)
    , port_(port) {
}

UDPReceiver::~UDPReceiver() {
    stop();
}

void UDPReceiver::start() {
    if (running_.load(std::memory_order_acquire)) {
        return;
    }
    
    if (!setup_socket()) {
        LOG_ERROR("Failed to setup UDP socket");
        return;
    }
    
    running_.store(true, std::memory_order_release);
    receiver_thread_ = std::thread(&UDPReceiver::receive_loop, this);
}

void UDPReceiver::stop() {
    running_.store(false, std::memory_order_release);
    
    if (receiver_thread_.joinable()) {
        receiver_thread_.join();
    }
    
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
}

void UDPReceiver::set_cpu_affinity(int cpu) {
    cpu_affinity_ = cpu;
}

void UDPReceiver::enable_kernel_bypass() {
    kernel_bypass_enabled_ = true;
}

bool UDPReceiver::setup_socket() {
    // Create UDP socket
    socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd_ < 0) {
        return false;
    }
    
    // Set socket options for low latency
    optimize_socket();
    
    // Bind to port
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);
    
    if (bind(socket_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    // Join multicast group
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(multicast_ip_.c_str());
    mreq.imr_interface.s_addr = INADDR_ANY;
    
    if (setsockopt(socket_fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, 
                   &mreq, sizeof(mreq)) < 0) {
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    LOG_INFO("UDP receiver setup complete");
    return true;
}

void UDPReceiver::optimize_socket() {
    // Increase receive buffer size
    int recv_buffer_size = 2 * 1024 * 1024; // 2MB
    setsockopt(socket_fd_, SOL_SOCKET, SO_RCVBUF, 
               &recv_buffer_size, sizeof(recv_buffer_size));
    
#ifdef __linux__
    // SO_BUSY_POLL: Use busy polling instead of interrupts
    // This reduces latency significantly (microseconds)
    if (kernel_bypass_enabled_) {
        int busy_poll = 50; // microseconds
        setsockopt(socket_fd_, SOL_SOCKET, SO_BUSY_POLL, 
                   &busy_poll, sizeof(busy_poll));
        
        // SO_INCOMING_CPU: Pin socket to specific CPU
        if (cpu_affinity_ >= 0) {
            setsockopt(socket_fd_, SOL_SOCKET, SO_INCOMING_CPU,
                      &cpu_affinity_, sizeof(cpu_affinity_));
        }
    }
#endif
    
    // Set socket to non-blocking for better control
    // (though we'll use blocking recv for simplicity in this demo)
}

void UDPReceiver::receive_loop() {
    // Set CPU affinity for this thread
#ifdef __linux__
    if (cpu_affinity_ >= 0) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_affinity_, &cpuset);
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
        LOG_INFO("Market data thread pinned to CPU");
    }
#endif
    
    // Pre-allocate buffer (avoid allocations in hot path)
    constexpr size_t BUFFER_SIZE = 65536;
    alignas(64) char buffer[BUFFER_SIZE];
    
    LOG_INFO("UDP receiver started");
    
    while (running_.load(std::memory_order_acquire)) {
        // Receive data (blocking)
        ssize_t bytes_received = recv(socket_fd_, buffer, BUFFER_SIZE, 0);
        
        if (bytes_received > 0) {
            // Process message immediately (zero-copy)
            // This is the critical path - must be fast!
            handler_.process_message(buffer, bytes_received);
        } else if (bytes_received < 0) {
            // Error handling
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                LOG_ERROR("UDP receive error");
                break;
            }
        }
    }
    
    LOG_INFO("UDP receiver stopped");
}

} // namespace hft
