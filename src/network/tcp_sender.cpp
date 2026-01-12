#include "network/tcp_sender.h"
#include "common/logger.h"
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <fcntl.h>

namespace hft {

TCPSender::TCPSender(const std::string& host, uint16_t port)
    : host_(host)
    , port_(port) {
}

TCPSender::~TCPSender() {
    disconnect();
}

bool TCPSender::connect() {
    if (connected_.load(std::memory_order_acquire)) {
        return true;
    }
    
    // Create TCP socket
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        LOG_ERROR("Failed to create TCP socket");
        return false;
    }
    
    // Optimize socket BEFORE connecting
    optimize_socket();
    
    // Connect to server
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    
    if (inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) <= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
        LOG_ERROR("Invalid address");
        return false;
    }
    
    if (::connect(socket_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(socket_fd_);
        socket_fd_ = -1;
        LOG_ERROR("Connection failed");
        return false;
    }
    
    connected_.store(true, std::memory_order_release);
    LOG_INFO("Connected to order gateway");
    return true;
}

void TCPSender::disconnect() {
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
    connected_.store(false, std::memory_order_release);
}

void TCPSender::enable_tcp_optimizations() {
    // This will be called before connect
}

void TCPSender::set_cpu_affinity(int cpu) {
    cpu_affinity_ = cpu;
}

void TCPSender::optimize_socket() {
    // TCP_NODELAY: Disable Nagle's algorithm
    // Critical for HFT - we want to send packets immediately
    int flag = 1;
    setsockopt(socket_fd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    
    // TCP_QUICKACK: Enable quick ACKs
    // Reduces round-trip latency
#ifdef __linux__
    flag = 1;
    setsockopt(socket_fd_, IPPROTO_TCP, TCP_QUICKACK, &flag, sizeof(flag));
#endif
    
    // Set send buffer size
    int send_buffer_size = 256 * 1024;
    setsockopt(socket_fd_, SOL_SOCKET, SO_SNDBUF, 
               &send_buffer_size, sizeof(send_buffer_size));
    
    // SO_PRIORITY: Set socket priority (for QoS)
#ifdef __linux__
    int priority = 6; // High priority
    setsockopt(socket_fd_, SOL_SOCKET, SO_PRIORITY, 
               &priority, sizeof(priority));
#endif
    
    // TCP_USER_TIMEOUT: Set timeout for unacknowledged data
#ifdef __linux__
    unsigned int timeout = 5000; // 5 seconds
    setsockopt(socket_fd_, IPPROTO_TCP, TCP_USER_TIMEOUT,
               &timeout, sizeof(timeout));
#endif
    
    // For ultimate performance, you could use:
    // - TCP_CORK/TCP_UNCORK for batching
    // - SO_BUSY_POLL for polling instead of interrupts
    // - DPDK or kernel bypass (XDP, AF_XDP) for sub-microsecond latency
}

bool TCPSender::send_order(const Order& order) {
    if (!connected_.load(std::memory_order_acquire)) {
        LOG_ERROR("Not connected to order gateway");
        return false;
    }
    
    // Send the order (blocking send)
    // In production, might use non-blocking I/O with epoll
    ssize_t sent = send(socket_fd_, &order, sizeof(order), 0);
    
    if (sent != sizeof(order)) {
        LOG_ERROR("Failed to send order");
        return false;
    }
    
    return true;
}

} // namespace hft
