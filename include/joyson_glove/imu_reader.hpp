#pragma once

#include "joyson_glove/protocol.hpp"
#include "joyson_glove/udp_client.hpp"
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>

namespace joyson_glove {

/**
 * IMU calibration data
 */
struct ImuCalibration {
    float roll_offset = 0.0f;
    float pitch_offset = 0.0f;
    float yaw_offset = 0.0f;
    bool is_calibrated = false;
};

/**
 * IMU reader configuration
 */
struct ImuReaderConfig {
    bool auto_start_thread = true;
    std::chrono::milliseconds update_interval{100};  // 10Hz default
};

/**
 * IMU reader for 3-axis orientation data
 * Provides orientation reading with drift compensation
 */
class ImuReader {
public:
    explicit ImuReader(std::shared_ptr<UdpClient> udp_client,
                      const ImuReaderConfig& config = ImuReaderConfig{});
    ~ImuReader();

    // Disable copy and move
    ImuReader(const ImuReader&) = delete;
    ImuReader& operator=(const ImuReader&) = delete;
    ImuReader(ImuReader&&) = delete;
    ImuReader& operator=(ImuReader&&) = delete;

    /**
     * Initialize IMU reader
     */
    bool initialize();

    /**
     * Start background data update thread
     */
    void start_update_thread();

    /**
     * Stop background data update thread
     */
    void stop_update_thread();

    /**
     * Check if update thread is running
     */
    bool is_thread_running() const { return thread_running_.load(std::memory_order_acquire); }

    /**
     * Read IMU data from hardware (blocking)
     *
     * WARNING: Do not call this method while the update thread is running,
     * as concurrent send/receive on the UDP socket may cause response mismatch.
     * Use get_cached_data() instead when the update thread is active.
     */
    std::optional<ImuData> read_imu();

    /**
     * Get cached IMU data (non-blocking)
     */
    ImuData get_cached_data() const;

    /**
     * Calibrate zero orientation (set current orientation as zero)
     */
    bool calibrate_zero_orientation();

    /**
     * Get calibration data
     */
    ImuCalibration get_calibration() const;

    /**
     * Set calibration data
     */
    void set_calibration(const ImuCalibration& calibration);

    /**
     * Get time since last data update
     */
    std::chrono::milliseconds get_data_age() const;

private:
    std::shared_ptr<UdpClient> udp_client_;
    ImuReaderConfig config_;

    // Cached data
    mutable std::mutex data_mutex_;
    ImuData cached_data_;

    // Calibration
    mutable std::mutex calibration_mutex_;
    ImuCalibration calibration_;

    // Background thread
    std::atomic<bool> thread_running_{false};
    std::thread update_thread_;

    // Thread function
    void update_loop();

    // Apply calibration to raw IMU data (thread-safe)
    ImuData apply_calibration(const ImuData& raw_data) const;

    // Apply calibration without locking (caller must hold calibration_mutex_)
    ImuData apply_calibration_unsafe(const ImuData& raw_data) const;
};

} // namespace joyson_glove
