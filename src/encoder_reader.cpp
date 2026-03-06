#include "joyson_glove/encoder_reader.hpp"
#include <iostream>

namespace joyson_glove {

EncoderReader::EncoderReader(std::shared_ptr<UdpClient> udp_client,
                             const EncoderReaderConfig& config)
    : udp_client_(std::move(udp_client)), config_(config) {
    // Initialize with default values
    cached_data_.adc_values.fill(0);
    cached_data_.timestamp = std::chrono::steady_clock::now();
}

EncoderReader::~EncoderReader() {
    try {
        stop_update_thread();
    } catch (...) {
        // Swallow exceptions - destructors must not throw
    }
}

bool EncoderReader::initialize() {
    std::cout << "[EncoderReader] Initializing..." << std::endl;

    // Read initial data to verify hardware connectivity
    auto data = read_encoders();
    if (data) {
        std::lock_guard<std::mutex> lock(data_mutex_);
        cached_data_ = *data;
    } else {
        std::cerr << "[EncoderReader] Failed to read initial encoder data" << std::endl;
        return false;
    }

    // Start background thread if configured
    if (config_.auto_start_thread) {
        start_update_thread();
    }

    std::cout << "[EncoderReader] Initialization complete" << std::endl;
    return true;
}

void EncoderReader::start_update_thread() {
    if (thread_running_.load(std::memory_order_acquire)) {
        return;
    }

    thread_running_.store(true, std::memory_order_release);
    try {
        update_thread_ = std::thread(&EncoderReader::update_loop, this);
        std::cout << "[EncoderReader] Update thread started" << std::endl;
    } catch (const std::system_error& e) {
        thread_running_.store(false, std::memory_order_release);
        std::cerr << "[EncoderReader] Failed to start update thread: " << e.what() << std::endl;
        throw;
    }
}

void EncoderReader::stop_update_thread() {
    if (!thread_running_.load(std::memory_order_acquire)) {
        return;
    }

    thread_running_.store(false, std::memory_order_release);
    if (update_thread_.joinable()) {
        update_thread_.join();
    }
    std::cout << "[EncoderReader] Update thread stopped" << std::endl;
}

std::optional<EncoderData> EncoderReader::read_encoders() {
    auto packet = ProtocolCodec::encode_read_all_encoders();
    auto response = udp_client_->send_and_receive(packet);

    if (!response) {
        return std::nullopt;
    }

    return ProtocolCodec::parse_encoder_data(*response);
}

EncoderData EncoderReader::get_cached_data() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return cached_data_;
}

std::array<float, NUM_ENCODER_CHANNELS> EncoderReader::voltages_to_angles(
    const std::array<float, NUM_ENCODER_CHANNELS>& voltages) const {
    std::array<float, NUM_ENCODER_CHANNELS> angles;

    // Apply calibration first
    auto calibrated_voltages = apply_calibration(voltages);

    // Convert to angles: 0V = 0°, 4V = 360°
    for (size_t i = 0; i < NUM_ENCODER_CHANNELS; ++i) {
        angles[i] = (calibrated_voltages[i] / ENCODER_VOLTAGE_MAX) * ENCODER_ANGLE_MAX;
    }

    return angles;
}

std::array<float, NUM_ENCODER_CHANNELS> EncoderReader::adc_to_voltages(
    const std::array<uint16_t, NUM_ENCODER_CHANNELS>& adc_values) const {
    std::array<float, NUM_ENCODER_CHANNELS> voltages;

    // Convert 16-bit ADC values (0-65535) to voltages (0-4V)
    constexpr float ADC_FULL_SCALE = 65536.0f;
    constexpr float VOLTAGE_MAX = 4.0f;

    for (size_t i = 0; i < NUM_ENCODER_CHANNELS; ++i) {
        voltages[i] = static_cast<float>(adc_values[i]) * VOLTAGE_MAX / ADC_FULL_SCALE;
    }

    return voltages;
}

std::array<float, NUM_ENCODER_CHANNELS> EncoderReader::get_cached_angles() const {
    auto data = get_cached_data();
    auto voltages = adc_to_voltages(data.adc_values);
    return voltages_to_angles(voltages);
}

bool EncoderReader::calibrate_zero_point() {
    std::cout << "[EncoderReader] Calibrating zero point..." << std::endl;

    // Read current voltages
    auto data = read_encoders();
    if (!data) {
        std::cerr << "[EncoderReader] Failed to read encoders for calibration" << std::endl;
        return false;
    }

    // Set current voltages as zero point and update cached data atomically
    std::scoped_lock lock(calibration_mutex_, data_mutex_);
    auto voltages = adc_to_voltages(data->adc_values);
    calibration_.zero_voltages = voltages;
    calibration_.scale_factors.fill(1.0f);
    calibration_.is_calibrated = true;

    // Re-apply calibration to cached data so get_cached_data() reflects the new zero
    cached_data_ = *data;

    std::cout << "[EncoderReader] Zero point calibration complete" << std::endl;

    return true;
}

EncoderCalibration EncoderReader::get_calibration() const {
    std::lock_guard<std::mutex> lock(calibration_mutex_);
    return calibration_;
}

void EncoderReader::set_calibration(const EncoderCalibration& calibration) {
    std::lock_guard<std::mutex> lock(calibration_mutex_);
    calibration_ = calibration;
}

std::chrono::milliseconds EncoderReader::get_data_age() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    auto now = std::chrono::steady_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - cached_data_.timestamp);
    return age;
}

bool EncoderReader::update_once() {
    // Read encoder data
    auto data = read_encoders();
    if (data) {
        std::lock_guard<std::mutex> lock(data_mutex_);
        cached_data_ = *data;
        return true;
    }
    return false;
}

void EncoderReader::update_loop() {
    int consecutive_failures = 0;

    while (thread_running_.load(std::memory_order_acquire)) {
        auto start_time = std::chrono::steady_clock::now();

        // Perform single update cycle
        bool success = update_once();
        if (success) {
            consecutive_failures = 0;
        } else {
            ++consecutive_failures;
            if (consecutive_failures == 10 ||
                (consecutive_failures > 0 && consecutive_failures % 100 == 0)) {
                std::cerr << "[EncoderReader] WARNING: " << consecutive_failures
                          << " consecutive read failures" << std::endl;
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

std::array<float, NUM_ENCODER_CHANNELS> EncoderReader::apply_calibration(
    const std::array<float, NUM_ENCODER_CHANNELS>& raw_voltages) const {
    std::lock_guard<std::mutex> lock(calibration_mutex_);
    return apply_calibration_unsafe(raw_voltages);
}

std::array<float, NUM_ENCODER_CHANNELS> EncoderReader::apply_calibration_unsafe(
    const std::array<float, NUM_ENCODER_CHANNELS>& raw_voltages) const {
    if (!calibration_.is_calibrated) {
        return raw_voltages;
    }

    std::array<float, NUM_ENCODER_CHANNELS> calibrated;
    for (size_t i = 0; i < NUM_ENCODER_CHANNELS; ++i) {
        calibrated[i] = (raw_voltages[i] - calibration_.zero_voltages[i]) * calibration_.scale_factors[i];
    }

    return calibrated;
}

} // namespace joyson_glove
