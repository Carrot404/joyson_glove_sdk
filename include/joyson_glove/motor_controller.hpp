#pragma once

#include "joyson_glove/protocol.hpp"
#include "joyson_glove/udp_client.hpp"
#include <array>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>

namespace joyson_glove {

/**
 * Motor controller configuration
 */
struct MotorControllerConfig {
    bool auto_start_thread = true;
    std::chrono::milliseconds update_interval{100};  // 10Hz default
};

/**
 * Motor controller for LAF actuators
 * Provides high-level motor control interface with status caching
 */
class MotorController {
public:
    explicit MotorController(std::shared_ptr<UdpClient> udp_client,
                            const MotorControllerConfig& config = MotorControllerConfig{});
    ~MotorController();

    // Disable copy and move
    MotorController(const MotorController&) = delete;
    MotorController& operator=(const MotorController&) = delete;
    MotorController(MotorController&&) = delete;
    MotorController& operator=(MotorController&&) = delete;

    /**
     * Initialize all motors (set to force mode with initial force 10 by default)
     * @return true if all motors initialized successfully
     */
    bool initialize();

    /**
     * Start background status update thread
     */
    void start_status_update_thread();

    /**
     * Stop background status update thread
     */
    void stop_status_update_thread();

    /**
     * Check if status update thread is running
     */
    bool is_thread_running() const { return thread_running_.load(std::memory_order_acquire); }

    // Single motor control
    bool set_motor_mode(uint8_t motor_id, uint8_t mode);
    bool set_motor_position(uint8_t motor_id, uint16_t position);
    bool set_motor_force(uint8_t motor_id, uint16_t force);
    bool set_motor_speed(uint8_t motor_id, uint16_t speed, uint16_t target_position);

    // Batch control (more efficient)
    bool set_all_modes(uint8_t mode);
    bool set_all_positions(const std::array<uint16_t, NUM_MOTORS>& positions);
    bool set_all_forces(const std::array<uint16_t, NUM_MOTORS>& forces);

    // Status reading
    std::optional<MotorStatus> get_motor_status(uint8_t motor_id);  // Blocking, queries hardware
    std::optional<MotorStatus> get_cached_status(uint8_t motor_id) const;  // Non-blocking, returns cached data
    std::array<MotorStatus, NUM_MOTORS> get_all_cached_status() const;

    /**
     * Get time since last status update for a motor
     */
    std::chrono::milliseconds get_status_age(uint8_t motor_id) const;

private:
    std::shared_ptr<UdpClient> udp_client_;
    MotorControllerConfig config_;

    // Cached motor status
    mutable std::mutex status_mutex_;
    std::array<MotorStatus, NUM_MOTORS> cached_status_;

    // Background thread
    std::atomic<bool> thread_running_{false};
    std::thread status_update_thread_;

    // Thread function
    void status_update_loop();

    // Helper: validate motor ID
    bool is_valid_motor_id(uint8_t motor_id) const {
        return motor_id >= 1 && motor_id <= NUM_MOTORS;
    }

    // Helper: validate motor mode
    bool is_valid_mode(uint8_t mode) const {
        return mode <= MODE_SPEED_FORCE;
    }

    // Helper: validate motor force
    bool is_valid_force(uint16_t force) const {
        return force <= MOTOR_FORCE_MAX;
    }

    // Helper: validate motor position
    bool is_valid_position(uint16_t position) const {
        return position <= MOTOR_POSITION_MAX;
    }
};

} // namespace joyson_glove
