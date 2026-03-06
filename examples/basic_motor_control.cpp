/**
 * @file basic_motor_control.cpp
 * @brief Example: basic motor control usage
 * @author Songjie Xiao
 * @copyright Copyright (c) 2026 Joyson Robot. All rights reserved.
 */

#include "joyson_glove/glove_sdk.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace joyson_glove;

// Helper function to print network statistics
void print_network_stats(const std::string& label, const NetworkStatistics& stats) {
    std::cout << label << std::endl;
    std::cout << "  Packets sent: " << stats.packets_sent << std::endl;
    std::cout << "  Packets received: " << stats.packets_received << std::endl;
    std::cout << "  Send errors: " << stats.send_errors << std::endl;
    std::cout << "  Receive errors: " << stats.receive_errors << std::endl;
    std::cout << "  Timeouts: " << stats.timeouts << std::endl;
    std::cout << "  Checksum errors: " << stats.checksum_errors << std::endl;
}

int main() {
    std::cout << "=== Joyson Glove SDK - Basic Motor Control Example ===" << std::endl;

    // Create SDK instance with default configuration
    GloveConfig config;
    config.send_timeout = std::chrono::milliseconds(500);
    config.receive_timeout = std::chrono::milliseconds(500);
    config.auto_start_thread = false;  // Manual control for this example
    GloveSDK sdk(config);

    // Initialize SDK
    std::cout << "\n[1] Initializing SDK..." << std::endl;
    if (!sdk.initialize()) {
        std::cerr << "Failed to initialize SDK" << std::endl;
        return 1;
    }

    // Set all motors to position mode (already done in initialize, but shown for clarity)
    std::cout << "\n[2] Setting motors to position mode..." << std::endl;
    if (!sdk.motor_controller().set_all_modes(MODE_POSITION)) {
        std::cerr << "Failed to set motor modes" << std::endl;
        return 1;
    }

    // Send position commands to all motors
    std::cout << "\n[3] Sending position commands..." << std::endl;
    std::array<uint16_t, NUM_MOTORS> target_positions = {500, 600, 700, 800, 900, 1000};

    for (size_t i = 0; i < NUM_MOTORS; ++i) {
        std::cout << "  Motor " << (i + 1) << ": target position = " << target_positions[i] << std::endl;
    }

    if (!sdk.set_all_positions(target_positions)) {
        std::cerr << "Failed to set motor positions" << std::endl;
        return 1;
    }

    // Wait for motors to move
    std::cout << "\n[4] Waiting for motors to reach target positions..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Read motor status
    std::cout << "\n[5] Reading motor status..." << std::endl;
    for (uint8_t motor_id = 1; motor_id <= NUM_MOTORS; ++motor_id) {
        auto status = sdk.motor_controller().get_motor_status(motor_id);
        if (status) {
            std::cout << "  Motor " << static_cast<int>(motor_id) << ":" << std::endl;
            std::cout << "    Position: " << status->actual_position << " (target: " << target_positions[motor_id - 1] << ")" << std::endl;
            std::cout << "    Current: " << status->actual_current << " mA" << std::endl;
            std::cout << "    Force: " << status->force_value << std::endl;
            std::cout << "    Temperature: " << static_cast<int>(status->temperature) << " °C" << std::endl;
        } else {
            std::cerr << "  Failed to read status for motor " << static_cast<int>(motor_id) << std::endl;
        }
    }

    // Test single motor control
    std::cout << "\n[6] Testing single motor control (Motor 1)..." << std::endl;
    uint8_t test_motor = 1;
    uint16_t test_position = 1500;

    std::cout << "  Setting motor " << static_cast<int>(test_motor) << " to position " << test_position << std::endl;
    if (sdk.set_motor_position(test_motor, test_position)) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        auto status = sdk.motor_controller().get_motor_status(test_motor);
        if (status) {
            std::cout << "  Current position: " << status->actual_position << std::endl;
        }
    }

    // Display network statistics
    std::cout << "\n[7] Network statistics:" << std::endl;
    print_network_stats("  Final statistics:", sdk.get_network_statistics());

    // Shutdown SDK
    std::cout << "\n[8] Shutting down SDK..." << std::endl;
    sdk.shutdown();

    std::cout << "\n=== Example completed successfully ===" << std::endl;
    return 0;
}
