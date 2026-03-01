#include "joyson_glove/protocol.hpp"
#include <gtest/gtest.h>
#include <cstring>
#include <iostream>
#include <cstdio>

using namespace joyson_glove;

// Test Packet serialization and deserialization
TEST(ProtocolTest, PacketSerializeDeserialize) {
    Packet packet;
    packet.header = PACKET_HEADER;
    packet.module_id = MODULE_ID;
    packet.target = TARGET_IMU;
    packet.command = 0x00;
    packet.body.clear();
    packet.tail = PACKET_TAIL;

    // Set length: target(1) + command(1) + body(0) = 2
    packet.length = 2;

    // Calculate checksum
    auto serialized = packet.serialize();
    packet.checksum = Packet::calculate_checksum(serialized);

    // Serialize with correct checksum
    serialized = packet.serialize();
    
    // Output serialized bytes in hex format
    std::cout << "Serialized packet bytes: ";
    for (size_t i = 0; i < serialized.size(); ++i) {
        std::printf("0x%02X ", static_cast<unsigned char>(serialized[i]));
    }
    std::cout << std::endl;
    EXPECT_EQ(serialized.size(), 7);  // header(1) + length(1) + module(1) + target(1) + cmd(1) + checksum(1) + tail(1)
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
    std::vector<uint8_t> data = {0xEE, 0x02, 0x01, 0x03, 0x00, 0x00, 0x7E};
    uint8_t checksum = Packet::calculate_checksum(data);
    
    // Output calculated checksum
    std::printf("Calculated checksum: 0x%02X (%u)\n", checksum, checksum);

    // Checksum should be sum of all bytes except last 2 (checksum + tail)
    uint32_t expected_sum = 0;
    for (size_t i = 0; i < data.size() - 2; ++i) {
        expected_sum += data[i];
    }
    EXPECT_EQ(checksum, static_cast<uint8_t>(expected_sum & 0xFF));
}

// Test packet with motor inner protocol (serialize and deserialize)
TEST(ProtocolTest, PacketWithMotorProtocolSerializeDeserialize) {
    // Create a packet with motor inner protocol (read motor status command)
    uint8_t motor_id = 3;
    auto packet = ProtocolCodec::encode_read_motor_status(motor_id);
    
    // Output original packet info
    std::cout << "\n=== Motor Protocol Packet Test ===" << std::endl;
    std::cout << "Motor ID: " << static_cast<int>(motor_id) << std::endl;
    std::cout << "Body size: " << packet.body.size() << " bytes" << std::endl;
    
    // Serialize the packet
    auto serialized = packet.serialize();
    
    // Output serialized bytes
    std::cout << "Serialized bytes (" << serialized.size() << " total): ";
    for (size_t i = 0; i < serialized.size(); ++i) {
        std::printf("0x%02X ", static_cast<unsigned char>(serialized[i]));
    }
    std::cout << std::endl;
    
    // Verify serialized structure
    EXPECT_EQ(serialized.size(), 15);  // header(1) + length(1) + module(1) + target(1) + cmd(1) + body(8) + checksum(1) + tail(1)
    EXPECT_EQ(serialized[0], PACKET_HEADER);  // 0xEE
    EXPECT_EQ(serialized[1], 10);  // length: target(1) + cmd(1) + body(8)
    EXPECT_EQ(serialized[2], MODULE_ID);  // 0x01
    EXPECT_EQ(serialized[3], TARGET_MOTOR);  // 0x01
    EXPECT_EQ(serialized[4], CMD_READ_STATUS);  // 0x20
    EXPECT_EQ(serialized[serialized.size() - 1], PACKET_TAIL);  // 0x7E
    
    // Verify motor inner protocol structure (body[0..7])
    EXPECT_EQ(serialized[5], MOTOR_HEADER_REQ_1);  // 0x55
    EXPECT_EQ(serialized[6], MOTOR_HEADER_REQ_2);  // 0xAA
    EXPECT_EQ(serialized[7], 0x03);  // inner length
    EXPECT_EQ(serialized[8], motor_id);  // motor ID
    EXPECT_EQ(serialized[9], MOTOR_INSTR_HOST_CMD);  // 0x30
    EXPECT_EQ(serialized[10], 0x00);  // Reg addr low
    EXPECT_EQ(serialized[11], 0x00);  // Reg addr high

    // Verify motor checksum (Len + MotorID + Instr + RegLo + RegHi)
    uint8_t expected_motor_checksum = (0x03 + motor_id + 0x30 + 0x00 + 0x00) & 0xFF;
    EXPECT_EQ(serialized[12], expected_motor_checksum);  // Motor checksum
    std::printf("Motor Checksum: 0x%02X (expected: 0x%02X)\n",
                static_cast<unsigned char>(serialized[12]), expected_motor_checksum);

    // Output outer checksum
    uint8_t checksum = serialized[serialized.size() - 2];
    std::printf("Checksum: 0x%02X (%u)\n", checksum, checksum);
    
    // Deserialize the packet
    auto deserialized = Packet::deserialize(serialized);
    ASSERT_TRUE(deserialized.has_value());
    
    // Verify all fields match
    EXPECT_EQ(deserialized->header, packet.header);
    EXPECT_EQ(deserialized->length, packet.length);
    EXPECT_EQ(deserialized->module_id, packet.module_id);
    EXPECT_EQ(deserialized->target, packet.target);
    EXPECT_EQ(deserialized->command, packet.command);
    EXPECT_EQ(deserialized->body.size(), packet.body.size());
    EXPECT_EQ(deserialized->checksum, packet.checksum);
    EXPECT_EQ(deserialized->tail, packet.tail);
    
    // Verify body content (motor inner protocol)
    for (size_t i = 0; i < packet.body.size(); ++i) {
        EXPECT_EQ(deserialized->body[i], packet.body[i]) 
            << "Body mismatch at index " << i;
    }
    
    std::cout << "✓ Serialization and deserialization verified successfully!" << std::endl;
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

    EXPECT_EQ(packet.module_id, MODULE_ID);
    EXPECT_EQ(packet.target, TARGET_MOTOR);
    EXPECT_EQ(packet.command, CMD_READ_STATUS);
    EXPECT_FALSE(packet.body.empty());  // Contains inner motor protocol

    auto serialized = packet.serialize();
    
    // Output serialized packet bytes
    std::cout << "\n=== EncodeReadMotorStatus Test ===" << std::endl;
    std::cout << "Motor ID: " << static_cast<int>(motor_id) << std::endl;
    std::cout << "Serialized bytes (" << serialized.size() << " total): ";
    for (size_t i = 0; i < serialized.size(); ++i) {
        std::printf("0x%02X ", static_cast<unsigned char>(serialized[i]));
    }
    std::cout << std::endl;
    
    EXPECT_EQ(serialized.size(), 15);  // header(1) + length(1) + module(1) + target(1) + cmd(1) + body(8) + checksum(1) + tail(1)
}

// Test encode_set_motor_mode
TEST(ProtocolTest, EncodeSetMotorMode) {
    uint8_t motor_id = 2;
    uint8_t mode = MODE_SERVO;
    auto packet = ProtocolCodec::encode_set_motor_mode(motor_id, mode);

    EXPECT_EQ(packet.module_id, MODULE_ID);
    EXPECT_EQ(packet.target, TARGET_MOTOR);
    EXPECT_EQ(packet.command, CMD_SET_MODE);
    EXPECT_FALSE(packet.body.empty());  // Contains inner motor protocol

    auto serialized = packet.serialize();
    
    // Output serialized packet bytes
    std::cout << "\n=== EncodeSetMotorMode Test ===" << std::endl;
    std::cout << "Motor ID: " << static_cast<int>(motor_id) << ", Mode: " << static_cast<int>(mode) << std::endl;
    std::cout << "Serialized bytes (" << serialized.size() << " total): ";
    for (size_t i = 0; i < serialized.size(); ++i) {
        std::printf("0x%02X ", static_cast<unsigned char>(serialized[i]));
    }
    std::cout << std::endl;
    
    EXPECT_EQ(serialized.size(), 17);  // With inner motor protocol
}

// Test encode_set_motor_position
TEST(ProtocolTest, EncodeSetMotorPosition) {
    uint8_t motor_id = 3;
    uint16_t position = 1500;
    auto packet = ProtocolCodec::encode_set_motor_position(motor_id, position);

    EXPECT_EQ(packet.module_id, MODULE_ID);
    EXPECT_EQ(packet.target, TARGET_MOTOR);
    EXPECT_EQ(packet.command, CMD_SET_POSITION);
    EXPECT_FALSE(packet.body.empty());  // Contains inner motor protocol

    auto serialized = packet.serialize();
    
    // Output serialized packet bytes
    std::cout << "\n=== EncodeSetMotorPosition Test ===" << std::endl;
    std::cout << "Motor ID: " << static_cast<int>(motor_id) << ", Position: " << position << std::endl;
    std::cout << "Serialized bytes (" << serialized.size() << " total): ";
    for (size_t i = 0; i < serialized.size(); ++i) {
        std::printf("0x%02X ", static_cast<unsigned char>(serialized[i]));
    }
    std::cout << std::endl;
    
    EXPECT_EQ(serialized.size(), 17);  // With inner motor protocol
}

// Test encode_set_motor_force
TEST(ProtocolTest, EncodeSetMotorForce) {
    uint8_t motor_id = 4;
    uint16_t force = 2048;
    auto packet = ProtocolCodec::encode_set_motor_force(motor_id, force);

    EXPECT_EQ(packet.module_id, MODULE_ID);
    EXPECT_EQ(packet.target, TARGET_MOTOR);
    EXPECT_EQ(packet.command, CMD_SET_FORCE);
    EXPECT_FALSE(packet.body.empty());  // Contains inner motor protocol

    auto serialized = packet.serialize();
    
    // Output serialized packet bytes
    std::cout << "\n=== EncodeSetMotorForce Test ===" << std::endl;
    std::cout << "Motor ID: " << static_cast<int>(motor_id) << ", Force: " << force << std::endl;
    std::cout << "Serialized bytes (" << serialized.size() << " total): ";
    for (size_t i = 0; i < serialized.size(); ++i) {
        std::printf("0x%02X ", static_cast<unsigned char>(serialized[i]));
    }
    std::cout << std::endl;
    
    EXPECT_EQ(serialized.size(), 17);  // With inner motor protocol
}

// Test encode_set_motor_speed
TEST(ProtocolTest, EncodeSetMotorSpeed) {
    uint8_t motor_id = 5;
    uint16_t speed = 1000;
    uint16_t target_position = 1800;
    auto packet = ProtocolCodec::encode_set_motor_speed(motor_id, speed, target_position);

    EXPECT_EQ(packet.module_id, MODULE_ID);
    EXPECT_EQ(packet.target, TARGET_MOTOR);
    EXPECT_EQ(packet.command, CMD_SET_SPEED);
    EXPECT_FALSE(packet.body.empty());  // Contains inner motor protocol

    auto serialized = packet.serialize();
    
    // Output serialized packet bytes
    std::cout << "\n=== EncodeSetMotorSpeed Test ===" << std::endl;
    std::cout << "Motor ID: " << static_cast<int>(motor_id) << ", Speed: " << speed 
              << ", Target Position: " << target_position << std::endl;
    std::cout << "Serialized bytes (" << serialized.size() << " total): ";
    for (size_t i = 0; i < serialized.size(); ++i) {
        std::printf("0x%02X ", static_cast<unsigned char>(serialized[i]));
    }
    std::cout << std::endl;
    
    EXPECT_EQ(serialized.size(), 19);  // With inner motor protocol (longer body)
}

// Test encode_read_all_encoders
TEST(ProtocolTest, EncodeReadAllEncoders) {
    auto packet = ProtocolCodec::encode_read_all_encoders();

    EXPECT_EQ(packet.module_id, MODULE_ID);
    EXPECT_EQ(packet.target, TARGET_ENCODER);
    EXPECT_EQ(packet.command, 0x00);
    EXPECT_EQ(packet.body.size(), 2);  // ADC_Index + ADC_Channel
    
    auto serialized = packet.serialize();
    
    // Output serialized packet bytes
    std::cout << "\n=== EncodeReadAllEncoders Test ===" << std::endl;
    std::cout << "Serialized bytes (" << serialized.size() << " total): ";
    for (size_t i = 0; i < serialized.size(); ++i) {
        std::printf("0x%02X ", static_cast<unsigned char>(serialized[i]));
    }
    std::cout << std::endl;
}

// Test encode_read_imu
TEST(ProtocolTest, EncodeReadImu) {
    auto packet = ProtocolCodec::encode_read_imu();

    EXPECT_EQ(packet.module_id, MODULE_ID);
    EXPECT_EQ(packet.target, TARGET_IMU);
    EXPECT_EQ(packet.command, 0x00);
    EXPECT_TRUE(packet.body.empty());
    
    auto serialized = packet.serialize();
    
    // Output serialized packet bytes
    std::cout << "\n=== EncodeReadImu Test ===" << std::endl;
    std::cout << "Serialized bytes (" << serialized.size() << " total): ";
    for (size_t i = 0; i < serialized.size(); ++i) {
        std::printf("0x%02X ", static_cast<unsigned char>(serialized[i]));
    }
    std::cout << std::endl;
}

// Test parse_motor_status
TEST(ProtocolTest, ParseMotorStatus) {
    Packet packet;
    packet.module_id = MODULE_ID;
    packet.target = TARGET_MOTOR;
    packet.command = CMD_READ_STATUS;

    // Create mock motor status body with inner motor protocol
    // [0xAA 0x55] [Len] [MotorID] [InstrType] [Reserved(2)] [TargetPos(2)] [ActualPos(2)] [ActualCur(2)] [ForceVal(2)] [ForceRaw(2)] [Temp(1)] [Fault(1)] [Checksum]
    packet.body.resize(20);
    packet.body[0] = MOTOR_HEADER_RESP_1;  // 0xAA
    packet.body[1] = MOTOR_HEADER_RESP_2;  // 0x55
    packet.body[2] = 0x0F;  // Len
    packet.body[3] = 1;     // Motor ID
    packet.body[4] = MOTOR_INSTR_HOST_CMD;  // Instr type
    packet.body[5] = 0x00;  // Reserved
    packet.body[6] = 0x00;  // Reserved
    
    uint16_t target_pos = 1000;
    uint16_t actual_pos = 1234;
    uint16_t actual_cur = 567;
    uint16_t force_val = 890;
    uint16_t force_raw = 900;
    uint8_t temp = 45;
    uint8_t fault = 0x00;
    
    packet.body[7] = target_pos & 0xFF;
    packet.body[8] = (target_pos >> 8) & 0xFF;
    packet.body[9] = actual_pos & 0xFF;
    packet.body[10] = (actual_pos >> 8) & 0xFF;
    packet.body[11] = actual_cur & 0xFF;
    packet.body[12] = (actual_cur >> 8) & 0xFF;
    packet.body[13] = force_val & 0xFF;
    packet.body[14] = (force_val >> 8) & 0xFF;
    packet.body[15] = force_raw & 0xFF;
    packet.body[16] = (force_raw >> 8) & 0xFF;
    packet.body[17] = temp;
    packet.body[18] = fault;
    packet.body[19] = 0x00;  // Checksum placeholder

    // Output constructed packet bytes
    std::cout << "\n=== ParseMotorStatus Test ===" << std::endl;
    std::cout << "Constructed packet body (" << packet.body.size() << " bytes): ";
    for (size_t i = 0; i < packet.body.size(); ++i) {
        std::printf("0x%02X ", static_cast<unsigned char>(packet.body[i]));
    }
    std::cout << std::endl;
    
    auto status = ProtocolCodec::parse_motor_status(packet);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(status->motor_id, 1);
    EXPECT_EQ(status->target_position, target_pos);
    EXPECT_EQ(status->actual_position, actual_pos);
    EXPECT_EQ(status->actual_current, actual_cur);
    EXPECT_EQ(status->force_value, force_val);
    EXPECT_EQ(status->force_raw, force_raw);
    EXPECT_EQ(status->temperature, temp);
    EXPECT_EQ(status->fault_code, fault);
}

// Test parse_encoder_data
TEST(ProtocolTest, ParseEncoderData) {
    Packet packet;
    packet.module_id = MODULE_ID;
    packet.target = TARGET_ENCODER;
    packet.command = 0x00;

    // Create mock encoder data: 16 uint16_t ADC values (32 bytes)
    packet.body.resize(32);
    for (size_t i = 0; i < NUM_ENCODER_CHANNELS; ++i) {
        uint16_t adc_value = static_cast<uint16_t>(i * 256);  // 0, 256, 512, ...
        size_t offset = i * 2;
        packet.body[offset + 0] = (adc_value >> 8) & 0xFF;
        packet.body[offset + 1] = adc_value & 0xFF;
    }

    // Output constructed packet bytes
    std::cout << "\n=== ParseEncoderData Test ===" << std::endl;
    std::cout << "Constructed packet body (" << packet.body.size() << " bytes): ";
    for (size_t i = 0; i < packet.body.size(); ++i) {
        std::printf("0x%02X ", static_cast<unsigned char>(packet.body[i]));
    }
    std::cout << std::endl;

    auto data = ProtocolCodec::parse_encoder_data(packet);
    ASSERT_TRUE(data.has_value());
    for (size_t i = 0; i < NUM_ENCODER_CHANNELS; ++i) {
        EXPECT_EQ(data->adc_values[i], static_cast<uint16_t>(i * 256));
    }
}

// Test parse_imu_data
TEST(ProtocolTest, ParseImuData) {
    Packet packet;
    packet.module_id = MODULE_ID;
    packet.target = 0x00;
    packet.command = 0x00;

    // Create mock IMU data: 3 floats (12 bytes)
    packet.body.resize(12);
    float roll = 10.5f;
    float pitch = -20.3f;
    float yaw = 180.0f;

    // Write float in big-endian (matching IMU firmware byte order)
    auto write_float_be = [&](size_t offset, float value) {
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(float));
        packet.body[offset + 0] = (bits >> 24) & 0xFF;
        packet.body[offset + 1] = (bits >> 16) & 0xFF;
        packet.body[offset + 2] = (bits >> 8) & 0xFF;
        packet.body[offset + 3] = bits & 0xFF;
    };

    write_float_be(0, roll);
    write_float_be(4, pitch);
    write_float_be(8, yaw);

    // Output constructed packet bytes
    std::cout << "\n=== ParseImuData Test ===" << std::endl;
    std::cout << "Constructed packet body (" << packet.body.size() << " bytes): ";
    for (size_t i = 0; i < packet.body.size(); ++i) {
        std::printf("0x%02X ", static_cast<unsigned char>(packet.body[i]));
    }
    std::cout << std::endl;
    std::cout << "Expected values - Roll: " << roll << ", Pitch: " << pitch << ", Yaw: " << yaw << std::endl;

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
