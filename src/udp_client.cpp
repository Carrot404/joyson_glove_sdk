#include "joyson_glove/udp_client.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

namespace joyson_glove {

UdpClient::UdpClient(const UdpConfig& config)
    : config_(config) {
}

UdpClient::~UdpClient() {
    disconnect();
}

UdpClient::UdpClient(UdpClient&& other) noexcept
    : config_(std::move(other.config_)),
      socket_fd_(other.socket_fd_),
      connected_(other.connected_.load()),
      stats_(other.stats_) {
    other.socket_fd_ = -1;
    other.connected_ = false;
}

UdpClient& UdpClient::operator=(UdpClient&& other) noexcept {
    if (this != &other) {
        disconnect();
        config_ = std::move(other.config_);
        socket_fd_ = other.socket_fd_;
        connected_ = other.connected_.load();
        stats_ = other.stats_;
        other.socket_fd_ = -1;
        other.connected_ = false;
    }
    return *this;
}

bool UdpClient::connect() {
    std::lock_guard<std::mutex> send_lock(send_mutex_);
    std::lock_guard<std::mutex> recv_lock(receive_mutex_);

    if (connected_.load()) {
        return true;
    }

    // Create UDP socket
    socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd_ < 0) {
        std::cerr << "[UdpClient] Failed to create socket: " << strerror(errno) << std::endl;
        return false;
    }

    // Set socket timeouts
    if (!set_socket_timeout(socket_fd_, SO_RCVTIMEO, config_.receive_timeout)) {
        std::cerr << "[UdpClient] Failed to set receive timeout" << std::endl;
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    if (!set_socket_timeout(socket_fd_, SO_SNDTIMEO, config_.send_timeout)) {
        std::cerr << "[UdpClient] Failed to set send timeout" << std::endl;
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    // Connect to server (for UDP, this just sets the default destination)
    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config_.server_port);

    if (inet_pton(AF_INET, config_.server_ip.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "[UdpClient] Invalid server IP address: " << config_.server_ip << std::endl;
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    if (::connect(socket_fd_, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        std::cerr << "[UdpClient] Failed to connect to " << config_.server_ip << ":"
                  << config_.server_port << " - " << strerror(errno) << std::endl;
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    connected_ = true;
    std::cout << "[UdpClient] Connected to " << config_.server_ip << ":" << config_.server_port << std::endl;
    return true;
}

void UdpClient::disconnect() {
    std::lock_guard<std::mutex> send_lock(send_mutex_);
    std::lock_guard<std::mutex> recv_lock(receive_mutex_);

    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
    connected_ = false;
}

bool UdpClient::send_packet(const Packet& packet) {
    std::lock_guard<std::mutex> lock(send_mutex_);

    if (!connected_.load()) {
        std::cerr << "[UdpClient] Not connected" << std::endl;
        update_send_stats(false);
        return false;
    }

    auto data = packet.serialize();
    ssize_t sent = send(socket_fd_, data.data(), data.size(), 0);

    if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            std::cerr << "[UdpClient] Send timeout" << std::endl;
        } else {
            std::cerr << "[UdpClient] Send error: " << strerror(errno) << std::endl;
        }
        update_send_stats(false);
        return false;
    }

    if (static_cast<size_t>(sent) != data.size()) {
        std::cerr << "[UdpClient] Partial send: " << sent << "/" << data.size() << " bytes" << std::endl;
        update_send_stats(false);
        return false;
    }

    update_send_stats(true);
    return true;
}

std::optional<Packet> UdpClient::receive_packet() {
    std::lock_guard<std::mutex> lock(receive_mutex_);

    if (!connected_.load()) {
        std::cerr << "[UdpClient] Not connected" << std::endl;
        update_receive_stats(false, false, false);
        return std::nullopt;
    }

    std::vector<uint8_t> buffer(1024);  // Max UDP packet size for this protocol
    ssize_t received = recv(socket_fd_, buffer.data(), buffer.size(), 0);

    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            update_receive_stats(false, true, false);
            return std::nullopt;
        } else {
            std::cerr << "[UdpClient] Receive error: " << strerror(errno) << std::endl;
            update_receive_stats(false, false, false);
            return std::nullopt;
        }
    }

    buffer.resize(received);
    auto packet = Packet::deserialize(buffer);

    if (!packet) {
        std::cerr << "[UdpClient] Failed to deserialize packet" << std::endl;
        update_receive_stats(false, false, true);
        return std::nullopt;
    }

    update_receive_stats(true, false, false);
    return packet;
}

std::optional<Packet> UdpClient::send_and_receive(const Packet& packet) {
    if (!send_packet(packet)) {
        return std::nullopt;
    }
    return receive_packet();
}

bool UdpClient::send_async(const Packet& packet) {
    return send_packet(packet);
}

NetworkStatistics UdpClient::get_statistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void UdpClient::reset_statistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = NetworkStatistics{};
}

bool UdpClient::set_socket_timeout(int socket_fd, int option, std::chrono::milliseconds timeout) {
    struct timeval tv;
    tv.tv_sec = timeout.count() / 1000;
    tv.tv_usec = (timeout.count() % 1000) * 1000;

    if (setsockopt(socket_fd, SOL_SOCKET, option, &tv, sizeof(tv)) < 0) {
        std::cerr << "[UdpClient] Failed to set socket timeout: " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

void UdpClient::update_send_stats(bool success) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    if (success) {
        stats_.packets_sent++;
    } else {
        stats_.send_errors++;
    }
}

void UdpClient::update_receive_stats(bool success, bool timeout, bool checksum_error) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    if (success) {
        stats_.packets_received++;
    } else if (timeout) {
        stats_.timeouts++;
    } else if (checksum_error) {
        stats_.checksum_errors++;
    } else {
        stats_.receive_errors++;
    }
}

} // namespace joyson_glove
