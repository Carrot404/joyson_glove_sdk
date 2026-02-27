#include "joyson_glove/glove_sdk.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>

using namespace joyson_glove;

int main() {
    std::cout << "=== Joyson Glove SDK - Read Sensors Example ===" << std::endl;

    // Create SDK instance with background threads enabled
    GloveConfig config;
    config.auto_start_threads = true;  // Enable background data updates
    GloveSDK sdk(config);

    // Initialize SDK
    std::cout << "\n[1] Initializing SDK..." << std::endl;
    if (!sdk.initialize()) {
        std::cerr << "Failed to initialize SDK" << std::endl;
        return 1;
    }

    // Wait for background threads to start collecting data
    std::cout << "\n[2] Waiting for sensor data..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Calibrate sensors (optional)
    std::cout << "\n[3] Calibrating sensors (set current position as zero)..." << std::endl;
    std::cout << "Press Enter to calibrate, or Ctrl+C to skip..." << std::endl;
    std::cin.get();

    if (sdk.calibrate_all()) {
        std::cout << "Calibration successful!" << std::endl;
    } else {
        std::cerr << "Calibration failed, continuing with uncalibrated data..." << std::endl;
    }

    // Read sensor data at 10Hz for 10 seconds
    std::cout << "\n[4] Reading sensor data (10Hz for 10 seconds)..." << std::endl;
    std::cout << "Move the glove to see data changes\n" << std::endl;

    const int duration_seconds = 10;
    const int update_rate_hz = 10;
    const auto update_interval = std::chrono::milliseconds(1000 / update_rate_hz);

    for (int i = 0; i < duration_seconds * update_rate_hz; ++i) {
        auto start_time = std::chrono::steady_clock::now();

        // Read encoder angles
        auto encoder_angles = sdk.get_encoder_angles();

        // Read IMU orientation
        auto imu_data = sdk.get_imu_orientation();

        // Display data
        std::cout << "\r[" << std::setw(3) << i + 1 << "/" << duration_seconds * update_rate_hz << "] ";

        // Show first 4 encoder channels (to keep output compact)
        std::cout << "Encoders: ";
        for (size_t j = 0; j < 4; ++j) {
            std::cout << "CH" << j << "=" << std::fixed << std::setprecision(1)
                      << encoder_angles[j] << "° ";
        }

        // Show IMU data
        std::cout << "| IMU: R=" << std::fixed << std::setprecision(1) << imu_data.roll
                  << "° P=" << imu_data.pitch << "° Y=" << imu_data.yaw << "°";

        std::cout << std::flush;

        // Sleep for remaining time
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        auto sleep_time = update_interval - std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
        if (sleep_time.count() > 0) {
            std::this_thread::sleep_for(sleep_time);
        }
    }

    std::cout << std::endl;

    // Display detailed encoder data
    std::cout << "\n[5] Detailed encoder data (all 16 channels):" << std::endl;
    auto encoder_data = sdk.encoder_reader().get_cached_data();
    auto encoder_angles = sdk.get_encoder_angles();

    for (size_t i = 0; i < NUM_ENCODER_CHANNELS; ++i) {
        std::cout << "  CH" << std::setw(2) << i << ": "
                  << std::fixed << std::setprecision(3) << encoder_data.voltages[i] << " V  ->  "
                  << std::fixed << std::setprecision(1) << encoder_angles[i] << "°" << std::endl;
    }

    // Display motor status
    std::cout << "\n[6] Motor status:" << std::endl;
    auto motor_status = sdk.motor_controller().get_all_cached_status();
    for (size_t i = 0; i < NUM_MOTORS; ++i) {
        std::cout << "  Motor " << (i + 1) << ": "
                  << "Pos=" << motor_status[i].position << " "
                  << "Vel=" << motor_status[i].velocity << " "
                  << "Force=" << motor_status[i].force << std::endl;
    }

    // Display data age (freshness)
    std::cout << "\n[7] Data freshness:" << std::endl;
    std::cout << "  Encoder data age: " << sdk.encoder_reader().get_data_age().count() << " ms" << std::endl;
    std::cout << "  IMU data age: " << sdk.imu_reader().get_data_age().count() << " ms" << std::endl;
    std::cout << "  Motor status age (Motor 1): " << sdk.motor_controller().get_status_age(1).count() << " ms" << std::endl;

    // Display network statistics
    std::cout << "\n[8] Network statistics:" << std::endl;
    auto stats = sdk.get_network_statistics();
    std::cout << "  Packets sent: " << stats.packets_sent << std::endl;
    std::cout << "  Packets received: " << stats.packets_received << std::endl;
    std::cout << "  Send errors: " << stats.send_errors << std::endl;
    std::cout << "  Receive errors: " << stats.receive_errors << std::endl;
    std::cout << "  Timeouts: " << stats.timeouts << std::endl;
    std::cout << "  Checksum errors: " << stats.checksum_errors << std::endl;

    double success_rate = 0.0;
    if (stats.packets_sent > 0) {
        success_rate = 100.0 * stats.packets_received / stats.packets_sent;
    }
    std::cout << "  Success rate: " << std::fixed << std::setprecision(2) << success_rate << "%" << std::endl;

    // Shutdown SDK
    std::cout << "\n[9] Shutting down SDK..." << std::endl;
    sdk.shutdown();

    std::cout << "\n=== Example completed successfully ===" << std::endl;
    return 0;
}
