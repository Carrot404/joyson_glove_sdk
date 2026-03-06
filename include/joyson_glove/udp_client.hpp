/**
 * @file udp_client.hpp
 * @brief UDP network client for glove communication
 * @author Songjie Xiao
 * @copyright Copyright (c) 2026 Joyson Robot. All rights reserved.
 */

#pragma once

#include "joyson_glove/protocol.hpp"
#include <string>
#include <mutex>
#include <atomic>
#include <chrono>

namespace joyson_glove {

/**
 * UDP client configuration
 */
struct UdpConfig {
    std::string server_ip = "192.168.10.123";
    uint16_t server_port = 8080;
    uint16_t local_port = 8080;  // Local port to bind (0 = auto)
    std::chrono::milliseconds send_timeout{100};
    std::chrono::milliseconds receive_timeout{100};
};

/**
 * Network statistics
 */
struct NetworkStatistics {
    uint64_t packets_sent = 0;
    uint64_t packets_received = 0;
    uint64_t send_errors = 0;
    uint64_t receive_errors = 0;
    uint64_t timeouts = 0;
    uint64_t checksum_errors = 0;
};

/**
 * UDP client for communication with glove hardware
 * Thread-safe implementation with timeout handling
 */
class UdpClient {
public:
    explicit UdpClient(const UdpConfig& config = UdpConfig{});
    ~UdpClient();

    // Disable copy, allow move
    UdpClient(const UdpClient&) = delete;
    UdpClient& operator=(const UdpClient&) = delete;
    UdpClient(UdpClient&&) noexcept;
    UdpClient& operator=(UdpClient&&) noexcept;

    /**
     * Connect to server
     * @return true if connection successful
     */
    bool connect();

    /**
     * Disconnect from server
     */
    void disconnect();

    /**
     * Check if connected
     */
    bool is_connected() const { return connected_.load(); }

    /**
     * Send packet (thread-safe)
     * @param packet Packet to send
     * @return true if sent successfully
     */
    bool send_packet(const Packet& packet);

    /**
     * Receive packet (thread-safe, blocking with timeout)
     * @return Received packet or nullopt on timeout/error
     */
    std::optional<Packet> receive_packet();

    /**
     * Send packet and wait for response (thread-safe)
     * @param packet Packet to send
     * @return Response packet or nullopt on timeout/error
     */
    std::optional<Packet> send_and_receive(const Packet& packet);

    /**
     * Send packet asynchronously (fire-and-forget)
     * @param packet Packet to send
     * @return true if sent successfully
     */
    bool send_async(const Packet& packet);

    /**
     * Get network statistics
     */
    NetworkStatistics get_statistics() const;

    /**
     * Reset statistics counters
     */
    void reset_statistics();

    /**
     * Get configuration
     */
    const UdpConfig& get_config() const { return config_; }

private:
    UdpConfig config_;
    int socket_fd_ = -1;
    std::atomic<bool> connected_{false};

    // Thread safety
    mutable std::mutex send_mutex_;
    mutable std::mutex receive_mutex_;
    mutable std::mutex stats_mutex_;

    // Statistics
    NetworkStatistics stats_;

    // Helper functions
    bool set_socket_timeout(int socket_fd, int option, std::chrono::milliseconds timeout);
    void update_send_stats(bool success);
    void update_receive_stats(bool success, bool timeout, bool checksum_error);
};

} // namespace joyson_glove
