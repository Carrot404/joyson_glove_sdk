#include "joyson_glove/imu_reader.hpp"
#include <iostream>
#include <fstream>
#include <cstring>

namespace joyson_glove {

ImuReader::ImuReader(std::shared_ptr<UdpClient> udp_client,
                     const ImuReaderConfig& config)
    : udp_client_(std::move(udp_client)), config_(config) {
    // Initialize calibration with default values
    calibration_.roll_offset = 0.0f;
    calibration_.pitch_offset = 0.0f;
    calibration_.yaw_offset = 0.0f;
    calibration_.is_calibrated = false;

    // Initialize cached data
    cached_data_.roll = 0.0f;
    cached_data_.pitch = 0.0f;
    cached_data_.yaw = 0.0f;
    cached_data_.timestamp = std::chrono::steady_clock::now();
}

ImuReader::~ImuReader() {
    stop_update_thread();
}

ImuReader::ImuReader(ImuReader&& other) noexcept
    : udp_client_(std::move(other.udp_client_)),
      config_(std::move(other.config_)),
      cached_data_(std::move(other.cached_data_)),
      calibration_(std::move(other.calibration_)),
      thread_running_(other.thread_running_.load()) {
    other.thread_running_ = false;
}

ImuReader& ImuReader::operator=(ImuReader&& other) noexcept {
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

bool ImuReader::initialize() {
    std::cout << "[ImuReader] Initializing..." << std::endl;

    // Try to load calibration
    if (!config_.calibration_file.empty()) {
        load_calibration(config_.calibration_file);
    }

    // Read initial data
    auto data = read_imu();
    if (data) {
        std::lock_guard<std::mutex> lock(data_mutex_);
        cached_data_ = apply_calibration(*data);
    } else {
        std::cerr << "[ImuReader] Failed to read initial IMU data" << std::endl;
    }

    // Start background thread if configured
    if (config_.auto_start_thread) {
        start_update_thread();
    }

    std::cout << "[ImuReader] Initialization complete" << std::endl;
    return true;
}

void ImuReader::start_update_thread() {
    if (thread_running_.load()) {
        return;
    }

    thread_running_ = true;
    update_thread_ = std::thread(&ImuReader::update_loop, this);
    std::cout << "[ImuReader] Update thread started" << std::endl;
}

void ImuReader::stop_update_thread() {
    if (!thread_running_.load()) {
        return;
    }

    thread_running_ = false;
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

    // Set current orientation as zero point
    std::lock_guard<std::mutex> lock(calibration_mutex_);
    calibration_.roll_offset = data->roll;
    calibration_.pitch_offset = data->pitch;
    calibration_.yaw_offset = data->yaw;
    calibration_.is_calibrated = true;

    std::cout << "[ImuReader] Zero orientation calibration complete" << std::endl;
    std::cout << "  Roll offset: " << calibration_.roll_offset << "°" << std::endl;
    std::cout << "  Pitch offset: " << calibration_.pitch_offset << "°" << std::endl;
    std::cout << "  Yaw offset: " << calibration_.yaw_offset << "°" << std::endl;

    // Save calibration
    if (!config_.calibration_file.empty()) {
        save_calibration(config_.calibration_file);
    }

    return true;
}

bool ImuReader::load_calibration(const std::string& filename) {
    std::string file = filename.empty() ? config_.calibration_file : filename;

    std::ifstream ifs(file, std::ios::binary);
    if (!ifs) {
        std::cerr << "[ImuReader] Calibration file not found: " << file << std::endl;
        return false;
    }

    ImuCalibration cal;
    ifs.read(reinterpret_cast<char*>(&cal.roll_offset), sizeof(cal.roll_offset));
    ifs.read(reinterpret_cast<char*>(&cal.pitch_offset), sizeof(cal.pitch_offset));
    ifs.read(reinterpret_cast<char*>(&cal.yaw_offset), sizeof(cal.yaw_offset));
    ifs.read(reinterpret_cast<char*>(&cal.is_calibrated), sizeof(cal.is_calibrated));

    if (!ifs) {
        std::cerr << "[ImuReader] Failed to read calibration file: " << file << std::endl;
        return false;
    }

    std::lock_guard<std::mutex> lock(calibration_mutex_);
    calibration_ = cal;

    std::cout << "[ImuReader] Calibration loaded from " << file << std::endl;
    return true;
}

bool ImuReader::save_calibration(const std::string& filename) const {
    std::string file = filename.empty() ? config_.calibration_file : filename;

    std::ofstream ofs(file, std::ios::binary);
    if (!ofs) {
        std::cerr << "[ImuReader] Failed to create calibration file: " << file << std::endl;
        return false;
    }

    std::lock_guard<std::mutex> lock(calibration_mutex_);
    ofs.write(reinterpret_cast<const char*>(&calibration_.roll_offset), sizeof(calibration_.roll_offset));
    ofs.write(reinterpret_cast<const char*>(&calibration_.pitch_offset), sizeof(calibration_.pitch_offset));
    ofs.write(reinterpret_cast<const char*>(&calibration_.yaw_offset), sizeof(calibration_.yaw_offset));
    ofs.write(reinterpret_cast<const char*>(&calibration_.is_calibrated), sizeof(calibration_.is_calibrated));

    if (!ofs) {
        std::cerr << "[ImuReader] Failed to write calibration file: " << file << std::endl;
        return false;
    }

    std::cout << "[ImuReader] Calibration saved to " << file << std::endl;
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

void ImuReader::update_loop() {
    while (thread_running_.load()) {
        auto start_time = std::chrono::steady_clock::now();

        // Read IMU data
        auto data = read_imu();
        if (data) {
            auto calibrated = apply_calibration(*data);
            std::lock_guard<std::mutex> lock(data_mutex_);
            cached_data_ = calibrated;
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

ImuData ImuReader::apply_calibration(const ImuData& raw_data) const {
    std::lock_guard<std::mutex> lock(calibration_mutex_);

    if (!calibration_.is_calibrated) {
        return raw_data;
    }

    ImuData calibrated = raw_data;
    calibrated.roll -= calibration_.roll_offset;
    calibrated.pitch -= calibration_.pitch_offset;
    calibrated.yaw -= calibration_.yaw_offset;

    return calibrated;
}

} // namespace joyson_glove
