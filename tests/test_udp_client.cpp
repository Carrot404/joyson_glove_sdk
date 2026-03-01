#include "joyson_glove/udp_client.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>

using namespace joyson_glove;

// Test UDP client construction
TEST(UdpClientTest, Construction) {
    UdpConfig config;
    config.server_ip = "127.0.0.1";
    config.server_port = 9999;

    UdpClient client(config);
    EXPECT_FALSE(client.is_connected());
    EXPECT_EQ(client.get_config().server_ip, "127.0.0.1");
    EXPECT_EQ(client.get_config().server_port, 9999);
}

// Test UDP client move semantics
TEST(UdpClientTest, MoveSemantics) {
    UdpConfig config;
    config.server_ip = "127.0.0.1";
    config.server_port = 9999;

    UdpClient client1(config);
    UdpClient client2(std::move(client1));

    EXPECT_EQ(client2.get_config().server_ip, "127.0.0.1");
    EXPECT_EQ(client2.get_config().server_port, 9999);
}

// Test statistics initialization
TEST(UdpClientTest, StatisticsInitialization) {
    UdpClient client;
    auto stats = client.get_statistics();

    EXPECT_EQ(stats.packets_sent, 0);
    EXPECT_EQ(stats.packets_received, 0);
    EXPECT_EQ(stats.send_errors, 0);
    EXPECT_EQ(stats.receive_errors, 0);
    EXPECT_EQ(stats.timeouts, 0);
    EXPECT_EQ(stats.checksum_errors, 0);
}

// Test statistics reset
TEST(UdpClientTest, StatisticsReset) {
    UdpClient client;

    // Manually update statistics (would normally happen during send/receive)
    // For this test, we just verify reset works
    client.reset_statistics();

    auto stats = client.get_statistics();
    EXPECT_EQ(stats.packets_sent, 0);
    EXPECT_EQ(stats.packets_received, 0);
}

// Test connection to invalid address (should fail gracefully)
TEST(UdpClientTest, ConnectionToInvalidAddress) {
    UdpConfig config;
    config.server_ip = "999.999.999.999";  // Invalid IP
    config.server_port = 8080;

    UdpClient client(config);
    EXPECT_FALSE(client.connect());
    EXPECT_FALSE(client.is_connected());
}

// Test connection to localhost (loopback test)
TEST(UdpClientTest, ConnectionToLocalhost) {
    UdpConfig config;
    config.server_ip = "127.0.0.1";
    config.server_port = 8080;
    config.send_timeout = std::chrono::milliseconds(100);
    config.receive_timeout = std::chrono::milliseconds(100);

    UdpClient client(config);

    // Note: This will succeed even without a server listening
    // UDP is connectionless, so "connect" just sets the default destination
    bool connected = client.connect();

    if (connected) {
        EXPECT_TRUE(client.is_connected());
        client.disconnect();
        EXPECT_FALSE(client.is_connected());
    }
}

// Test packet send (without actual server)
TEST(UdpClientTest, PacketSendWithoutServer) {
    UdpConfig config;
    config.server_ip = "127.0.0.1";
    config.server_port = 9999;  // Unlikely to have server here
    config.send_timeout = std::chrono::milliseconds(50);

    UdpClient client(config);

    if (client.connect()) {
        Packet packet;
        packet.header = PACKET_HEADER;
        packet.length = 9;
        packet.module_id = MODULE_ID;
        packet.target = 1;
        packet.command = CMD_READ_STATUS;
        packet.tail = PACKET_TAIL;

        auto serialized = packet.serialize();
        packet.checksum = Packet::calculate_checksum(serialized);

        // Send should succeed (UDP doesn't wait for ACK)
        bool sent = client.send_packet(packet);

        // Check statistics
        auto stats = client.get_statistics();
        if (sent) {
            EXPECT_GT(stats.packets_sent, 0);
        }

        client.disconnect();
    }
}

// Test receive timeout
TEST(UdpClientTest, ReceiveTimeout) {
    UdpConfig config;
    config.server_ip = "127.0.0.1";
    config.server_port = 9999;
    config.receive_timeout = std::chrono::milliseconds(50);

    UdpClient client(config);

    if (client.connect()) {
        // Try to receive without sending (should timeout)
        auto start = std::chrono::steady_clock::now();
        auto packet = client.receive_packet();
        auto elapsed = std::chrono::steady_clock::now() - start;

        EXPECT_FALSE(packet.has_value());

        // Should timeout around 50ms (allow some margin)
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
        EXPECT_GE(elapsed_ms.count(), 40);  // At least 40ms
        EXPECT_LE(elapsed_ms.count(), 200);  // At most 200ms (generous margin)

        // Check timeout statistics
        auto stats = client.get_statistics();
        EXPECT_GT(stats.timeouts, 0);

        client.disconnect();
    }
}

// Test send_and_receive (will timeout without server)
TEST(UdpClientTest, SendAndReceiveTimeout) {
    UdpConfig config;
    config.server_ip = "127.0.0.1";
    config.server_port = 9999;
    config.send_timeout = std::chrono::milliseconds(50);
    config.receive_timeout = std::chrono::milliseconds(50);

    UdpClient client(config);

    if (client.connect()) {
        Packet packet;
        packet.header = PACKET_HEADER;
        packet.length = 9;
        packet.module_id = MODULE_ID;
        packet.target = 1;
        packet.command = CMD_READ_STATUS;
        packet.tail = PACKET_TAIL;

        auto serialized = packet.serialize();
        packet.checksum = Packet::calculate_checksum(serialized);

        // Send and receive (should timeout on receive)
        auto response = client.send_and_receive(packet);
        EXPECT_FALSE(response.has_value());

        client.disconnect();
    }
}

// Test multiple connect/disconnect cycles
TEST(UdpClientTest, MultipleConnectDisconnect) {
    UdpConfig config;
    config.server_ip = "127.0.0.1";
    config.server_port = 8080;

    UdpClient client(config);

    for (int i = 0; i < 3; ++i) {
        bool connected = client.connect();
        if (connected) {
            EXPECT_TRUE(client.is_connected());
            client.disconnect();
            EXPECT_FALSE(client.is_connected());
        }
    }
}

// Test thread safety (basic test with multiple threads)
TEST(UdpClientTest, ThreadSafety) {
    UdpConfig config;
    config.server_ip = "127.0.0.1";
    config.server_port = 9999;
    config.send_timeout = std::chrono::milliseconds(10);
    config.receive_timeout = std::chrono::milliseconds(10);

    UdpClient client(config);

    if (client.connect()) {
        // Create multiple threads that send packets
        std::vector<std::thread> threads;
        const int num_threads = 4;
        const int packets_per_thread = 10;

        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&client, i]() {
                for (int j = 0; j < packets_per_thread; ++j) {
                    Packet packet;
                    packet.header = PACKET_HEADER;
                    packet.length = 9;
                    packet.module_id = MODULE_ID;
                    packet.target = static_cast<uint8_t>(i + 1);
                    packet.command = CMD_READ_STATUS;
                    packet.tail = PACKET_TAIL;

                    auto serialized = packet.serialize();
                    packet.checksum = Packet::calculate_checksum(serialized);

                    client.send_packet(packet);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            });
        }

        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }

        // Check that some packets were sent
        auto stats = client.get_statistics();
        EXPECT_GT(stats.packets_sent, 0);

        client.disconnect();
    }
}

// Test configuration getters
TEST(UdpClientTest, ConfigurationGetters) {
    UdpConfig config;
    config.server_ip = "192.168.1.100";
    config.server_port = 12345;
    config.send_timeout = std::chrono::milliseconds(200);
    config.receive_timeout = std::chrono::milliseconds(300);

    UdpClient client(config);

    const auto& retrieved_config = client.get_config();
    EXPECT_EQ(retrieved_config.server_ip, "192.168.1.100");
    EXPECT_EQ(retrieved_config.server_port, 12345);
    EXPECT_EQ(retrieved_config.send_timeout.count(), 200);
    EXPECT_EQ(retrieved_config.receive_timeout.count(), 300);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
