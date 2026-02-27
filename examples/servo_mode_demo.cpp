#include "joyson_glove/glove_sdk.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <algorithm>

using namespace joyson_glove;

int main() {
    std::cout << "=== Joyson Glove SDK - Servo Mode Demo ===" << std::endl;
    std::cout << "This example demonstrates high-frequency motor control in servo mode" << std::endl;

    // Create SDK instance
    GloveConfig config;
    config.auto_start_threads = true;
    GloveSDK sdk(config);

    // Initialize SDK
    std::cout << "\n[1] Initializing SDK..." << std::endl;
    if (!sdk.initialize()) {
        std::cerr << "Failed to initialize SDK" << std::endl;
        return 1;
    }

    // Set all motors to servo mode
    std::cout << "\n[2] Setting motors to servo mode..." << std::endl;
    if (!sdk.motor_controller().set_all_modes(MODE_SERVO)) {
        std::cerr << "Failed to set motors to servo mode" << std::endl;
        return 1;
    }

    std::cout << "Servo mode enabled. Motors will follow sinusoidal trajectories." << std::endl;

    // Servo mode parameters
    const int control_rate_hz = 50;  // 50Hz = 20ms update rate (meets protocol requirement)
    const auto control_interval = std::chrono::milliseconds(1000 / control_rate_hz);
    const int duration_seconds = 10;
    const int total_iterations = duration_seconds * control_rate_hz;

    // Sinusoidal trajectory parameters
    const double frequency_hz = 0.5;  // 0.5Hz = 2 second period
    const double amplitude = 500.0;   // Position amplitude (steps)
    const double center = 1000.0;     // Center position (steps)

    std::cout << "\n[3] Starting servo control loop..." << std::endl;
    std::cout << "  Control rate: " << control_rate_hz << " Hz (" << control_interval.count() << " ms)" << std::endl;
    std::cout << "  Duration: " << duration_seconds << " seconds" << std::endl;
    std::cout << "  Trajectory: Sinusoidal (freq=" << frequency_hz << " Hz, amp=" << amplitude << " steps)" << std::endl;
    std::cout << std::endl;

    // Statistics
    int successful_updates = 0;
    int failed_updates = 0;
    std::chrono::microseconds total_latency{0};
    std::chrono::microseconds max_latency{0};
    std::chrono::microseconds min_latency{std::chrono::microseconds::max()};

    // Control loop
    for (int i = 0; i < total_iterations; ++i) {
        auto loop_start = std::chrono::steady_clock::now();

        // Calculate target positions for all motors (sinusoidal trajectory)
        double time_sec = static_cast<double>(i) / control_rate_hz;
        double phase = 2.0 * M_PI * frequency_hz * time_sec;

        std::array<uint16_t, NUM_MOTORS> target_positions;
        for (size_t motor_idx = 0; motor_idx < NUM_MOTORS; ++motor_idx) {
            // Each motor has a different phase offset
            double motor_phase = phase + (motor_idx * M_PI / 3.0);
            double position = center + amplitude * std::sin(motor_phase);
            target_positions[motor_idx] = static_cast<uint16_t>(std::clamp(position, 0.0, static_cast<double>(MOTOR_POSITION_MAX)));
        }

        // Send position commands
        auto send_start = std::chrono::steady_clock::now();
        bool success = sdk.set_all_positions(target_positions);
        auto send_end = std::chrono::steady_clock::now();

        // Update statistics
        if (success) {
            successful_updates++;
            auto latency = std::chrono::duration_cast<std::chrono::microseconds>(send_end - send_start);
            total_latency += latency;
            max_latency = std::max(max_latency, latency);
            min_latency = std::min(min_latency, latency);
        } else {
            failed_updates++;
        }

        // Display progress (every 10 iterations)
        if (i % 10 == 0) {
            std::cout << "\r[" << std::setw(3) << (i * 100 / total_iterations) << "%] "
                      << "Iteration: " << std::setw(4) << i << "/" << total_iterations << " | "
                      << "Motor 1 target: " << std::setw(4) << target_positions[0] << " | "
                      << "Success: " << successful_updates << " | "
                      << "Failed: " << failed_updates << std::flush;
        }

        // Sleep for remaining time to maintain control rate
        auto loop_end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(loop_end - loop_start);
        auto sleep_time = control_interval - elapsed;

        if (sleep_time.count() > 0) {
            std::this_thread::sleep_for(sleep_time);
        } else if (elapsed > control_interval + std::chrono::milliseconds(5)) {
            // Warn if loop is taking too long (more than 5ms over target)
            std::cerr << "\nWarning: Control loop overrun at iteration " << i
                      << " (took " << elapsed.count() << " ms)" << std::endl;
        }
    }

    std::cout << std::endl;

    // Display statistics
    std::cout << "\n[4] Servo control statistics:" << std::endl;
    std::cout << "  Total iterations: " << total_iterations << std::endl;
    std::cout << "  Successful updates: " << successful_updates << std::endl;
    std::cout << "  Failed updates: " << failed_updates << std::endl;
    std::cout << "  Success rate: " << std::fixed << std::setprecision(2)
              << (100.0 * successful_updates / total_iterations) << "%" << std::endl;

    if (successful_updates > 0) {
        auto avg_latency = total_latency / successful_updates;
        std::cout << "  Average latency: " << avg_latency.count() << " µs" << std::endl;
        std::cout << "  Min latency: " << min_latency.count() << " µs" << std::endl;
        std::cout << "  Max latency: " << max_latency.count() << " µs" << std::endl;
    }

    // Read final motor status
    std::cout << "\n[5] Final motor status:" << std::endl;
    for (uint8_t motor_id = 1; motor_id <= NUM_MOTORS; ++motor_id) {
        auto status = sdk.motor_controller().get_cached_status(motor_id);
        if (status) {
            std::cout << "  Motor " << static_cast<int>(motor_id) << ": "
                      << "Pos=" << status->position << " "
                      << "Vel=" << status->velocity << " "
                      << "Force=" << status->force << std::endl;
        }
    }

    // Display network statistics
    std::cout << "\n[6] Network statistics:" << std::endl;
    auto stats = sdk.get_network_statistics();
    std::cout << "  Packets sent: " << stats.packets_sent << std::endl;
    std::cout << "  Packets received: " << stats.packets_received << std::endl;
    std::cout << "  Send errors: " << stats.send_errors << std::endl;
    std::cout << "  Receive errors: " << stats.receive_errors << std::endl;
    std::cout << "  Timeouts: " << stats.timeouts << std::endl;
    std::cout << "  Checksum errors: " << stats.checksum_errors << std::endl;

    // Return motors to position mode
    std::cout << "\n[7] Returning motors to position mode..." << std::endl;
    sdk.motor_controller().set_all_modes(MODE_POSITION);

    // Shutdown SDK
    std::cout << "\n[8] Shutting down SDK..." << std::endl;
    sdk.shutdown();

    std::cout << "\n=== Example completed successfully ===" << std::endl;
    return 0;
}
