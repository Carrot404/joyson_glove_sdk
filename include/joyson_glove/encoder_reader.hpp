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
 * Encoder calibration data
 */
struct EncoderCalibration {
    std::array<float, NUM_ENCODER_CHANNELS> zero_voltages{};   // Zero-point voltages (default 0.0)
    std::array<float, NUM_ENCODER_CHANNELS> scale_factors{};   // Scale factors (default 1.0)
    bool is_calibrated = false;

    EncoderCalibration() {
        zero_voltages.fill(0.0f);
        scale_factors.fill(1.0f);
    }
};

/**
 * Encoder reader configuration
 */
struct EncoderReaderConfig {
    bool auto_start_thread = true;
    std::chrono::milliseconds update_interval{100};  // 10Hz default
};

/**
 * Encoder reader for 16-channel ADC data
 * Provides voltage reading and angle conversion with calibration support
 */
class EncoderReader {
public:
    explicit EncoderReader(std::shared_ptr<UdpClient> udp_client,
                          const EncoderReaderConfig& config = EncoderReaderConfig{});
    ~EncoderReader();

    // Disable copy and move
    EncoderReader(const EncoderReader&) = delete;
    EncoderReader& operator=(const EncoderReader&) = delete;
    EncoderReader(EncoderReader&&) = delete;
    EncoderReader& operator=(EncoderReader&&) = delete;

    /**
     * Initialize encoder reader
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
     * Read encoder voltages from hardware (blocking)
     *
     * WARNING: Do not call this method while the update thread is running,
     * as concurrent send/receive on the UDP socket may cause response mismatch.
     * Use get_cached_data() instead when the update thread is active.
     */
    std::optional<EncoderData> read_encoders();

    /**
     * Get cached encoder data (non-blocking)
     */
    EncoderData get_cached_data() const;

    /**
     * Convert ADC values to voltages (16-bit ADC, 0-65535 -> 0-4V)
     */
    std::array<float, NUM_ENCODER_CHANNELS> adc_to_voltages(
        const std::array<uint16_t, NUM_ENCODER_CHANNELS>& adc_values) const;

    /**
     * Convert voltages to angles (0V=0°, 4V=360°)
     */
    std::array<float, NUM_ENCODER_CHANNELS> voltages_to_angles(
        const std::array<float, NUM_ENCODER_CHANNELS>& voltages) const;

    /**
     * Get cached angles (non-blocking)
     */
    std::array<float, NUM_ENCODER_CHANNELS> get_cached_angles() const;

    /**
     * Calibrate zero point (set current position as zero)
     */
    bool calibrate_zero_point();

    /**
     * Get calibration data
     */
    EncoderCalibration get_calibration() const;

    /**
     * Set calibration data
     */
    void set_calibration(const EncoderCalibration& calibration);

    /**
     * Get time since last data update
     */
    std::chrono::milliseconds get_data_age() const;

private:
    std::shared_ptr<UdpClient> udp_client_;
    EncoderReaderConfig config_;

    // Cached data
    mutable std::mutex data_mutex_;
    EncoderData cached_data_;

    // Calibration
    mutable std::mutex calibration_mutex_;
    EncoderCalibration calibration_;

    // Background thread
    std::atomic<bool> thread_running_{false};
    std::thread update_thread_;

    // Thread function
    void update_loop();

    // Apply calibration to raw voltages (thread-safe)
    std::array<float, NUM_ENCODER_CHANNELS> apply_calibration(
        const std::array<float, NUM_ENCODER_CHANNELS>& raw_voltages) const;

    // Apply calibration without locking (caller must hold calibration_mutex_)
    std::array<float, NUM_ENCODER_CHANNELS> apply_calibration_unsafe(
        const std::array<float, NUM_ENCODER_CHANNELS>& raw_voltages) const;
};

} // namespace joyson_glove
