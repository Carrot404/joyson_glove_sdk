#include "joyson_glove/protocol.hpp"
#include <cstring>
#include <algorithm>

namespace joyson_glove {

// Packet serialization
std::vector<uint8_t> Packet::serialize() const {
    std::vector<uint8_t> data;
    data.reserve(8 + body.size());

    // Header
    data.push_back(header);

    // Length (1 byte)
    data.push_back(length);

    // Module ID, Target, Command
    data.push_back(module_id);
    data.push_back(target);
    data.push_back(command);

    // Body
    data.insert(data.end(), body.begin(), body.end());

    // Checksum (1 byte)
    data.push_back(checksum);

    // Tail
    data.push_back(tail);

    return data;
}

// Packet deserialization
std::optional<Packet> Packet::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 7) {  // Minimum packet size: header(1) + length(1) + module(1) + target(1) + cmd(1) + checksum(1) + tail(1)
        return std::nullopt;
    }

    Packet packet;
    size_t idx = 0;

    // Header
    packet.header = data[idx++];
    if (packet.header != PACKET_HEADER) {
        return std::nullopt;
    }

    // Length (1 byte)
    packet.length = data[idx++];

    // FIXME(firmware): IMU and Encoder responses have known length field bugs (3 bytes too large).
    // Remove these workarounds once firmware fixes the length field.
    //
    // IMU response (raw bytes: EE 11 01 02):
    //   Bug 1 - Length field returns 0x11, correct is 0x0E
    //   Bug 2 - Target field is incorrect (returns 0x02, should be 0x03)
    //   Bug 3 - Checksum is always 0x3A (not calculated correctly)
    //
    // Encoder response (raw bytes: EE 25 01 02):
    //   Bug 1 - Length field returns 0x25, correct is 0x22
    const bool is_imu_response = (data.size() >= 4 &&
                                  data[0] == PACKET_HEADER && data[1] == 0x11 &&
                                  data[2] == 0x01 && data[3] == 0x02);
    const bool is_encoder_response = (data.size() >= 4 &&
                                      data[0] == PACKET_HEADER && data[1] == 0x25 &&
                                      data[2] == 0x01 && data[3] == 0x02);
    const bool skip_length_check = is_imu_response || is_encoder_response;

    // Validate total length: should be data.size() - 5 (excluding header, length, module_id, checksum, tail)
    size_t expected_total_length = packet.length + 5;
    if (!skip_length_check && expected_total_length != data.size()) {
        return std::nullopt;
    }

    // Module ID, Target, Command
    packet.module_id = data[idx++];
    packet.target = data[idx++];
    packet.command = data[idx++];

    // Body (everything between command and checksum)
    size_t body_size = data.size() - 7;  // Total - (header + length + module + target + cmd + checksum + tail)
    packet.body.resize(body_size);
    std::copy(data.begin() + idx, data.begin() + idx + body_size, packet.body.begin());
    idx += body_size;

    // Checksum (1 byte)
    packet.checksum = data[idx++];

    // Tail
    packet.tail = data[idx++];
    if (packet.tail != PACKET_TAIL) {
        return std::nullopt;
    }

    // Skip outer checksum for IMU response (reuse is_imu_response detected above)
    if (!is_imu_response && !packet.validate_checksum()) {
        return std::nullopt;
    }

    // Validate inner motor checksum if body contains a motor packet
    if (!packet.validate_motor_checksum()) {
        return std::nullopt;
    }

    return packet;
}

// Calculate outer checksum
uint8_t Packet::calculate_checksum(const std::vector<uint8_t>& data) {
    uint32_t sum = 0;
    // Sum all bytes except checksum and tail (last 2 bytes)
    for (size_t i = 0; i < data.size() - 2; ++i) {
        sum += data[i];
    }
    return static_cast<uint8_t>(sum & 0xFF);
}

// Calculate inner motor checksum (excluding motor header 0x55/0xAA and checksum)
uint8_t Packet::calculate_motor_checksum(const std::vector<uint8_t>& inner_data) {
    uint32_t sum = 0;
    // Sum from offset 2 (skip header) to end-1 (skip checksum)
    for (size_t i = 2; i < inner_data.size() - 1; ++i) {
        sum += inner_data[i];
    }
    return static_cast<uint8_t>(sum & 0xFF);
}

// Validate checksum
bool Packet::validate_checksum() const {
    auto serialized = serialize();
    uint8_t calculated = calculate_checksum(serialized);
    return calculated == checksum;
}

// Check if body contains a motor inner packet and validate its checksum
bool Packet::validate_motor_checksum() const {
    // Motor inner packet requires at least: header(2) + len(1) + checksum(1) = 4 bytes
    if (body.size() < 4) {
        return true;  // Not a motor packet, skip validation
    }

    bool is_motor_request = (body[0] == MOTOR_HEADER_REQ_1 && body[1] == MOTOR_HEADER_REQ_2);
    bool is_motor_response = (body[0] == MOTOR_HEADER_RESP_1 && body[1] == MOTOR_HEADER_RESP_2);

    if (!is_motor_request && !is_motor_response) {
        return true;  // Not a motor packet, skip validation
    }

    uint8_t expected = calculate_motor_checksum(body);
    uint8_t actual = body.back();
    return expected == actual;
}

// Helper: write uint16 little-endian
void ProtocolCodec::write_uint16_le(std::vector<uint8_t>& data, uint16_t value) {
    data.push_back(static_cast<uint8_t>(value & 0xFF));
    data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

// Helper: read uint16 little-endian
uint16_t ProtocolCodec::read_uint16_le(const std::vector<uint8_t>& data, size_t offset) {
    return data[offset] | (data[offset + 1] << 8);
}

// Helper: read uint16 big-endian
uint16_t ProtocolCodec::read_uint16_be(const std::vector<uint8_t>& data, size_t offset) {
    return (data[offset] << 8) | data[offset + 1];
}

// Helper: read float big-endian (IMU firmware sends floats in big-endian)
float ProtocolCodec::read_float_be(const std::vector<uint8_t>& data, size_t offset) {
    uint32_t bits = (data[offset] << 24) | (data[offset + 1] << 16) |
                    (data[offset + 2] << 8) | data[offset + 3];
    float value;
    std::memcpy(&value, &bits, sizeof(float));
    return value;
}

// Encode: Read motor ID (0x10) - 16 bytes
Packet ProtocolCodec::encode_read_motor_id(uint8_t motor_id) {
    Packet packet;
    packet.module_id = MODULE_ID;
    packet.target = TARGET_MOTOR;
    packet.command = CMD_READ_MOTOR_ID;

    // Inner motor packet: [0x55 0xAA] [Len] [MotorID] [InstrType] [RegAddrLo] [RegAddrHi] [RegCount] [Checksum]
    std::vector<uint8_t> inner;
    inner.push_back(MOTOR_HEADER_REQ_1);  // 0x55
    inner.push_back(MOTOR_HEADER_REQ_2);  // 0xAA
    inner.push_back(0x04);                // Len = 4
    inner.push_back(motor_id);            // Motor ID
    inner.push_back(MOTOR_INSTR_READ_REG); // 0x31
    inner.push_back(REG_MOTOR_ID);        // 0x16
    inner.push_back(0x00);                // Reg addr high
    inner.push_back(0x01);                // Reg count
    inner.push_back(0x00);                // Placeholder for checksum

    // Calculate inner checksum
    inner.back() = Packet::calculate_motor_checksum(inner);

    packet.body = inner;

    // Calculate outer length: total_length - 5
    packet.length = 2 + static_cast<uint8_t>(packet.body.size());  // target + cmd + body

    // Serialize and calculate outer checksum
    auto serialized = packet.serialize();
    packet.checksum = Packet::calculate_checksum(serialized);

    return packet;
}

// Encode: Read motor status (0x20) - 15 bytes
Packet ProtocolCodec::encode_read_motor_status(uint8_t motor_id) {
    Packet packet;
    packet.module_id = MODULE_ID;
    packet.target = TARGET_MOTOR;
    packet.command = CMD_READ_STATUS;

    // Inner motor packet: [0x55 0xAA] [Len] [MotorID] [InstrType] [RegAddrLo] [RegAddrHi] [Checksum]
    std::vector<uint8_t> inner;
    inner.push_back(MOTOR_HEADER_REQ_1);  // 0x55
    inner.push_back(MOTOR_HEADER_REQ_2);  // 0xAA
    inner.push_back(0x03);                // Len = 3
    inner.push_back(motor_id);            // Motor ID
    inner.push_back(MOTOR_INSTR_HOST_CMD); // 0x30
    inner.push_back(0x00);                // Reg addr low
    inner.push_back(0x00);                // Reg addr high
    inner.push_back(0x00);                // Placeholder for checksum

    // Calculate inner checksum
    inner.back() = Packet::calculate_motor_checksum(inner);

    packet.body = inner;

    // Calculate outer length
    packet.length = 2 + static_cast<uint8_t>(packet.body.size());

    // Serialize and calculate outer checksum
    auto serialized = packet.serialize();
    packet.checksum = Packet::calculate_checksum(serialized);

    return packet;
}

// Encode: Set motor mode (0x30) - 17 bytes
Packet ProtocolCodec::encode_set_motor_mode(uint8_t motor_id, uint8_t mode) {
    Packet packet;
    packet.module_id = MODULE_ID;
    packet.target = TARGET_MOTOR;
    packet.command = CMD_SET_MODE;

    // Inner motor packet: [0x55 0xAA] [Len] [MotorID] [CMD] [RegAddrLo] [RegAddrHi] [ModeLo] [ModeHi] [Checksum]
    std::vector<uint8_t> inner;
    inner.push_back(MOTOR_HEADER_REQ_1);  // 0x55
    inner.push_back(MOTOR_HEADER_REQ_2);  // 0xAA
    inner.push_back(0x05);                // Len = 5
    inner.push_back(motor_id);            // Motor ID
    inner.push_back(MOTOR_INSTR_READ_WRITE); // 0x32
    inner.push_back(REG_MODE);            // 0x25
    inner.push_back(0x00);                // Reg addr high
    write_uint16_le(inner, static_cast<uint16_t>(mode));
    inner.push_back(0x00);                // Placeholder for checksum

    // Calculate inner checksum
    inner.back() = Packet::calculate_motor_checksum(inner);

    packet.body = inner;

    // Calculate outer length
    packet.length = 2 + static_cast<uint8_t>(packet.body.size());

    // Serialize and calculate outer checksum
    auto serialized = packet.serialize();
    packet.checksum = Packet::calculate_checksum(serialized);

    return packet;
}

// Encode: Set motor position (0x31) - 17 bytes
Packet ProtocolCodec::encode_set_motor_position(uint8_t motor_id, uint16_t position) {
    Packet packet;
    packet.module_id = MODULE_ID;
    packet.target = TARGET_MOTOR;
    packet.command = CMD_SET_POSITION;

    // Inner motor packet
    std::vector<uint8_t> inner;
    inner.push_back(MOTOR_HEADER_REQ_1);  // 0x55
    inner.push_back(MOTOR_HEADER_REQ_2);  // 0xAA
    inner.push_back(0x05);                // Len = 5
    inner.push_back(motor_id);            // Motor ID
    inner.push_back(MOTOR_INSTR_READ_WRITE); // 0x32
    inner.push_back(REG_POSITION);        // 0x29
    inner.push_back(0x00);                // Reg addr high
    write_uint16_le(inner, position);
    inner.push_back(0x00);                // Placeholder for checksum

    // Calculate inner checksum
    inner.back() = Packet::calculate_motor_checksum(inner);

    packet.body = inner;

    // Calculate outer length
    packet.length = 2 + static_cast<uint8_t>(packet.body.size());

    // Serialize and calculate outer checksum
    auto serialized = packet.serialize();
    packet.checksum = Packet::calculate_checksum(serialized);

    return packet;
}

// Encode: Set motor force (0x33) - 17 bytes
Packet ProtocolCodec::encode_set_motor_force(uint8_t motor_id, uint16_t force) {
    Packet packet;
    packet.module_id = MODULE_ID;
    packet.target = TARGET_MOTOR;
    packet.command = CMD_SET_FORCE;

    // Inner motor packet
    std::vector<uint8_t> inner;
    inner.push_back(MOTOR_HEADER_REQ_1);  // 0x55
    inner.push_back(MOTOR_HEADER_REQ_2);  // 0xAA
    inner.push_back(0x05);                // Len = 5
    inner.push_back(motor_id);            // Motor ID
    inner.push_back(MOTOR_INSTR_READ_WRITE); // 0x32
    inner.push_back(REG_FORCE);           // 0x27
    inner.push_back(0x00);                // Reg addr high
    write_uint16_le(inner, force);
    inner.push_back(0x00);                // Placeholder for checksum

    // Calculate inner checksum
    inner.back() = Packet::calculate_motor_checksum(inner);

    packet.body = inner;

    // Calculate outer length
    packet.length = 2 + static_cast<uint8_t>(packet.body.size());

    // Serialize and calculate outer checksum
    auto serialized = packet.serialize();
    packet.checksum = Packet::calculate_checksum(serialized);

    return packet;
}

// Encode: Set motor speed (0x32) - 19 bytes
Packet ProtocolCodec::encode_set_motor_speed(uint8_t motor_id, uint16_t speed, uint16_t target_position) {
    Packet packet;
    packet.module_id = MODULE_ID;
    packet.target = TARGET_MOTOR;
    packet.command = CMD_SET_SPEED;

    // Inner motor packet (longer body with speed + position)
    std::vector<uint8_t> inner;
    inner.push_back(MOTOR_HEADER_REQ_1);  // 0x55
    inner.push_back(MOTOR_HEADER_REQ_2);  // 0xAA
    inner.push_back(0x07);                // Len = 7
    inner.push_back(motor_id);            // Motor ID
    inner.push_back(MOTOR_INSTR_READ_WRITE); // 0x32
    inner.push_back(REG_SPEED);           // 0x28
    inner.push_back(0x00);                // Reg addr high
    write_uint16_le(inner, speed);
    write_uint16_le(inner, target_position);
    inner.push_back(0x00);                // Placeholder for checksum

    // Calculate inner checksum
    inner.back() = Packet::calculate_motor_checksum(inner);

    packet.body = inner;

    // Calculate outer length
    packet.length = 2 + static_cast<uint8_t>(packet.body.size());

    // Serialize and calculate outer checksum
    auto serialized = packet.serialize();
    packet.checksum = Packet::calculate_checksum(serialized);

    return packet;
}

// Encode: Read all encoders (9 bytes)
Packet ProtocolCodec::encode_read_all_encoders() {
    Packet packet;
    packet.module_id = MODULE_ID;
    packet.target = TARGET_ENCODER;
    packet.command = 0x00;  // Read command

    // Body: ADC_Index(1) + ADC_Channel(1)
    packet.body.push_back(0xFF);  // ADC index = 0xFF (all)
    packet.body.push_back(0xFF);  // ADC channel = 0xFF (all)

    // Calculate outer length
    packet.length = 2 + static_cast<uint8_t>(packet.body.size());

    // Serialize and calculate outer checksum
    auto serialized = packet.serialize();
    packet.checksum = Packet::calculate_checksum(serialized);

    return packet;
}

// Encode: Read IMU (7 bytes)
Packet ProtocolCodec::encode_read_imu() {
    Packet packet;
    packet.module_id = MODULE_ID;
    packet.target = TARGET_IMU;
    packet.command = 0x00;  // Read command
    packet.body.clear();

    // Calculate outer length
    packet.length = 2 + static_cast<uint8_t>(packet.body.size());

    // Serialize and calculate outer checksum
    auto serialized = packet.serialize();
    packet.checksum = Packet::calculate_checksum(serialized);

    return packet;
}

// Parse: Motor status (25 bytes response)
std::optional<MotorStatus> ProtocolCodec::parse_motor_status(const Packet& packet) {
    if (packet.module_id != MODULE_ID || packet.target != TARGET_MOTOR) {
        return std::nullopt;
    }

    // Body contains inner motor response: [0xAA 0x55] [Len] [MotorID] [InstrType] [Reserved(2)] [Data...] [Checksum]
    // Expected inner body size: header(2) + len(1) + motor_id(1) + instr(1) + reserved(2) + data(10) + checksum(1) = 18 bytes
    if (packet.body.size() < 18) {
        return std::nullopt;
    }

    // Verify inner motor response header
    if (packet.body[0] != MOTOR_HEADER_RESP_1 || packet.body[1] != MOTOR_HEADER_RESP_2) {
        return std::nullopt;
    }

    MotorStatus status;
    status.motor_id = packet.body[3];  // Motor ID at offset 3

    // Data starts at offset 7 (after header(2) + len(1) + motor_id(1) + instr(1) + reserved(2))
    size_t data_offset = 7;
    status.target_position = read_uint16_le(packet.body, data_offset);
    status.actual_position = read_uint16_le(packet.body, data_offset + 2);
    status.actual_current = read_uint16_le(packet.body, data_offset + 4);
    status.force_value = read_uint16_le(packet.body, data_offset + 6);
    status.force_raw = read_uint16_le(packet.body, data_offset + 8);
    status.temperature = packet.body[data_offset + 10];
    status.fault_code = packet.body[data_offset + 11];
    status.timestamp = std::chrono::steady_clock::now();

    return status;
}

// Parse: Encoder data (39 bytes response)
std::optional<EncoderData> ProtocolCodec::parse_encoder_data(const Packet& packet) {
    if (packet.module_id != MODULE_ID || packet.target != TARGET_ENCODER) {
        return std::nullopt;
    }

    // Expected body size: 16 channels * 2 bytes = 32 bytes
    if (packet.body.size() < 32) {
        return std::nullopt;
    }

    EncoderData data;
    for (size_t i = 0; i < NUM_ENCODER_CHANNELS; ++i) {
        data.adc_values[i] = read_uint16_be(packet.body, i * 2);
    }
    data.timestamp = std::chrono::steady_clock::now();

    return data;
}

// Parse: IMU data (19 bytes response)
std::optional<ImuData> ProtocolCodec::parse_imu_data(const Packet& packet) {
    // Note: Firmware bug - response Target field may be incorrect, so we check module_id only
    if (packet.module_id != MODULE_ID) {
        return std::nullopt;
    }

    // Expected body size: 3 floats * 4 bytes = 12 bytes
    if (packet.body.size() < 12) {
        return std::nullopt;
    }

    ImuData data;
    data.roll = read_float_be(packet.body, 0);
    data.pitch = read_float_be(packet.body, 4);
    data.yaw = read_float_be(packet.body, 8);
    data.timestamp = std::chrono::steady_clock::now();

    return data;
}

} // namespace joyson_glove
