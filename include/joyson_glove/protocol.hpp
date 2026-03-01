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

// Module ID (fixed)
constexpr uint8_t MODULE_ID = 0x01;

// Target IDs (board modules)
constexpr uint8_t TARGET_MOTOR = 0x01;
constexpr uint8_t TARGET_ENCODER = 0x02;
constexpr uint8_t TARGET_IMU = 0x03;
constexpr uint8_t TARGET_LRA = 0x04;

// Motor commands
constexpr uint8_t CMD_READ_MOTOR_ID = 0x10;
constexpr uint8_t CMD_READ_STATUS = 0x20;
constexpr uint8_t CMD_SET_MODE = 0x30;
constexpr uint8_t CMD_SET_POSITION = 0x31;
constexpr uint8_t CMD_SET_SPEED = 0x32;
constexpr uint8_t CMD_SET_FORCE = 0x33;

// Inner motor protocol headers
constexpr uint8_t MOTOR_HEADER_REQ_1 = 0x55;
constexpr uint8_t MOTOR_HEADER_REQ_2 = 0xAA;
constexpr uint8_t MOTOR_HEADER_RESP_1 = 0xAA;
constexpr uint8_t MOTOR_HEADER_RESP_2 = 0x55;

// Inner motor instruction types
constexpr uint8_t MOTOR_INSTR_READ_REG = 0x31;
constexpr uint8_t MOTOR_INSTR_HOST_CMD = 0x30;
constexpr uint8_t MOTOR_INSTR_READ_WRITE = 0x32;

// Motor modes
constexpr uint8_t MODE_POSITION = 0x00;
constexpr uint8_t MODE_SERVO = 0x01;
constexpr uint8_t MODE_SPEED = 0x02;
constexpr uint8_t MODE_FORCE = 0x03;
constexpr uint8_t MODE_VOLTAGE = 0x04;
constexpr uint8_t MODE_SPEED_FORCE = 0x05;

// Motor registers
constexpr uint8_t REG_MOTOR_ID = 0x16;
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
 * Format: [Header(1)] [Length(1)] [ModuleID(1)] [Target(1)] [Command(1)] [Body(N)] [Checksum(1)] [Tail(1)]
 */
struct Packet {
    uint8_t header = PACKET_HEADER;
    uint8_t length = 0;  // Length from Target to end of Body (total_length - 5)
    uint8_t module_id = MODULE_ID;
    uint8_t target = 0;
    uint8_t command = 0;
    std::vector<uint8_t> body;
    uint8_t checksum = 0;
    uint8_t tail = PACKET_TAIL;

    // Serialize packet to byte array
    std::vector<uint8_t> serialize() const;

    // Deserialize byte array to packet, returns nullopt if invalid
    static std::optional<Packet> deserialize(const std::vector<uint8_t>& data);

    // Calculate outer checksum (sum of all bytes excluding checksum and tail, take low 8 bits)
    static uint8_t calculate_checksum(const std::vector<uint8_t>& data);

    // Calculate inner motor checksum (excluding motor header and checksum)
    static uint8_t calculate_motor_checksum(const std::vector<uint8_t>& inner_data);

    // Validate outer packet checksum
    bool validate_checksum() const;

    // Check if body contains a motor inner packet and validate its checksum
    bool validate_motor_checksum() const;
};

/**
 * Motor status data (from 0x20 READ_STATUS response)
 */
struct MotorStatus {
    uint8_t motor_id = 0;
    uint16_t target_position = 0;   // Target position
    uint16_t actual_position = 0;   // Actual position [0, 2000]
    uint16_t actual_current = 0;    // Actual current
    uint16_t force_value = 0;       // Force sensor value [0, 4095]
    uint16_t force_raw = 0;         // Force sensor raw value
    uint8_t temperature = 0;        // Temperature
    uint8_t fault_code = 0;         // Fault code
    std::chrono::steady_clock::time_point timestamp;
};

/**
 * Encoder data (16 channels)
 */
struct EncoderData {
    std::array<uint16_t, NUM_ENCODER_CHANNELS> adc_values;  // ADC raw values
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
    static Packet encode_read_motor_id(uint8_t motor_id);
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
    static uint16_t read_uint16_le(const std::vector<uint8_t>& data, size_t offset);
    static uint16_t read_uint16_be(const std::vector<uint8_t>& data, size_t offset);
    static float read_float_be(const std::vector<uint8_t>& data, size_t offset);
};

} // namespace joyson_glove
