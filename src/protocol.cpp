#include "joyson_glove/protocol.hpp"
#include <cstring>
#include <algorithm>

namespace joyson_glove {

// Packet serialization
std::vector<uint8_t> Packet::serialize() const {
    std::vector<uint8_t> data;
    data.reserve(length);

    // Header
    data.push_back(header);

    // Length (little-endian)
    data.push_back(static_cast<uint8_t>(length & 0xFF));
    data.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));

    // Module ID, Target, Command
    data.push_back(module_id);
    data.push_back(target);
    data.push_back(command);

    // Body
    data.insert(data.end(), body.begin(), body.end());

    // Checksum (little-endian)
    data.push_back(static_cast<uint8_t>(checksum & 0xFF));
    data.push_back(static_cast<uint8_t>((checksum >> 8) & 0xFF));

    // Tail
    data.push_back(tail);

    return data;
}

// Packet deserialization
std::optional<Packet> Packet::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 9) {  // Minimum packet size: header(1) + length(2) + module(1) + target(1) + cmd(1) + checksum(2) + tail(1)
        return std::nullopt;
    }

    Packet packet;
    size_t idx = 0;

    // Header
    packet.header = data[idx++];
    if (packet.header != PACKET_HEADER) {
        return std::nullopt;
    }

    // Length
    packet.length = data[idx] | (data[idx + 1] << 8);
    idx += 2;

    if (packet.length != data.size()) {
        return std::nullopt;
    }

    // Module ID, Target, Command
    packet.module_id = data[idx++];
    packet.target = data[idx++];
    packet.command = data[idx++];

    // Body (everything between command and checksum)
    size_t body_size = data.size() - 9;  // Total - (header + length + module + target + cmd + checksum + tail)
    packet.body.resize(body_size);
    std::copy(data.begin() + idx, data.begin() + idx + body_size, packet.body.begin());
    idx += body_size;

    // Checksum
    packet.checksum = data[idx] | (data[idx + 1] << 8);
    idx += 2;

    // Tail
    packet.tail = data[idx++];
    if (packet.tail != PACKET_TAIL) {
        return std::nullopt;
    }

    // Validate checksum
    if (!packet.validate_checksum()) {
        return std::nullopt;
    }

    return packet;
}

// Calculate checksum
uint16_t Packet::calculate_checksum(const std::vector<uint8_t>& data) {
    uint32_t sum = 0;
    // Sum all bytes except checksum and tail (last 3 bytes)
    for (size_t i = 0; i < data.size() - 3; ++i) {
        sum += data[i];
    }
    return static_cast<uint16_t>(sum & 0xFFFF);
}

// Validate checksum
bool Packet::validate_checksum() const {
    auto serialized = serialize();
    uint16_t calculated = calculate_checksum(serialized);
    return calculated == checksum;
}

// Helper: write uint16 little-endian
void ProtocolCodec::write_uint16_le(std::vector<uint8_t>& data, uint16_t value) {
    data.push_back(static_cast<uint8_t>(value & 0xFF));
    data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

// Helper: write int16 little-endian
void ProtocolCodec::write_int16_le(std::vector<uint8_t>& data, int16_t value) {
    write_uint16_le(data, static_cast<uint16_t>(value));
}

// Helper: write float little-endian
void ProtocolCodec::write_float_le(std::vector<uint8_t>& data, float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(float));
    data.push_back(static_cast<uint8_t>(bits & 0xFF));
    data.push_back(static_cast<uint8_t>((bits >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>((bits >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((bits >> 24) & 0xFF));
}

// Helper: read uint16 little-endian
uint16_t ProtocolCodec::read_uint16_le(const std::vector<uint8_t>& data, size_t offset) {
    return data[offset] | (data[offset + 1] << 8);
}

// Helper: read int16 little-endian
int16_t ProtocolCodec::read_int16_le(const std::vector<uint8_t>& data, size_t offset) {
    return static_cast<int16_t>(read_uint16_le(data, offset));
}

// Helper: read float little-endian
float ProtocolCodec::read_float_le(const std::vector<uint8_t>& data, size_t offset) {
    uint32_t bits = data[offset] | (data[offset + 1] << 8) |
                    (data[offset + 2] << 16) | (data[offset + 3] << 24);
    float value;
    std::memcpy(&value, &bits, sizeof(float));
    return value;
}

// Encode: Read motor status (15 bytes)
Packet ProtocolCodec::encode_read_motor_status(uint8_t motor_id) {
    Packet packet;
    packet.module_id = MODULE_MOTOR;
    packet.target = motor_id;
    packet.command = CMD_READ_STATUS;
    packet.body.clear();  // No body for read command

    // Calculate length and checksum
    auto serialized = packet.serialize();
    packet.length = static_cast<uint16_t>(serialized.size());
    packet.checksum = Packet::calculate_checksum(serialized);

    return packet;
}

// Encode: Set motor mode (17 bytes)
Packet ProtocolCodec::encode_set_motor_mode(uint8_t motor_id, uint8_t mode) {
    Packet packet;
    packet.module_id = MODULE_MOTOR;
    packet.target = motor_id;
    packet.command = CMD_SET_MODE;

    // Body: register(1) + value(2)
    packet.body.push_back(REG_MODE);
    write_uint16_le(packet.body, static_cast<uint16_t>(mode));

    // Calculate length and checksum
    auto serialized = packet.serialize();
    packet.length = static_cast<uint16_t>(serialized.size());
    packet.checksum = Packet::calculate_checksum(serialized);

    return packet;
}

// Encode: Set motor position (17 bytes)
Packet ProtocolCodec::encode_set_motor_position(uint8_t motor_id, uint16_t position) {
    Packet packet;
    packet.module_id = MODULE_MOTOR;
    packet.target = motor_id;
    packet.command = CMD_SET_POSITION;

    // Body: register(1) + value(2)
    packet.body.push_back(REG_POSITION);
    write_uint16_le(packet.body, position);

    // Calculate length and checksum
    auto serialized = packet.serialize();
    packet.length = static_cast<uint16_t>(serialized.size());
    packet.checksum = Packet::calculate_checksum(serialized);

    return packet;
}

// Encode: Set motor force (17 bytes)
Packet ProtocolCodec::encode_set_motor_force(uint8_t motor_id, uint16_t force) {
    Packet packet;
    packet.module_id = MODULE_MOTOR;
    packet.target = motor_id;
    packet.command = CMD_SET_FORCE;

    // Body: register(1) + value(2)
    packet.body.push_back(REG_FORCE);
    write_uint16_le(packet.body, force);

    // Calculate length and checksum
    auto serialized = packet.serialize();
    packet.length = static_cast<uint16_t>(serialized.size());
    packet.checksum = Packet::calculate_checksum(serialized);

    return packet;
}

// Encode: Set motor speed (19 bytes)
Packet ProtocolCodec::encode_set_motor_speed(uint8_t motor_id, uint16_t speed, uint16_t target_position) {
    Packet packet;
    packet.module_id = MODULE_MOTOR;
    packet.target = motor_id;
    packet.command = CMD_SET_SPEED;

    // Body: speed_register(1) + speed(2) + position_register(1) + position(2)
    packet.body.push_back(REG_SPEED);
    write_uint16_le(packet.body, speed);
    packet.body.push_back(REG_POSITION);
    write_uint16_le(packet.body, target_position);

    // Calculate length and checksum
    auto serialized = packet.serialize();
    packet.length = static_cast<uint16_t>(serialized.size());
    packet.checksum = Packet::calculate_checksum(serialized);

    return packet;
}

// Encode: Read all encoders (9 bytes)
Packet ProtocolCodec::encode_read_all_encoders() {
    Packet packet;
    packet.module_id = MODULE_ENCODER;
    packet.target = 0x00;  // Read all channels
    packet.command = 0x00;  // Read command
    packet.body.clear();

    // Calculate length and checksum
    auto serialized = packet.serialize();
    packet.length = static_cast<uint16_t>(serialized.size());
    packet.checksum = Packet::calculate_checksum(serialized);

    return packet;
}

// Encode: Read IMU (7 bytes)
Packet ProtocolCodec::encode_read_imu() {
    Packet packet;
    packet.module_id = MODULE_IMU;
    packet.target = 0x00;
    packet.command = 0x00;  // Read command
    packet.body.clear();

    // Calculate length and checksum
    auto serialized = packet.serialize();
    packet.length = static_cast<uint16_t>(serialized.size());
    packet.checksum = Packet::calculate_checksum(serialized);

    return packet;
}

// Parse: Motor status (25 bytes response)
std::optional<MotorStatus> ProtocolCodec::parse_motor_status(const Packet& packet) {
    if (packet.module_id != MODULE_MOTOR) {
        return std::nullopt;
    }

    // Expected body size: position(2) + velocity(2) + force(2) + mode(1) + state(1) = 8 bytes
    if (packet.body.size() < 8) {
        return std::nullopt;
    }

    MotorStatus status;
    status.motor_id = packet.target;
    status.position = read_uint16_le(packet.body, 0);
    status.velocity = read_int16_le(packet.body, 2);
    status.force = read_uint16_le(packet.body, 4);
    status.mode = packet.body[6];
    status.state = packet.body[7];
    status.timestamp = std::chrono::steady_clock::now();

    return status;
}

// Parse: Encoder data (39 bytes response)
std::optional<EncoderData> ProtocolCodec::parse_encoder_data(const Packet& packet) {
    if (packet.module_id != MODULE_ENCODER) {
        return std::nullopt;
    }

    // Expected body size: 16 floats * 4 bytes = 64 bytes
    if (packet.body.size() < 64) {
        return std::nullopt;
    }

    EncoderData data;
    for (size_t i = 0; i < NUM_ENCODER_CHANNELS; ++i) {
        data.voltages[i] = read_float_le(packet.body, i * 4);
    }
    data.timestamp = std::chrono::steady_clock::now();

    return data;
}

// Parse: IMU data (19 bytes response)
std::optional<ImuData> ProtocolCodec::parse_imu_data(const Packet& packet) {
    // Note: Firmware bug - response shows target 0x02 instead of 0x03, so we check module_id only
    if (packet.module_id != MODULE_IMU) {
        return std::nullopt;
    }

    // Expected body size: 3 floats * 4 bytes = 12 bytes
    if (packet.body.size() < 12) {
        return std::nullopt;
    }

    ImuData data;
    data.roll = read_float_le(packet.body, 0);
    data.pitch = read_float_le(packet.body, 4);
    data.yaw = read_float_le(packet.body, 8);
    data.timestamp = std::chrono::steady_clock::now();

    return data;
}

} // namespace joyson_glove
