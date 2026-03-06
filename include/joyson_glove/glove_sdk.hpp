/**
 * @file glove_sdk.hpp
 * @brief Top-level SDK interface for Joyson Glove
 * @author Songjie Xiao
 * @copyright Copyright (c) 2026 Joyson Robot. All rights reserved.
 */

#pragma once

#include "joyson_glove/protocol.hpp"
#include "joyson_glove/udp_client.hpp"
#include "joyson_glove/motor_controller.hpp"
#include "joyson_glove/encoder_reader.hpp"
#include "joyson_glove/imu_reader.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace joyson_glove {

/**
 * Glove SDK configuration
 */
struct GloveConfig {
    // Network configuration
    std::string server_ip = "192.168.10.123";
    uint16_t server_port = 8080;
    uint16_t local_port = 8080;  // Local port to bind (required for hardware)
    std::chrono::milliseconds send_timeout{100};
    std::chrono::milliseconds receive_timeout{100};

    // Unified thread configuration
    bool auto_start_thread = true;
    std::chrono::milliseconds update_interval{100};  // 10Hz unified update rate
};

/**
 * Main SDK interface for Joyson Glove
 * Provides unified API for motor control and sensor data acquisition
 */
class GloveSDK {
public:
    explicit GloveSDK(const GloveConfig& config = GloveConfig{});
    ~GloveSDK();

    // Disable copy and move
    GloveSDK(const GloveSDK&) = delete;
    GloveSDK& operator=(const GloveSDK&) = delete;
    GloveSDK(GloveSDK&&) = delete;
    GloveSDK& operator=(GloveSDK&&) = delete;

    /**
     * Initialize SDK and connect to hardware
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * Shutdown SDK and disconnect from hardware
     */
    void shutdown();

    /**
     * Check if SDK is initialized
     */
    bool is_initialized() const { return initialized_.load(std::memory_order_acquire); }

    /**
     * @brief Start the unified update thread
     */
    void start();

    /**
     * @brief Stop the unified update thread
     */
    void stop();

    /**
     * @brief Check if the unified update thread is running
     * @return true if thread is running
     */
    bool is_running() const;

    // Component access (SDK must be initialized, asserts in debug builds)
    MotorController& motor_controller() {
        assert(motor_controller_ && "SDK not initialized");
        return *motor_controller_;
    }
    EncoderReader& encoder_reader() {
        assert(encoder_reader_ && "SDK not initialized");
        return *encoder_reader_;
    }
    ImuReader& imu_reader() {
        assert(imu_reader_ && "SDK not initialized");
        return *imu_reader_;
    }
    UdpClient& udp_client() {
        assert(udp_client_ && "SDK not initialized");
        return *udp_client_;
    }

    const MotorController& motor_controller() const {
        assert(motor_controller_ && "SDK not initialized");
        return *motor_controller_;
    }
    const EncoderReader& encoder_reader() const {
        assert(encoder_reader_ && "SDK not initialized");
        return *encoder_reader_;
    }
    const ImuReader& imu_reader() const {
        assert(imu_reader_ && "SDK not initialized");
        return *imu_reader_;
    }
    const UdpClient& udp_client() const {
        assert(udp_client_ && "SDK not initialized");
        return *udp_client_;
    }

    // Convenience methods - Motor control
    bool set_motor_mode(uint8_t motor_id, uint8_t mode);
    bool set_all_modes(uint8_t mode);
    bool set_motor_position(uint8_t motor_id, uint16_t position);
    bool set_motor_force(uint8_t motor_id, uint16_t force);
    bool set_motor_speed(uint8_t motor_id, uint16_t speed, uint16_t target_position);
    bool set_all_positions(const std::array<uint16_t, NUM_MOTORS>& positions);
    bool set_all_forces(const std::array<uint16_t, NUM_MOTORS>& forces);

    // Convenience methods - Sensor reading
    std::array<float, NUM_ENCODER_CHANNELS> get_encoder_angles() const;
    ImuData get_imu_orientation() const;

    // Calibration
    bool calibrate_all();
    bool calibrate_encoders();
    bool calibrate_imu();

    // Statistics
    NetworkStatistics get_network_statistics() const;
    void reset_network_statistics();

    /**
     * Get configuration
     */
    const GloveConfig& get_config() const { return config_; }

private:
    /**
     * Check if SDK is initialized, log error if not
     * @return true if initialized
     */
    bool ensure_initialized() const;

    /**
     * Release all component resources in reverse creation order
     */
    void cleanup_resources();

    // Unified update thread
    void unified_update_loop();
    std::atomic<bool> thread_running_{false};
    std::thread update_thread_;

    GloveConfig config_;
    std::mutex init_mutex_;
    std::atomic<bool> initialized_{false};

    // Components (udp_client_ is shared_ptr as it's shared with sub-components)
    std::shared_ptr<UdpClient> udp_client_;
    std::unique_ptr<MotorController> motor_controller_;
    std::unique_ptr<EncoderReader> encoder_reader_;
    std::unique_ptr<ImuReader> imu_reader_;
};

} // namespace joyson_glove
