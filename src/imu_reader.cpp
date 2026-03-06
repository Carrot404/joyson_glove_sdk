#include "joyson_glove/imu_reader.hpp"
#include <cmath>
#include <iostream>

namespace joyson_glove {

ImuReader::ImuReader(std::shared_ptr<UdpClient> udp_client,
                     const ImuReaderConfig& config)
    : udp_client_(std::move(udp_client)), config_(config) {
    // Initialize with default values
    cached_data_.roll = 0.0f;
    cached_data_.pitch = 0.0f;
    cached_data_.yaw = 0.0f;
    cached_data_.timestamp = std::chrono::steady_clock::now();
}

ImuReader::~ImuReader() {
    try {
        stop_update_thread();
    } catch (...) {
        // Swallow exceptions - destructors must not throw
    }
}

bool ImuReader::initialize() {
    std::cout << "[ImuReader] Initializing..." << std::endl;

    // Read initial data to verify hardware connectivity
    auto data = read_imu();
    if (data) {
        std::lock_guard<std::mutex> lock(data_mutex_);
        cached_data_ = apply_calibration(*data);
    } else {
        std::cerr << "[ImuReader] Failed to read initial IMU data" << std::endl;
        return false;
    }

    // Start background thread if configured
    if (config_.auto_start_thread) {
        start_update_thread();
    }

    std::cout << "[ImuReader] Initialization complete" << std::endl;
    return true;
}

void ImuReader::start_update_thread() {
    if (thread_running_.load(std::memory_order_acquire)) {
        return;
    }

    thread_running_.store(true, std::memory_order_release);
    try {
        update_thread_ = std::thread(&ImuReader::update_loop, this);
        std::cout << "[ImuReader] Update thread started" << std::endl;
    } catch (const std::system_error& e) {
        thread_running_.store(false, std::memory_order_release);
        std::cerr << "[ImuReader] Failed to start update thread: " << e.what() << std::endl;
        throw;
    }
}

void ImuReader::stop_update_thread() {
    if (!thread_running_.load(std::memory_order_acquire)) {
        return;
    }

    thread_running_.store(false, std::memory_order_release);
    if (update_thread_.joinable()) {
        update_thread_.join();
    }
    std::cout << "[ImuReader] Update thread stopped" << std::endl;
}

std::optional<ImuData> ImuReader::read_imu() {
    auto packet = ProtocolCodec::encode_read_imu();
    auto response = udp_client_->send_and_receive(packet);

    if (!response) {
        return std::nullopt;
    }

    return ProtocolCodec::parse_imu_data(*response);
}

ImuData ImuReader::get_cached_data() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return cached_data_;
}

bool ImuReader::calibrate_zero_orientation() {
    std::cout << "[ImuReader] Calibrating zero orientation..." << std::endl;

    // Read current orientation
    auto data = read_imu();
    if (!data) {
        std::cerr << "[ImuReader] Failed to read IMU for calibration" << std::endl;
        return false;
    }

    // Set current orientation as zero point and update cached data atomically
    std::scoped_lock lock(calibration_mutex_, data_mutex_);
    calibration_.roll_offset  = data->roll;
    calibration_.pitch_offset = data->pitch;
    calibration_.yaw_offset   = data->yaw;
    calibration_.is_calibrated = true;

    // Re-apply calibration to cached data so get_cached_data() reflects the new zero
    cached_data_ = apply_calibration_unsafe(*data);

    std::cout << "[ImuReader] Zero orientation calibration complete" << std::endl;
    std::cout << "  Roll offset:  " << calibration_.roll_offset  << "°" << std::endl;
    std::cout << "  Pitch offset: " << calibration_.pitch_offset << "°" << std::endl;
    std::cout << "  Yaw offset:   " << calibration_.yaw_offset   << "°" << std::endl;

    return true;
}

ImuCalibration ImuReader::get_calibration() const {
    std::lock_guard<std::mutex> lock(calibration_mutex_);
    return calibration_;
}

void ImuReader::set_calibration(const ImuCalibration& calibration) {
    std::lock_guard<std::mutex> lock(calibration_mutex_);
    calibration_ = calibration;
}

std::chrono::milliseconds ImuReader::get_data_age() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    auto now = std::chrono::steady_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - cached_data_.timestamp);
    return age;
}

bool ImuReader::update_once() {
    // Read IMU data
    auto data = read_imu();
    if (data) {
        auto calibrated = apply_calibration(*data);
        std::lock_guard<std::mutex> lock(data_mutex_);
        cached_data_ = calibrated;
        return true;
    }
    return false;
}

void ImuReader::update_loop() {
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
                std::cerr << "[ImuReader] WARNING: " << consecutive_failures
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

ImuData ImuReader::apply_calibration(const ImuData& raw_data) const {
    std::lock_guard<std::mutex> lock(calibration_mutex_);
    return apply_calibration_unsafe(raw_data);
}

ImuData ImuReader::apply_calibration_unsafe(const ImuData& raw_data) const {
    if (!calibration_.is_calibrated) {
        return raw_data;
    }

    ImuData calibrated = raw_data;
    calibrated.roll -= calibration_.roll_offset;
    calibrated.pitch -= calibration_.pitch_offset;

    // Wrap yaw to [0, 360) to handle circular discontinuity
    calibrated.yaw = std::fmod(raw_data.yaw - calibration_.yaw_offset + 360.0f, 360.0f);

    return calibrated;
}

} // namespace joyson_glove
