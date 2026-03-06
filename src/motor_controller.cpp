#include "joyson_glove/motor_controller.hpp"
#include <iostream>

namespace joyson_glove {

MotorController::MotorController(std::shared_ptr<UdpClient> udp_client,
                                 const MotorControllerConfig& config)
    : udp_client_(std::move(udp_client)), config_(config) {
    // Initialize cached status with default values
    for (size_t i = 0; i < NUM_MOTORS; ++i) {
        cached_status_[i].motor_id = static_cast<uint8_t>(i + 1);
        cached_status_[i].timestamp = std::chrono::steady_clock::now();
    }
}

MotorController::~MotorController() {
    try {
        stop_status_update_thread();
    } catch (...) {
        // Swallow exceptions - destructors must not throw
    }
}


bool MotorController::initialize() {
    std::cout << "[MotorController] Initializing motors..." << std::endl;

    // Set all motors to force mode with initial force value
    if (!set_all_modes(MODE_FORCE)) {
        std::cerr << "[MotorController] Failed to set motors to force mode" << std::endl;
        return false;
    }

    // Set initial force value for all motors
    constexpr uint16_t initial_force = 10;
    std::array<uint16_t, NUM_MOTORS> forces;
    forces.fill(initial_force);
    if (!set_all_forces(forces)) {
        std::cerr << "[MotorController] Failed to set initial force values" << std::endl;
        return false;
    }

    // // Query initial status for all motors
    // for (uint8_t motor_id = 1; motor_id <= NUM_MOTORS; ++motor_id) {
    //     auto status = get_motor_status(motor_id);
    //     if (status) {
    //         std::lock_guard<std::mutex> lock(status_mutex_);
    //         cached_status_[motor_id - 1] = *status;
    //     } else {
    //         std::cerr << "[MotorController] Failed to read initial status for motor "
    //                   << static_cast<int>(motor_id) << std::endl;
    //     }
    // }

    // Start background thread if configured
    if (config_.auto_start_thread) {
        start_status_update_thread();
    }

    std::cout << "[MotorController] Initialization complete" << std::endl;
    return true;
}

void MotorController::start_status_update_thread() {
    if (thread_running_.load(std::memory_order_acquire)) {
        return;
    }

    thread_running_.store(true, std::memory_order_release);
    try {
        status_update_thread_ = std::thread(&MotorController::status_update_loop, this);
        std::cout << "[MotorController] Status update thread started" << std::endl;
    } catch (const std::system_error& e) {
        thread_running_.store(false, std::memory_order_release);
        std::cerr << "[MotorController] Failed to start status update thread: " << e.what() << std::endl;
        throw;
    }
}

void MotorController::stop_status_update_thread() {
    if (!thread_running_.load(std::memory_order_acquire)) {
        return;
    }

    thread_running_.store(false, std::memory_order_release);
    if (status_update_thread_.joinable()) {
        status_update_thread_.join();
    }
    std::cout << "[MotorController] Status update thread stopped" << std::endl;
}

bool MotorController::set_motor_mode(uint8_t motor_id, uint8_t mode) {
    if (!is_valid_motor_id(motor_id)) {
        std::cerr << "[MotorController] Invalid motor ID: " << static_cast<int>(motor_id) << std::endl;
        return false;
    }

    if (!is_valid_mode(mode)) {
        std::cerr << "[MotorController] Invalid motor mode: " << static_cast<int>(mode) << std::endl;
        return false;
    }

    auto packet = ProtocolCodec::encode_set_motor_mode(motor_id, mode);
    auto response = udp_client_->send_and_receive(packet);

    if (!response) {
        std::cerr << "[MotorController] Failed to set mode for motor " << static_cast<int>(motor_id) << std::endl;
        return false;
    }

    return true;
}

bool MotorController::set_motor_position(uint8_t motor_id, uint16_t position) {
    if (!is_valid_motor_id(motor_id)) {
        std::cerr << "[MotorController] Invalid motor ID: " << static_cast<int>(motor_id) << std::endl;
        return false;
    }

    if (!is_valid_position(position)) {
        std::cerr << "[MotorController] Position out of range: " << position << std::endl;
        return false;
    }

    auto packet = ProtocolCodec::encode_set_motor_position(motor_id, position);
    auto response = udp_client_->send_and_receive(packet);

    if (!response) {
        std::cerr << "[MotorController] Failed to set position for motor " << static_cast<int>(motor_id) << std::endl;
        return false;
    }

    return true;
}

bool MotorController::set_motor_force(uint8_t motor_id, uint16_t force) {
    if (!is_valid_motor_id(motor_id)) {
        std::cerr << "[MotorController] Invalid motor ID: " << static_cast<int>(motor_id) << std::endl;
        return false;
    }

    if (!is_valid_force(force)) {
        std::cerr << "[MotorController] Force out of range: " << force << std::endl;
        return false;
    }

    auto packet = ProtocolCodec::encode_set_motor_force(motor_id, force);
    auto response = udp_client_->send_and_receive(packet);

    if (!response) {
        std::cerr << "[MotorController] Failed to set force for motor " << static_cast<int>(motor_id) << std::endl;
        return false;
    }

    return true;
}

bool MotorController::set_motor_speed(uint8_t motor_id, uint16_t speed, uint16_t target_position) {
    if (!is_valid_motor_id(motor_id)) {
        std::cerr << "[MotorController] Invalid motor ID: " << static_cast<int>(motor_id) << std::endl;
        return false;
    }

    if (!is_valid_position(target_position)) {
        std::cerr << "[MotorController] Target position out of range: " << target_position << std::endl;
        return false;
    }

    auto packet = ProtocolCodec::encode_set_motor_speed(motor_id, speed, target_position);
    auto response = udp_client_->send_and_receive(packet);

    if (!response) {
        std::cerr << "[MotorController] Failed to set speed for motor " << static_cast<int>(motor_id) << std::endl;
        return false;
    }

    return true;
}

bool MotorController::set_all_modes(uint8_t mode) {
    bool success = true;
    for (uint8_t motor_id = 1; motor_id <= NUM_MOTORS; ++motor_id) {
        if (!set_motor_mode(motor_id, mode)) {
            success = false;
        }
    }
    return success;
}

bool MotorController::set_all_positions(const std::array<uint16_t, NUM_MOTORS>& positions) {
    bool success = true;
    for (size_t i = 0; i < NUM_MOTORS; ++i) {
        uint8_t motor_id = static_cast<uint8_t>(i + 1);
        if (!set_motor_position(motor_id, positions[i])) {
            success = false;
        }
    }
    return success;
}

bool MotorController::set_all_forces(const std::array<uint16_t, NUM_MOTORS>& forces) {
    bool success = true;
    for (size_t i = 0; i < NUM_MOTORS; ++i) {
        uint8_t motor_id = static_cast<uint8_t>(i + 1);
        if (!set_motor_force(motor_id, forces[i])) {
            success = false;
        }
    }
    return success;
}

std::optional<MotorStatus> MotorController::get_motor_status(uint8_t motor_id) {
    if (!is_valid_motor_id(motor_id)) {
        std::cerr << "[MotorController] Invalid motor ID: " << static_cast<int>(motor_id) << std::endl;
        return std::nullopt;
    }

    auto packet = ProtocolCodec::encode_read_motor_status(motor_id);
    auto response = udp_client_->send_and_receive(packet);

    if (!response) {
        return std::nullopt;
    }

    return ProtocolCodec::parse_motor_status(*response);
}

std::optional<MotorStatus> MotorController::get_cached_status(uint8_t motor_id) const {
    if (!is_valid_motor_id(motor_id)) {
        return std::nullopt;
    }

    std::lock_guard<std::mutex> lock(status_mutex_);
    return cached_status_[motor_id - 1];
}

std::array<MotorStatus, NUM_MOTORS> MotorController::get_all_cached_status() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return cached_status_;
}

std::chrono::milliseconds MotorController::get_status_age(uint8_t motor_id) const {
    if (!is_valid_motor_id(motor_id)) {
        return std::chrono::milliseconds::max();
    }

    std::lock_guard<std::mutex> lock(status_mutex_);
    auto now = std::chrono::steady_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - cached_status_[motor_id - 1].timestamp);
    return age;
}

void MotorController::status_update_loop() {
    int consecutive_failures = 0;

    while (thread_running_.load(std::memory_order_acquire)) {
        auto start_time = std::chrono::steady_clock::now();

        // Update status for all motors
        for (uint8_t motor_id = 1; motor_id <= NUM_MOTORS; ++motor_id) {
            if (!thread_running_.load(std::memory_order_acquire)) {
                break;
            }

            auto status = get_motor_status(motor_id);
            if (status) {
                consecutive_failures = 0;
                std::lock_guard<std::mutex> lock(status_mutex_);
                cached_status_[motor_id - 1] = *status;
            } else {
                ++consecutive_failures;
                if (consecutive_failures == 10 ||
                    (consecutive_failures > 0 && consecutive_failures % 100 == 0)) {
                    std::cerr << "[MotorController] WARNING: " << consecutive_failures
                              << " consecutive read failures" << std::endl;
                }
            }
        }

        // Sleep for remaining interval without truncation loss
        auto elapsed    = std::chrono::steady_clock::now() - start_time;
        auto sleep_time = config_.update_interval - elapsed;

        if (sleep_time > std::chrono::steady_clock::duration::zero()) {
            std::this_thread::sleep_for(sleep_time);
        }
    }
}

} // namespace joyson_glove
