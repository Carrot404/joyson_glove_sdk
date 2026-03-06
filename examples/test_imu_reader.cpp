/**
 * @file test_imu_reader.cpp
 * @brief Example: standalone IMU reader testing
 * @author Songjie Xiao
 * @copyright Copyright (c) 2026 Joyson Robot. All rights reserved.
 */

#include "joyson_glove/imu_reader.hpp"
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

static void print_imu_data(const ImuData& data, const std::string& label) {
    std::cout << "[" << label << "] "
              << "Roll=" << std::fixed << std::setprecision(2) << data.roll << "°  "
              << "Pitch=" << data.pitch << "°  "
              << "Yaw=" << data.yaw << "°" << std::endl;
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);

    std::cout << "=== ImuReader Standalone Test ===" << std::endl;

    // ── Step 1: Configure and connect UDP client ──
    UdpConfig udp_config;
    udp_config.server_ip       = (argc > 1) ? argv[1] : "192.168.10.123";
    udp_config.server_port     = (argc > 2) ? static_cast<uint16_t>(std::stoi(argv[2])) : 8080;
    udp_config.local_port      = udp_config.server_port;
    udp_config.send_timeout    = std::chrono::milliseconds(150);
    udp_config.receive_timeout = std::chrono::milliseconds(150);

    std::cout << "\n[1] Connecting to " << udp_config.server_ip
              << ":" << udp_config.server_port << " ..." << std::endl;

    auto udp_client = std::make_shared<UdpClient>(udp_config);
    if (!udp_client->connect()) {
        std::cerr << "Failed to connect UDP client" << std::endl;
        return 1;
    }
    std::cout << "    UDP client connected" << std::endl;

    // ── Step 2: Initialize ImuReader (without auto-start thread) ──
    ImuReaderConfig imu_config;
    imu_config.auto_start_thread = false;
    imu_config.update_interval   = std::chrono::milliseconds(100);  // 10Hz

    ImuReader imu(udp_client, imu_config);

    std::cout << "\n[2] Initializing ImuReader..." << std::endl;
    if (!imu.initialize()) {
        std::cerr << "Failed to initialize ImuReader (hardware not responding?)" << std::endl;
        udp_client->disconnect();
        return 1;
    }

    // ── Step 3: Direct blocking read (thread not running) ──
    std::cout << "\n[3] Direct blocking read (5 samples):" << std::endl;
    for (int i = 0; i < 5; ++i) {
        auto data = imu.read_imu();
        if (data) {
            print_imu_data(*data, "raw " + std::to_string(i + 1));
        } else {
            std::cerr << "    Read failed at sample " << (i + 1) << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // ── Step 4: Calibrate zero orientation ──
    std::cout << "\n[4] Calibrating zero orientation (current pose = zero)..." << std::endl;
    if (!imu.calibrate_zero_orientation()) {
        std::cerr << "    Calibration failed" << std::endl;
    }

    // Verify calibration applied: cached data should now be near zero
    auto cached = imu.get_cached_data();
    print_imu_data(cached, "post-cal cached");

    // ── Step 5: Start background thread and read cached data ──
    std::cout << "\n[5] Starting background update thread (10Hz)..." << std::endl;
    imu.start_update_thread();
    std::cout << "    Thread running: " << std::boolalpha << imu.is_thread_running() << std::endl;

    std::cout << "\n    Reading cached data for 5 seconds (Ctrl+C to stop early)...\n" << std::endl;
    const auto start = std::chrono::steady_clock::now();
    const auto duration = std::chrono::seconds(5);

    while (g_running) {
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed >= duration) break;

        auto data = imu.get_cached_data();
        auto age  = imu.get_data_age();

        std::cout << "\r    "
                  << "R=" << std::fixed << std::setprecision(2) << std::setw(8) << data.roll << "°  "
                  << "P=" << std::setw(8) << data.pitch << "°  "
                  << "Y=" << std::setw(8) << data.yaw << "°  "
                  << "age=" << std::setw(4) << age.count() << "ms"
                  << std::flush;

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    std::cout << std::endl;

    // ── Step 6: Stop and report ──
    std::cout << "\n[6] Stopping update thread..." << std::endl;
    imu.stop_update_thread();
    std::cout << "    Thread running: " << std::boolalpha << imu.is_thread_running() << std::endl;

    // Print final calibration state
    auto cal = imu.get_calibration();
    std::cout << "\n[7] Final calibration state:" << std::endl;
    std::cout << "    Calibrated: " << std::boolalpha << cal.is_calibrated << std::endl;
    std::cout << "    Offsets: roll=" << cal.roll_offset
              << "° pitch=" << cal.pitch_offset
              << "° yaw=" << cal.yaw_offset << "°" << std::endl;

    // Network stats
    auto stats = udp_client->get_statistics();
    std::cout << "\n[8] Network statistics:" << std::endl;
    std::cout << "    Sent: " << stats.packets_sent
              << "  Received: " << stats.packets_received
              << "  Errors: " << (stats.send_errors + stats.receive_errors)
              << "  Timeouts: " << stats.timeouts << std::endl;

    udp_client->disconnect();
    std::cout << "\n=== Test completed ===" << std::endl;
    return 0;
}
