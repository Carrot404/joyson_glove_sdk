/**
 * @file test_motor_controller.cpp
 * @brief Example: standalone motor controller testing
 * @author Songjie Xiao
 * @copyright Copyright (c) 2026 Joyson Robot. All rights reserved.
 */

#include "joyson_glove/motor_controller.hpp"
#include "joyson_glove/udp_client.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <csignal>

using namespace joyson_glove;

static volatile std::sig_atomic_t g_running = 1;

static void signal_handler(int /*signum*/) {
    g_running = 0;
}

static void print_motor_status(const MotorStatus& status, const std::string& label) {
    std::cout << "[" << label << "] "
              << "ID=" << static_cast<int>(status.motor_id) << "  "
              << "Target=" << std::setw(4) << status.target_position << "  "
              << "Actual=" << std::setw(4) << status.actual_position << "  "
              << "Force=" << std::setw(4) << status.force_value << "  "
              << "Current=" << std::setw(4) << status.actual_current << std::endl;
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);

    std::cout << "=== MotorController Standalone Test ===" << std::endl;

    // ── Step 1: Configure and connect UDP client ──
    UdpConfig udp_config;
    udp_config.server_ip       = (argc > 1) ? argv[1] : "192.168.10.123";
    udp_config.server_port     = (argc > 2) ? static_cast<uint16_t>(std::stoi(argv[2])) : 8080;
    udp_config.local_port      = udp_config.server_port;
    udp_config.send_timeout    = std::chrono::milliseconds(500);
    udp_config.receive_timeout = std::chrono::milliseconds(500);

    std::cout << "\n[1] Connecting to " << udp_config.server_ip
              << ":" << udp_config.server_port << " ..." << std::endl;

    auto udp_client = std::make_shared<UdpClient>(udp_config);
    if (!udp_client->connect()) {
        std::cerr << "Failed to connect UDP client" << std::endl;
        return 1;
    }
    std::cout << "    UDP client connected" << std::endl;

    // ── Step 2: Initialize MotorController (without auto-start thread) ──
    MotorControllerConfig motor_config;
    motor_config.auto_start_thread = false;
    motor_config.update_interval   = std::chrono::milliseconds(100);  // 10Hz

    MotorController motor(udp_client, motor_config);

    std::cout << "\n[2] Initializing MotorController..." << std::endl;
    if (!motor.initialize()) {
        std::cerr << "Failed to initialize MotorController (hardware not responding?)" << std::endl;
        udp_client->disconnect();
        return 1;
    }

    // ── Step 3: Direct blocking read (thread not running) ──
    std::cout << "\n[3] Direct blocking read (motor 1-3 status):" << std::endl;
    for (uint8_t motor_id = 1; motor_id <= 3; ++motor_id) {
        auto status = motor.get_motor_status(motor_id);
        if (status) {
            print_motor_status(*status, "motor " + std::to_string(motor_id));
        } else {
            std::cerr << "    Read failed for motor " << static_cast<int>(motor_id) << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // ── Step 4: Test force mode control ──
    std::cout << "\n[4] Testing force mode control (motor 1)..." << std::endl;

    if (!motor.set_motor_mode(1, MODE_FORCE)) {
        std::cerr << "    Failed to set motor 1 to force mode" << std::endl;
    }

    std::cout << "    Setting force values: 10 -> 50 -> 100 -> 10" << std::endl;
    const std::array<uint16_t, 4> test_forces = {10, 50, 100, 10};

    for (auto force : test_forces) {
        if (motor.set_motor_force(1, force)) {
            std::cout << "    Force set to " << force << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            auto status = motor.get_motor_status(1);
            if (status) {
                print_motor_status(*status, "verify");
            }
        } else {
            std::cerr << "    Failed to set force " << force << std::endl;
        }
    }

    // ── Step 5: Test batch force control ──
    std::cout << "\n[5] Testing batch force control (all motors)..." << std::endl;
    std::array<uint16_t, NUM_MOTORS> forces;
    forces.fill(20);

    if (motor.set_all_forces(forces)) {
        std::cout << "    All motors set to force 20" << std::endl;
    } else {
        std::cerr << "    Batch force control failed" << std::endl;
    }

    // ── Step 6: Start background thread and read cached data ──
    std::cout << "\n[6] Starting background update thread (10Hz)..." << std::endl;
    motor.start_status_update_thread();
    std::cout << "    Thread running: " << std::boolalpha << motor.is_thread_running() << std::endl;

    std::cout << "\n    Reading cached data for 5 seconds (Ctrl+C to stop early)...\n" << std::endl;
    const auto start = std::chrono::steady_clock::now();
    const auto duration = std::chrono::seconds(5);

    while (g_running) {
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed >= duration) break;

        // Display status for first 3 motors
        std::cout << "\r    ";
        for (uint8_t motor_id = 1; motor_id <= 3; ++motor_id) {
            auto status = motor.get_cached_status(motor_id);
            auto age = motor.get_status_age(motor_id);

            if (status) {
                std::cout << "M" << static_cast<int>(motor_id) << ":"
                          << "P=" << std::setw(4) << status->actual_position << " "
                          << "F=" << std::setw(4) << status->force_value << " "
                          << "(" << std::setw(3) << age.count() << "ms)  ";
            }
        }
        std::cout << std::flush;

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    std::cout << std::endl;

    // ── Step 7: Stop and report ──
    std::cout << "\n[7] Stopping update thread..." << std::endl;
    motor.stop_status_update_thread();
    std::cout << "    Thread running: " << std::boolalpha << motor.is_thread_running() << std::endl;

    // Print final status for all motors
    std::cout << "\n[8] Final status for all motors:" << std::endl;
    auto all_status = motor.get_all_cached_status();
    for (size_t i = 0; i < NUM_MOTORS; ++i) {
        print_motor_status(all_status[i], "motor " + std::to_string(i + 1));
    }

    // Network stats
    auto stats = udp_client->get_statistics();
    std::cout << "\n[9] Network statistics:" << std::endl;
    std::cout << "    Sent: " << stats.packets_sent
              << "  Received: " << stats.packets_received
              << "  Errors: " << (stats.send_errors + stats.receive_errors)
              << "  Timeouts: " << stats.timeouts << std::endl;

    udp_client->disconnect();
    std::cout << "\n=== Test completed ===" << std::endl;
    return 0;
}
