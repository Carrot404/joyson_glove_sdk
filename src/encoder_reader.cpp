#include "joyson_glove/encoder_reader.hpp"
#include <iostream>
#include <fstream>
#include <cstring>

namespace joyson_glove {

EncoderReader::EncoderReader(std::shared_ptr<UdpClient> udp_client,
                             const EncoderReaderConfig& config)
    : udp_client_(std::move(udp_client)), config_(config) {
    // Initialize calibration with default values
    calibration_.zero_voltages.fill(0.0f);
    calibration_.scale_factors.fill(1.0f);
    calibration_.is_calibrated = false;

    // Initialize cached data
    cached_data_.voltages.fill(0.0f);
    cached_data_.timestamp = std::chrono::steady_clock::now();
}

EncoderReader::~EncoderReader() {
    stop_update_thread();
}

EncoderReader::EncoderReader(EncoderReader&& other) noexcept
    : udp_client_(std::move(other.udp_client_)),
      config_(std::move(other.config_)),
      cached_data_(std::move(other.cached_data_)),
      calibration_(std::move(other.calibration_)),
      thread_running_(other.thread_running_.load()) {
    other.thread_running_ = false;
}

EncoderReader& EncoderReader::operator=(EncoderReader&& other) noexcept {
    if (this != &other) {
        stop_update_thread();
        udp_client_ = std::move(other.udp_client_);
        config_ = std::move(other.config_);
        cached_data_ = std::move(other.cached_data_);
        calibration_ = std::move(other.calibration_);
        thread_running_ = other.thread_running_.load();
        other.thread_running_ = false;
    }
    return *this;
}

bool EncoderReader::initialize() {
    std::cout << "[EncoderReader] Initializing..." << std::endl;

    // Try to load calibration
    if (!config_.calibration_file.empty()) {
        load_calibration(config_.calibration_file);
    }

    // Read initial data
    auto data = read_encoders();
    if (data) {
        std::lock_guard<std::mutex> lock(data_mutex_);
        cached_data_ = *data;
    } else {
        std::cerr << "[EncoderReader] Failed to read initial encoder data" << std::endl;
    }

    // Start background thread if configured
    if (config_.auto_start_thread) {
        start_update_thread();
    }

    std::cout << "[EncoderReader] Initialization complete" << std::endl;
    return true;
}

void EncoderReader::start_update_thread() {
    if (thread_running_.load()) {
        return;
    }

    thread_running_ = true;
    update_thread_ = std::thread(&EncoderReader::update_loop, this);
    std::cout << "[EncoderReader] Update thread started" << std::endl;
}

void EncoderReader::stop_update_thread() {
    if (!thread_running_.load()) {
        return;
    }

    thread_running_ = false;
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

std::array<float, NUM_ENCODER_CHANNELS> EncoderReader::get_cached_angles() const {
    auto data = get_cached_data();
    return voltages_to_angles(data.voltages);
}

bool EncoderReader::calibrate_zero_point() {
    std::cout << "[EncoderReader] Calibrating zero point..." << std::endl;

    // Read current voltages
    auto data = read_encoders();
    if (!data) {
        std::cerr << "[EncoderReader] Failed to read encoders for calibration" << std::endl;
        return false;
    }

    // Set current voltages as zero point
    std::lock_guard<std::mutex> lock(calibration_mutex_);
    calibration_.zero_voltages = data->voltages;
    calibration_.scale_factors.fill(1.0f);
    calibration_.is_calibrated = true;

    std::cout << "[EncoderReader] Zero point calibration complete" << std::endl;

    // Save calibration
    if (!config_.calibration_file.empty()) {
        save_calibration(config_.calibration_file);
    }

    return true;
}

bool EncoderReader::load_calibration(const std::string& filename) {
    std::string file = filename.empty() ? config_.calibration_file : filename;

    std::ifstream ifs(file, std::ios::binary);
    if (!ifs) {
        std::cerr << "[EncoderReader] Calibration file not found: " << file << std::endl;
        return false;
    }

    EncoderCalibration cal;
    ifs.read(reinterpret_cast<char*>(&cal.zero_voltages), sizeof(cal.zero_voltages));
    ifs.read(reinterpret_cast<char*>(&cal.scale_factors), sizeof(cal.scale_factors));
    ifs.read(reinterpret_cast<char*>(&cal.is_calibrated), sizeof(cal.is_calibrated));

    if (!ifs) {
        std::cerr << "[EncoderReader] Failed to read calibration file: " << file << std::endl;
        return false;
    }

    std::lock_guard<std::mutex> lock(calibration_mutex_);
    calibration_ = cal;

    std::cout << "[EncoderReader] Calibration loaded from " << file << std::endl;
    return true;
}

bool EncoderReader::save_calibration(const std::string& filename) const {
    std::string file = filename.empty() ? config_.calibration_file : filename;

    std::ofstream ofs(file, std::ios::binary);
    if (!ofs) {
        std::cerr << "[EncoderReader] Failed to create calibration file: " << file << std::endl;
        return false;
    }

    std::lock_guard<std::mutex> lock(calibration_mutex_);
    ofs.write(reinterpret_cast<const char*>(&calibration_.zero_voltages), sizeof(calibration_.zero_voltages));
    ofs.write(reinterpret_cast<const char*>(&calibration_.scale_factors), sizeof(calibration_.scale_factors));
    ofs.write(reinterpret_cast<const char*>(&calibration_.is_calibrated), sizeof(calibration_.is_calibrated));

    if (!ofs) {
        std::cerr << "[EncoderReader] Failed to write calibration file: " << file << std::endl;
        return false;
    }

    std::cout << "[EncoderReader] Calibration saved to " << file << std::endl;
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

void EncoderReader::update_loop() {
    while (thread_running_.load()) {
        auto start_time = std::chrono::steady_clock::now();

        // Read encoder data
        auto data = read_encoders();
        if (data) {
            std::lock_guard<std::mutex> lock(data_mutex_);
            cached_data_ = *data;
        }

        // Sleep for remaining time to maintain update rate
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        auto sleep_time = config_.update_interval -
                         std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);

        if (sleep_time.count() > 0) {
            std::this_thread::sleep_for(sleep_time);
        }
    }
}

std::array<float, NUM_ENCODER_CHANNELS> EncoderReader::apply_calibration(
    const std::array<float, NUM_ENCODER_CHANNELS>& raw_voltages) const {
    std::lock_guard<std::mutex> lock(calibration_mutex_);

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
