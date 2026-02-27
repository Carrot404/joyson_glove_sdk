#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>
#include <chrono>

namespace joyson_glove {

// Protocol constants
constexpr uint8_t PACKET_HEADER = 0xEE;
constexpr uint8_t PACKET_TAIL = 0x7E;

// Module IDs
constexpr uint8_t MODULE_MOTOR = 0x01;
constexpr uint8_t MODULE_ENCODER = 0x02;
constexpr uint8_t MODULE_IMU = 0x03;

// Motor commands
constexpr uint8_t CMD_READ_STATUS = 0x00;
constexpr uint8_t CMD_READ_STATUS_ALT = 0x20;
constexpr uint8_t CMD_SET_MODE = 0x30;
constexpr uint8_t CMD_SET_POSITION = 0x31;
constexpr uint8_t CMD_SET_SPEED = 0x32;
constexpr uint8_t CMD_SET_FORCE = 0x33;

// Motor modes
constexpr uint8_t MODE_POSITION = 0x00;
constexpr uint8_t MODE_SERVO = 0x01;
constexpr uint8_t MODE_SPEED = 0x02;
constexpr uint8_t MODE_FORCE = 0x03;

// Motor registers
constexpr uint8_t REG_MODE = 0x25;
constexpr uint8_t REG_FORCE = 0x27;
constexpr uint8_t REG_SPEED = 0x28;
constexpr uint8_t REG_POSITION = 0x29;

// Hardware limits
constexpr uint8_t NUM_MOTORS = 6;
constexpr uint8_t NUM_ENCODER_CHANNELS = 16;
constexpr uint16_t MOTOR_POSITION_MAX = 2000;
constexpr uint16_t MOTOR_FORCE_MAX = 4095;
constexpr float ENCODER_VOLTAGE_MAX = 4.0f;
constexpr float ENCODER_ANGLE_MAX = 360.0f;

/**
 * UDP packet structure
 * Format: [Header(1)] [Length(2)] [ModuleID(1)] [Target(1)] [Command(1)] [Body(N)] [Checksum(2)] [Tail(1)]
 */
struct Packet {
    uint8_t header = PACKET_HEADER;
    uint16_t length = 0;  // Total packet length including header and tail
    uint8_t module_id = 0;
    uint8_t target = 0;
    uint8_t command = 0;
    std::vector<uint8_t> body;
    uint16_t checksum = 0;
    uint8_t tail = PACKET_TAIL;

    // Serialize packet to byte array
    std::vector<uint8_t> serialize() const;

    // Deserialize byte array to packet, returns nullopt if invalid
    static std::optional<Packet> deserialize(const std::vector<uint8_t>& data);

    // Calculate checksum (sum of all bytes, take low 16 bits)
    static uint16_t calculate_checksum(const std::vector<uint8_t>& data);

    // Validate checksum
    bool validate_checksum() const;
};

/**
 * Motor status data
 */
struct MotorStatus {
    uint8_t motor_id = 0;
    uint16_t position = 0;      // Current position [0, 2000]
    int16_t velocity = 0;       // Current velocity (steps/s)
    uint16_t force = 0;         // Current force [0, 4095]
    uint8_t mode = 0;           // Current mode
    uint8_t state = 0;          // Motor state flags
    std::chrono::steady_clock::time_point timestamp;
};

/**
 * Encoder data (16 channels)
 */
struct EncoderData {
    std::array<float, NUM_ENCODER_CHANNELS> voltages;  // ADC voltages [0, 4V]
    std::chrono::steady_clock::time_point timestamp;
};

/**
 * IMU orientation data
 */
struct ImuData {
    float roll = 0.0f;   // Roll angle in degrees
    float pitch = 0.0f;  // Pitch angle in degrees
    float yaw = 0.0f;    // Yaw angle in degrees
    std::chrono::steady_clock::time_point timestamp;
};

/**
 * Protocol encoder/decoder
 */
class ProtocolCodec {
public:
    // Motor commands
    static Packet encode_read_motor_status(uint8_t motor_id);
    static Packet encode_set_motor_mode(uint8_t motor_id, uint8_t mode);
    static Packet encode_set_motor_position(uint8_t motor_id, uint16_t position);
    static Packet encode_set_motor_force(uint8_t motor_id, uint16_t force);
    static Packet encode_set_motor_speed(uint8_t motor_id, uint16_t speed, uint16_t target_position);

    // Sensor commands
    static Packet encode_read_all_encoders();
    static Packet encode_read_imu();

    // Response parsers
    static std::optional<MotorStatus> parse_motor_status(const Packet& packet);
    static std::optional<EncoderData> parse_encoder_data(const Packet& packet);
    static std::optional<ImuData> parse_imu_data(const Packet& packet);

private:
    // Helper functions for byte conversion
    static void write_uint16_le(std::vector<uint8_t>& data, uint16_t value);
    static void write_int16_le(std::vector<uint8_t>& data, int16_t value);
    static void write_float_le(std::vector<uint8_t>& data, float value);
    static uint16_t read_uint16_le(const std::vector<uint8_t>& data, size_t offset);
    static int16_t read_int16_le(const std::vector<uint8_t>& data, size_t offset);
    static float read_float_le(const std::vector<uint8_t>& data, size_t offset);
};

} // namespace joyson_glove
