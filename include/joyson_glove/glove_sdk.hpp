#pragma once

#include "joyson_glove/protocol.hpp"
#include "joyson_glove/udp_client.hpp"
#include "joyson_glove/motor_controller.hpp"
#include "joyson_glove/encoder_reader.hpp"
#include "joyson_glove/imu_reader.hpp"
#include <memory>
#include <string>

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

    // Component configuration
    bool auto_start_threads = true;
    std::chrono::milliseconds motor_update_interval{10};    // 100Hz
    std::chrono::milliseconds encoder_update_interval{10};  // 100Hz
    std::chrono::milliseconds imu_update_interval{10};      // 100Hz

};

/**
 * Main SDK interface for Joyson Glove
 * Provides unified API for motor control and sensor data acquisition
 */
class GloveSDK {
public:
    explicit GloveSDK(const GloveConfig& config = GloveConfig{});
    ~GloveSDK();

    // Disable copy, allow move
    GloveSDK(const GloveSDK&) = delete;
    GloveSDK& operator=(const GloveSDK&) = delete;
    GloveSDK(GloveSDK&&) noexcept;
    GloveSDK& operator=(GloveSDK&&) noexcept;

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
    bool is_initialized() const { return initialized_; }

    // Component access
    MotorController& motor_controller() { return *motor_controller_; }
    EncoderReader& encoder_reader() { return *encoder_reader_; }
    ImuReader& imu_reader() { return *imu_reader_; }
    UdpClient& udp_client() { return *udp_client_; }

    const MotorController& motor_controller() const { return *motor_controller_; }
    const EncoderReader& encoder_reader() const { return *encoder_reader_; }
    const ImuReader& imu_reader() const { return *imu_reader_; }
    const UdpClient& udp_client() const { return *udp_client_; }

    // Convenience methods - Motor control
    bool set_motor_position(uint8_t motor_id, uint16_t position);
    bool set_motor_force(uint8_t motor_id, uint16_t force);
    bool set_motor_speed(uint8_t motor_id, uint16_t speed, uint16_t target_position);
    bool set_all_positions(const std::array<uint16_t, NUM_MOTORS>& positions);
    bool set_all_forces(const std::array<uint16_t, NUM_MOTORS>& forces);

    // Convenience methods - Sensor reading
    std::array<float, NUM_ENCODER_CHANNELS> get_encoder_angles();
    ImuData get_imu_orientation();

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
    GloveConfig config_;
    bool initialized_ = false;

    // Components
    std::shared_ptr<UdpClient> udp_client_;
    std::unique_ptr<MotorController> motor_controller_;
    std::unique_ptr<EncoderReader> encoder_reader_;
    std::unique_ptr<ImuReader> imu_reader_;
};

} // namespace joyson_glove
