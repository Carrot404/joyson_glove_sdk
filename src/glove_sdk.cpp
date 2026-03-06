/**
 * @file glove_sdk.cpp
 * @brief Implementation of GloveSDK
 * @author Songjie Xiao
 * @copyright Copyright (c) 2026 Joyson Robot. All rights reserved.
 */

#include "joyson_glove/glove_sdk.hpp"
#include <iostream>

namespace joyson_glove {

GloveSDK::GloveSDK(const GloveConfig& config)
    : config_(config) {
}

GloveSDK::~GloveSDK() {
    try {
        shutdown();
    } catch (...) {
        // Swallow exceptions - destructors must not throw
    }
}

bool GloveSDK::initialize() {
    std::lock_guard<std::mutex> lock(init_mutex_);

    if (initialized_.load(std::memory_order_relaxed)) {
        std::cout << "[GloveSDK] Already initialized" << std::endl;
        return true;
    }

    std::cout << "[GloveSDK] Initializing..." << std::endl;

    // Create UDP client
    UdpConfig udp_config;
    udp_config.server_ip = config_.server_ip;
    udp_config.server_port = config_.server_port;
    udp_config.local_port = config_.local_port;
    udp_config.send_timeout = config_.send_timeout;
    udp_config.receive_timeout = config_.receive_timeout;

    udp_client_ = std::make_shared<UdpClient>(udp_config);

    if (!udp_client_->connect()) {
        std::cerr << "[GloveSDK] Failed to connect to hardware" << std::endl;
        cleanup_resources();
        return false;
    }

    // Create motor controller (always disable auto_start_thread)
    MotorControllerConfig motor_config;
    motor_config.auto_start_thread = false;
    motor_controller_ = std::make_unique<MotorController>(udp_client_, motor_config);

    if (!motor_controller_->initialize()) {
        std::cerr << "[GloveSDK] Failed to initialize motor controller" << std::endl;
        cleanup_resources();
        return false;
    }

    // Create encoder reader (always disable auto_start_thread)
    EncoderReaderConfig encoder_config;
    encoder_config.auto_start_thread = false;
    encoder_reader_ = std::make_unique<EncoderReader>(udp_client_, encoder_config);

    if (!encoder_reader_->initialize()) {
        std::cerr << "[GloveSDK] Failed to initialize encoder reader" << std::endl;
        cleanup_resources();
        return false;
    }

    // Create IMU reader (always disable auto_start_thread)
    ImuReaderConfig imu_config;
    imu_config.auto_start_thread = false;
    imu_reader_ = std::make_unique<ImuReader>(udp_client_, imu_config);

    if (!imu_reader_->initialize()) {
        std::cerr << "[GloveSDK] Failed to initialize IMU reader" << std::endl;
        cleanup_resources();
        return false;
    }

    initialized_.store(true, std::memory_order_release);
    std::cout << "[GloveSDK] Initialization complete" << std::endl;

    // Start unified thread if auto_start_thread is enabled
    if (config_.auto_start_thread) {
        start();
    }

    return true;
}

void GloveSDK::shutdown() {
    std::lock_guard<std::mutex> lock(init_mutex_);

    if (!initialized_.load(std::memory_order_relaxed)) {
        return;
    }

    std::cout << "[GloveSDK] Shutting down..." << std::endl;

    // Stop unified thread first
    stop();

    // Set flag before cleanup so other threads stop using components
    initialized_.store(false, std::memory_order_release);
    cleanup_resources();

    std::cout << "[GloveSDK] Shutdown complete" << std::endl;
}

bool GloveSDK::ensure_initialized() const {
    if (!initialized_.load(std::memory_order_acquire)) {
        std::cerr << "[GloveSDK] Not initialized" << std::endl;
        return false;
    }
    return true;
}

void GloveSDK::start() {
    if (!ensure_initialized()) {
        std::cerr << "[GloveSDK] Cannot start: SDK not initialized" << std::endl;
        return;
    }

    if (thread_running_.load(std::memory_order_acquire)) {
        std::cerr << "[GloveSDK] Unified update thread already running" << std::endl;
        return;
    }

    thread_running_.store(true, std::memory_order_release);
    update_thread_ = std::thread(&GloveSDK::unified_update_loop, this);
    std::cout << "[GloveSDK] Unified update thread started" << std::endl;
}

void GloveSDK::stop() {
    if (!thread_running_.load(std::memory_order_acquire)) {
        return;
    }

    thread_running_.store(false, std::memory_order_release);
    if (update_thread_.joinable()) {
        update_thread_.join();
    }
    std::cout << "[GloveSDK] Unified update thread stopped" << std::endl;
}

bool GloveSDK::is_running() const {
    return thread_running_.load(std::memory_order_acquire);
}

void GloveSDK::unified_update_loop() {
    constexpr auto MIN_SLEEP_TIME = std::chrono::milliseconds(1);
    constexpr auto OVERRUN_THRESHOLD = std::chrono::milliseconds(10);

    while (thread_running_.load(std::memory_order_acquire)) {
        auto start_time = std::chrono::steady_clock::now();

        // Update motor controller
        if (motor_controller_) {
            motor_controller_->update_once();
        }
        if (!thread_running_.load(std::memory_order_acquire)) {
            break;
        }

        // Update encoder reader
        if (encoder_reader_) {
            encoder_reader_->update_once();
        }
        if (!thread_running_.load(std::memory_order_acquire)) {
            break;
        }

        // Update IMU reader
        if (imu_reader_) {
            imu_reader_->update_once();
        }

        // Calculate sleep time and check for overruns
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        auto sleep_time = config_.update_interval - elapsed;

        // Warn if update loop is taking too long
        if (sleep_time < -OVERRUN_THRESHOLD) {
            auto overrun_ms = std::chrono::duration_cast<std::chrono::milliseconds>(-sleep_time).count();
            std::cerr << "[GloveSDK] Update loop overrun by " << overrun_ms << "ms" << std::endl;
        }

        // Sleep for remaining interval (with minimum threshold)
        if (sleep_time > MIN_SLEEP_TIME) {
            std::this_thread::sleep_for(sleep_time);
        }
    }
}

void GloveSDK::cleanup_resources() {
    // Clean up in reverse creation order
    // Note: Sub-components hold shared_ptr to udp_client_, so they can still use it
    // during their destruction. Destructors will handle thread cleanup automatically.
    imu_reader_.reset();
    encoder_reader_.reset();
    motor_controller_.reset();
    
    // Disconnect UDP after all components are destroyed
    if (udp_client_) {
        udp_client_->disconnect();
        udp_client_.reset();
    }
}

bool GloveSDK::set_motor_mode(uint8_t motor_id, uint8_t mode) {
    if (!ensure_initialized()) return false;
    return motor_controller_->set_motor_mode(motor_id, mode);
}

bool GloveSDK::set_all_modes(uint8_t mode) {
    if (!ensure_initialized()) return false;
    return motor_controller_->set_all_modes(mode);
}

bool GloveSDK::set_motor_position(uint8_t motor_id, uint16_t position) {
    if (!ensure_initialized()) return false;
    return motor_controller_->set_motor_position(motor_id, position);
}

bool GloveSDK::set_motor_force(uint8_t motor_id, uint16_t force) {
    if (!ensure_initialized()) return false;
    return motor_controller_->set_motor_force(motor_id, force);
}

bool GloveSDK::set_motor_speed(uint8_t motor_id, uint16_t speed, uint16_t target_position) {
    if (!ensure_initialized()) return false;
    return motor_controller_->set_motor_speed(motor_id, speed, target_position);
}

bool GloveSDK::set_all_positions(const std::array<uint16_t, NUM_MOTORS>& positions) {
    if (!ensure_initialized()) return false;
    return motor_controller_->set_all_positions(positions);
}

bool GloveSDK::set_all_forces(const std::array<uint16_t, NUM_MOTORS>& forces) {
    if (!ensure_initialized()) return false;
    return motor_controller_->set_all_forces(forces);
}

std::array<float, NUM_ENCODER_CHANNELS> GloveSDK::get_encoder_angles() const {
    if (!ensure_initialized()) return {};
    return encoder_reader_->get_cached_angles();
}

ImuData GloveSDK::get_imu_orientation() const {
    if (!ensure_initialized()) return {};
    return imu_reader_->get_cached_data();
}

bool GloveSDK::calibrate_all() {
    if (!ensure_initialized()) return false;

    std::cout << "[GloveSDK] Calibrating all sensors..." << std::endl;
    
    bool encoder_success = calibrate_encoders();
    bool imu_success = calibrate_imu();
    bool overall_success = encoder_success && imu_success;

    if (overall_success) {
        std::cout << "[GloveSDK] All sensors calibrated successfully" << std::endl;
    } else {
        std::cerr << "[GloveSDK] Calibration failed - "
                  << "Encoder: " << (encoder_success ? "OK" : "FAILED") << ", "
                  << "IMU: " << (imu_success ? "OK" : "FAILED") << std::endl;
    }

    return overall_success;
}

bool GloveSDK::calibrate_encoders() {
    if (!ensure_initialized()) return false;
    return encoder_reader_->calibrate_zero_point();
}

bool GloveSDK::calibrate_imu() {
    if (!ensure_initialized()) return false;
    return imu_reader_->calibrate_zero_orientation();
}

NetworkStatistics GloveSDK::get_network_statistics() const {
    if (!initialized_.load(std::memory_order_acquire) || !udp_client_) {
        return {};
    }
    return udp_client_->get_statistics();
}

void GloveSDK::reset_network_statistics() {
    if (initialized_.load(std::memory_order_acquire) && udp_client_) {
        udp_client_->reset_statistics();
    }
}

} // namespace joyson_glove
