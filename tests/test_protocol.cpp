#include "joyson_glove/protocol.hpp"
#include <gtest/gtest.h>
#include <cstring>

using namespace joyson_glove;

// Test Packet serialization and deserialization
TEST(ProtocolTest, PacketSerializeDeserialize) {
    Packet packet;
    packet.header = PACKET_HEADER;
    packet.module_id = MODULE_MOTOR;
    packet.target = 1;
    packet.command = CMD_READ_STATUS;
    packet.body.clear();
    packet.tail = PACKET_TAIL;

    // Set length first (must be set before calculating checksum)
    packet.length = 9;  // Minimum packet size without body

    // Calculate checksum with correct length
    auto serialized = packet.serialize();
    packet.checksum = Packet::calculate_checksum(serialized);

    // Serialize with correct length and checksum
    serialized = packet.serialize();
    EXPECT_EQ(serialized.size(), 9);  // Minimum packet size without body
    EXPECT_EQ(serialized[0], PACKET_HEADER);
    EXPECT_EQ(serialized[serialized.size() - 1], PACKET_TAIL);

    // Deserialize
    auto deserialized = Packet::deserialize(serialized);
    ASSERT_TRUE(deserialized.has_value());
    EXPECT_EQ(deserialized->header, packet.header);
    EXPECT_EQ(deserialized->length, packet.length);
    EXPECT_EQ(deserialized->module_id, packet.module_id);
    EXPECT_EQ(deserialized->target, packet.target);
    EXPECT_EQ(deserialized->command, packet.command);
    EXPECT_EQ(deserialized->checksum, packet.checksum);
    EXPECT_EQ(deserialized->tail, packet.tail);
}

// Test checksum calculation
TEST(ProtocolTest, ChecksumCalculation) {
    std::vector<uint8_t> data = {0xEE, 0x0F, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x7E};
    uint16_t checksum = Packet::calculate_checksum(data);

    // Checksum should be sum of all bytes except last 3 (checksum + tail)
    uint32_t expected_sum = 0;
    for (size_t i = 0; i < data.size() - 3; ++i) {
        expected_sum += data[i];
    }
    EXPECT_EQ(checksum, static_cast<uint16_t>(expected_sum & 0xFFFF));
}

// Test invalid packet deserialization
TEST(ProtocolTest, InvalidPacketDeserialization) {
    // Too short
    std::vector<uint8_t> short_data = {0xEE, 0x05, 0x00};
    EXPECT_FALSE(Packet::deserialize(short_data).has_value());

    // Wrong header
    std::vector<uint8_t> wrong_header = {0xFF, 0x0F, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x7E};
    EXPECT_FALSE(Packet::deserialize(wrong_header).has_value());

    // Wrong tail
    std::vector<uint8_t> wrong_tail = {0xEE, 0x09, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0xFF};
    EXPECT_FALSE(Packet::deserialize(wrong_tail).has_value());
}

// Test encode_read_motor_status
TEST(ProtocolTest, EncodeReadMotorStatus) {
    uint8_t motor_id = 1;
    auto packet = ProtocolCodec::encode_read_motor_status(motor_id);

    EXPECT_EQ(packet.module_id, MODULE_MOTOR);
    EXPECT_EQ(packet.target, motor_id);
    EXPECT_EQ(packet.command, CMD_READ_STATUS);
    EXPECT_TRUE(packet.body.empty());

    auto serialized = packet.serialize();
    EXPECT_EQ(serialized.size(), 9);  // Minimum packet size
}

// Test encode_set_motor_mode
TEST(ProtocolTest, EncodeSetMotorMode) {
    uint8_t motor_id = 2;
    uint8_t mode = MODE_SERVO;
    auto packet = ProtocolCodec::encode_set_motor_mode(motor_id, mode);

    EXPECT_EQ(packet.module_id, MODULE_MOTOR);
    EXPECT_EQ(packet.target, motor_id);
    EXPECT_EQ(packet.command, CMD_SET_MODE);
    EXPECT_EQ(packet.body.size(), 3);  // register(1) + value(2)
    EXPECT_EQ(packet.body[0], REG_MODE);

    auto serialized = packet.serialize();
    EXPECT_EQ(serialized.size(), 12);  // 9 (base) + 3 (body)
}

// Test encode_set_motor_position
TEST(ProtocolTest, EncodeSetMotorPosition) {
    uint8_t motor_id = 3;
    uint16_t position = 1500;
    auto packet = ProtocolCodec::encode_set_motor_position(motor_id, position);

    EXPECT_EQ(packet.module_id, MODULE_MOTOR);
    EXPECT_EQ(packet.target, motor_id);
    EXPECT_EQ(packet.command, CMD_SET_POSITION);
    EXPECT_EQ(packet.body.size(), 3);  // register(1) + value(2)
    EXPECT_EQ(packet.body[0], REG_POSITION);

    // Check little-endian encoding
    uint16_t decoded_position = packet.body[1] | (packet.body[2] << 8);
    EXPECT_EQ(decoded_position, position);
}

// Test encode_set_motor_force
TEST(ProtocolTest, EncodeSetMotorForce) {
    uint8_t motor_id = 4;
    uint16_t force = 2048;
    auto packet = ProtocolCodec::encode_set_motor_force(motor_id, force);

    EXPECT_EQ(packet.module_id, MODULE_MOTOR);
    EXPECT_EQ(packet.target, motor_id);
    EXPECT_EQ(packet.command, CMD_SET_FORCE);
    EXPECT_EQ(packet.body.size(), 3);
    EXPECT_EQ(packet.body[0], REG_FORCE);

    uint16_t decoded_force = packet.body[1] | (packet.body[2] << 8);
    EXPECT_EQ(decoded_force, force);
}

// Test encode_set_motor_speed
TEST(ProtocolTest, EncodeSetMotorSpeed) {
    uint8_t motor_id = 5;
    uint16_t speed = 1000;
    uint16_t target_position = 1800;
    auto packet = ProtocolCodec::encode_set_motor_speed(motor_id, speed, target_position);

    EXPECT_EQ(packet.module_id, MODULE_MOTOR);
    EXPECT_EQ(packet.target, motor_id);
    EXPECT_EQ(packet.command, CMD_SET_SPEED);
    EXPECT_EQ(packet.body.size(), 6);  // speed_reg(1) + speed(2) + pos_reg(1) + pos(2)
    EXPECT_EQ(packet.body[0], REG_SPEED);
    EXPECT_EQ(packet.body[3], REG_POSITION);

    uint16_t decoded_speed = packet.body[1] | (packet.body[2] << 8);
    uint16_t decoded_position = packet.body[4] | (packet.body[5] << 8);
    EXPECT_EQ(decoded_speed, speed);
    EXPECT_EQ(decoded_position, target_position);
}

// Test encode_read_all_encoders
TEST(ProtocolTest, EncodeReadAllEncoders) {
    auto packet = ProtocolCodec::encode_read_all_encoders();

    EXPECT_EQ(packet.module_id, MODULE_ENCODER);
    EXPECT_EQ(packet.target, 0x00);
    EXPECT_EQ(packet.command, 0x00);
    EXPECT_TRUE(packet.body.empty());
}

// Test encode_read_imu
TEST(ProtocolTest, EncodeReadImu) {
    auto packet = ProtocolCodec::encode_read_imu();

    EXPECT_EQ(packet.module_id, MODULE_IMU);
    EXPECT_EQ(packet.target, 0x00);
    EXPECT_EQ(packet.command, 0x00);
    EXPECT_TRUE(packet.body.empty());
}

// Test parse_motor_status
TEST(ProtocolTest, ParseMotorStatus) {
    Packet packet;
    packet.module_id = MODULE_MOTOR;
    packet.target = 1;
    packet.command = CMD_READ_STATUS;

    // Create mock motor status body: position(2) + velocity(2) + force(2) + mode(1) + state(1)
    packet.body.resize(8);
    uint16_t position = 1234;
    int16_t velocity = -567;
    uint16_t force = 890;
    uint8_t mode = MODE_POSITION;
    uint8_t state = 0x01;

    packet.body[0] = position & 0xFF;
    packet.body[1] = (position >> 8) & 0xFF;
    packet.body[2] = velocity & 0xFF;
    packet.body[3] = (velocity >> 8) & 0xFF;
    packet.body[4] = force & 0xFF;
    packet.body[5] = (force >> 8) & 0xFF;
    packet.body[6] = mode;
    packet.body[7] = state;

    auto status = ProtocolCodec::parse_motor_status(packet);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(status->motor_id, 1);
    EXPECT_EQ(status->position, position);
    EXPECT_EQ(status->velocity, velocity);
    EXPECT_EQ(status->force, force);
    EXPECT_EQ(status->mode, mode);
    EXPECT_EQ(status->state, state);
}

// Test parse_encoder_data
TEST(ProtocolTest, ParseEncoderData) {
    Packet packet;
    packet.module_id = MODULE_ENCODER;
    packet.target = 0x00;
    packet.command = 0x00;

    // Create mock encoder data: 16 floats (64 bytes)
    packet.body.resize(64);
    for (size_t i = 0; i < NUM_ENCODER_CHANNELS; ++i) {
        float voltage = static_cast<float>(i) * 0.25f;  // 0.0, 0.25, 0.5, ...
        uint32_t bits;
        std::memcpy(&bits, &voltage, sizeof(float));
        size_t offset = i * 4;
        packet.body[offset + 0] = bits & 0xFF;
        packet.body[offset + 1] = (bits >> 8) & 0xFF;
        packet.body[offset + 2] = (bits >> 16) & 0xFF;
        packet.body[offset + 3] = (bits >> 24) & 0xFF;
    }

    auto data = ProtocolCodec::parse_encoder_data(packet);
    ASSERT_TRUE(data.has_value());
    for (size_t i = 0; i < NUM_ENCODER_CHANNELS; ++i) {
        EXPECT_FLOAT_EQ(data->voltages[i], static_cast<float>(i) * 0.25f);
    }
}

// Test parse_imu_data
TEST(ProtocolTest, ParseImuData) {
    Packet packet;
    packet.module_id = MODULE_IMU;
    packet.target = 0x00;
    packet.command = 0x00;

    // Create mock IMU data: 3 floats (12 bytes)
    packet.body.resize(12);
    float roll = 10.5f;
    float pitch = -20.3f;
    float yaw = 180.0f;

    auto write_float = [&](size_t offset, float value) {
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(float));
        packet.body[offset + 0] = bits & 0xFF;
        packet.body[offset + 1] = (bits >> 8) & 0xFF;
        packet.body[offset + 2] = (bits >> 16) & 0xFF;
        packet.body[offset + 3] = (bits >> 24) & 0xFF;
    };

    write_float(0, roll);
    write_float(4, pitch);
    write_float(8, yaw);

    auto data = ProtocolCodec::parse_imu_data(packet);
    ASSERT_TRUE(data.has_value());
    EXPECT_FLOAT_EQ(data->roll, roll);
    EXPECT_FLOAT_EQ(data->pitch, pitch);
    EXPECT_FLOAT_EQ(data->yaw, yaw);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
