#include "joyson_glove/glove_sdk.hpp"
#include <iostream>

namespace joyson_glove {

GloveSDK::GloveSDK(const GloveConfig& config)
    : config_(config) {
}

GloveSDK::~GloveSDK() {
    shutdown();
}

GloveSDK::GloveSDK(GloveSDK&& other) noexcept
    : config_(std::move(other.config_)),
      initialized_(other.initialized_),
      udp_client_(std::move(other.udp_client_)),
      motor_controller_(std::move(other.motor_controller_)),
      encoder_reader_(std::move(other.encoder_reader_)),
      imu_reader_(std::move(other.imu_reader_)) {
    other.initialized_ = false;
}

GloveSDK& GloveSDK::operator=(GloveSDK&& other) noexcept {
    if (this != &other) {
        shutdown();
        config_ = std::move(other.config_);
        initialized_ = other.initialized_;
        udp_client_ = std::move(other.udp_client_);
        motor_controller_ = std::move(other.motor_controller_);
        encoder_reader_ = std::move(other.encoder_reader_);
        imu_reader_ = std::move(other.imu_reader_);
        other.initialized_ = false;
    }
    return *this;
}

bool GloveSDK::initialize() {
    if (initialized_) {
        std::cout << "[GloveSDK] Already initialized" << std::endl;
        return true;
    }

    std::cout << "[GloveSDK] Initializing..." << std::endl;

    // Create UDP client
    UdpConfig udp_config;
    udp_config.server_ip = config_.server_ip;
    udp_config.server_port = config_.server_port;
    udp_config.send_timeout = config_.send_timeout;
    udp_config.receive_timeout = config_.receive_timeout;

    udp_client_ = std::make_shared<UdpClient>(udp_config);

    // Connect to hardware
    if (!udp_client_->connect()) {
        std::cerr << "[GloveSDK] Failed to connect to hardware" << std::endl;
        return false;
    }

    // Create motor controller
    MotorControllerConfig motor_config;
    motor_config.auto_start_thread = config_.auto_start_threads;
    motor_config.update_interval = config_.motor_update_interval;
    motor_controller_ = std::make_unique<MotorController>(udp_client_, motor_config);

    if (!motor_controller_->initialize()) {
        std::cerr << "[GloveSDK] Failed to initialize motor controller" << std::endl;
        return false;
    }

    // Create encoder reader
    EncoderReaderConfig encoder_config;
    encoder_config.auto_start_thread = config_.auto_start_threads;
    encoder_config.update_interval = config_.encoder_update_interval;
    encoder_config.calibration_file = config_.encoder_calibration_file;
    encoder_reader_ = std::make_unique<EncoderReader>(udp_client_, encoder_config);

    if (!encoder_reader_->initialize()) {
        std::cerr << "[GloveSDK] Failed to initialize encoder reader" << std::endl;
        return false;
    }

    // Create IMU reader
    ImuReaderConfig imu_config;
    imu_config.auto_start_thread = config_.auto_start_threads;
    imu_config.update_interval = config_.imu_update_interval;
    imu_config.calibration_file = config_.imu_calibration_file;
    imu_reader_ = std::make_unique<ImuReader>(udp_client_, imu_config);

    if (!imu_reader_->initialize()) {
        std::cerr << "[GloveSDK] Failed to initialize IMU reader" << std::endl;
        return false;
    }

    initialized_ = true;
    std::cout << "[GloveSDK] Initialization complete" << std::endl;
    return true;
}

void GloveSDK::shutdown() {
    if (!initialized_) {
        return;
    }

    std::cout << "[GloveSDK] Shutting down..." << std::endl;

    // Stop all background threads
    if (motor_controller_) {
        motor_controller_->stop_status_update_thread();
    }
    if (encoder_reader_) {
        encoder_reader_->stop_update_thread();
    }
    if (imu_reader_) {
        imu_reader_->stop_update_thread();
    }

    // Disconnect UDP client
    if (udp_client_) {
        udp_client_->disconnect();
    }

    // Release resources
    motor_controller_.reset();
    encoder_reader_.reset();
    imu_reader_.reset();
    udp_client_.reset();

    initialized_ = false;
    std::cout << "[GloveSDK] Shutdown complete" << std::endl;
}

bool GloveSDK::set_motor_position(uint8_t motor_id, uint16_t position) {
    if (!initialized_) {
        std::cerr << "[GloveSDK] Not initialized" << std::endl;
        return false;
    }
    return motor_controller_->set_motor_position(motor_id, position);
}

bool GloveSDK::set_motor_force(uint8_t motor_id, uint16_t force) {
    if (!initialized_) {
        std::cerr << "[GloveSDK] Not initialized" << std::endl;
        return false;
    }
    return motor_controller_->set_motor_force(motor_id, force);
}

bool GloveSDK::set_motor_speed(uint8_t motor_id, uint16_t speed, uint16_t target_position) {
    if (!initialized_) {
        std::cerr << "[GloveSDK] Not initialized" << std::endl;
        return false;
    }
    return motor_controller_->set_motor_speed(motor_id, speed, target_position);
}

bool GloveSDK::set_all_positions(const std::array<uint16_t, NUM_MOTORS>& positions) {
    if (!initialized_) {
        std::cerr << "[GloveSDK] Not initialized" << std::endl;
        return false;
    }
    return motor_controller_->set_all_positions(positions);
}

bool GloveSDK::set_all_forces(const std::array<uint16_t, NUM_MOTORS>& forces) {
    if (!initialized_) {
        std::cerr << "[GloveSDK] Not initialized" << std::endl;
        return false;
    }
    return motor_controller_->set_all_forces(forces);
}

std::array<float, NUM_ENCODER_CHANNELS> GloveSDK::get_encoder_angles() {
    if (!initialized_) {
        std::cerr << "[GloveSDK] Not initialized" << std::endl;
        return {};
    }
    return encoder_reader_->get_cached_angles();
}

ImuData GloveSDK::get_imu_orientation() {
    if (!initialized_) {
        std::cerr << "[GloveSDK] Not initialized" << std::endl;
        return {};
    }
    return imu_reader_->get_cached_data();
}

bool GloveSDK::calibrate_all() {
    std::cout << "[GloveSDK] Calibrating all sensors..." << std::endl;
    bool success = true;

    if (!calibrate_encoders()) {
        success = false;
    }

    if (!calibrate_imu()) {
        success = false;
    }

    if (success) {
        std::cout << "[GloveSDK] All sensors calibrated successfully" << std::endl;
    } else {
        std::cerr << "[GloveSDK] Some sensors failed to calibrate" << std::endl;
    }

    return success;
}

bool GloveSDK::calibrate_encoders() {
    if (!initialized_) {
        std::cerr << "[GloveSDK] Not initialized" << std::endl;
        return false;
    }
    return encoder_reader_->calibrate_zero_point();
}

bool GloveSDK::calibrate_imu() {
    if (!initialized_) {
        std::cerr << "[GloveSDK] Not initialized" << std::endl;
        return false;
    }
    return imu_reader_->calibrate_zero_orientation();
}

NetworkStatistics GloveSDK::get_network_statistics() const {
    if (!initialized_ || !udp_client_) {
        return {};
    }
    return udp_client_->get_statistics();
}

void GloveSDK::reset_network_statistics() {
    if (initialized_ && udp_client_) {
        udp_client_->reset_statistics();
    }
}

} // namespace joyson_glove
