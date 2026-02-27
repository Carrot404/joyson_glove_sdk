# Joyson Glove SDK

C++ SDK for controlling Joyson exoskeleton glove hardware with LAF actuators, encoders, and IMU sensors.

## Features

- **Motor Control**: Control 6 LAF actuators in multiple modes (position, force, speed, servo)
- **Sensor Reading**: Read 16-channel encoder data and 3-axis IMU orientation
- **Thread-Safe**: All operations are thread-safe with mutex protection
- **Asynchronous Updates**: Background threads poll sensors at configurable rates (default 100Hz)
- **Calibration Support**: Zero-point calibration for encoders and IMU
- **Non-Blocking API**: Cached data access for real-time performance
- **ROS2-Friendly**: Clean C++ interface easy to wrap in ROS2 nodes

## Hardware Specifications

- **Motors**: 6 LAF actuators (ID 1-6)
  - Modes: Position, Servo, Speed, Force
  - Position range: [0, 2000] steps
  - Force range: [0, 4095]
- **Encoders**: 16 channels (CH0-15)
  - ADC voltage range: 0-4V
  - Angle mapping: 0V = 0°, 4V = 360°
- **IMU**: 3-axis orientation (roll, pitch, yaw in degrees)
- **Communication**: UDP protocol via 192.168.10.123:8080

## Requirements

- C++17 compiler (GCC 7+, Clang 5+)
- CMake 3.16+
- Linux (tested on Ubuntu 20.04+)
- spdlog (required, for logging)
- Google Test (required when `BUILD_TESTS=ON`, default ON)

## Installation

### Install Dependencies

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential cmake libspdlog-dev libgtest-dev

# Arch Linux
sudo pacman -S base-devel cmake spdlog gtest
```

### Build from Source

```bash
# Clone repository
cd joyson_glove_sdk

# Create build directory
mkdir build && cd build

# Configure
cmake ..

# Build
make -j$(nproc)

# Install (optional)
sudo make install
```

### Build Options

```bash
# Disable examples
cmake -DBUILD_EXAMPLES=OFF ..

# Disable tests
cmake -DBUILD_TESTS=OFF ..

# Release build
cmake -DCMAKE_BUILD_TYPE=Release ..
```

## Quick Start

### Basic Motor Control

```cpp
#include "joyson_glove/glove_sdk.hpp"

using namespace joyson_glove;

int main() {
    // Create SDK instance
    GloveSDK sdk;

    // Initialize and connect to hardware
    if (!sdk.initialize()) {
        return 1;
    }

    // Set motor positions
    std::array<uint16_t, NUM_MOTORS> positions = {500, 600, 700, 800, 900, 1000};
    sdk.set_all_positions(positions);

    // Read motor status
    auto status = sdk.motor_controller().get_motor_status(1);
    if (status) {
        std::cout << "Motor 1 position: " << status->position << std::endl;
    }

    // Shutdown
    sdk.shutdown();
    return 0;
}
```

### Read Sensor Data

```cpp
#include "joyson_glove/glove_sdk.hpp"

using namespace joyson_glove;

int main() {
    // Create SDK with background threads enabled
    GloveConfig config;
    config.auto_start_threads = true;
    GloveSDK sdk(config);

    if (!sdk.initialize()) {
        return 1;
    }

    // Calibrate sensors
    sdk.calibrate_all();

    // Read cached data (non-blocking)
    auto encoder_angles = sdk.get_encoder_angles();
    auto imu_data = sdk.get_imu_orientation();

    std::cout << "Encoder CH0: " << encoder_angles[0] << "°" << std::endl;
    std::cout << "IMU Roll: " << imu_data.roll << "°" << std::endl;

    sdk.shutdown();
    return 0;
}
```

### Servo Mode (High-Frequency Control)

```cpp
#include "joyson_glove/glove_sdk.hpp"
#include <chrono>
#include <thread>

using namespace joyson_glove;

int main() {
    GloveSDK sdk;
    sdk.initialize();

    // Set to servo mode
    sdk.motor_controller().set_all_modes(MODE_SERVO);

    // 50Hz control loop (20ms update rate)
    for (int i = 0; i < 500; ++i) {
        auto start = std::chrono::steady_clock::now();

        // Calculate target positions
        std::array<uint16_t, NUM_MOTORS> targets;
        for (size_t j = 0; j < NUM_MOTORS; ++j) {
            targets[j] = 1000 + 500 * std::sin(i * 0.1);
        }

        // Send commands
        sdk.set_all_positions(targets);

        // Sleep to maintain 50Hz
        auto elapsed = std::chrono::steady_clock::now() - start;
        std::this_thread::sleep_for(std::chrono::milliseconds(20) - elapsed);
    }

    sdk.shutdown();
    return 0;
}
```

## API Reference

### GloveSDK

Main SDK interface providing unified API for all components.

#### Initialization

```cpp
GloveSDK(const GloveConfig& config = GloveConfig{});
bool initialize();
void shutdown();
bool is_initialized() const;
```

#### Motor Control

```cpp
bool set_motor_position(uint8_t motor_id, uint16_t position);
bool set_motor_force(uint8_t motor_id, uint16_t force);
bool set_motor_speed(uint8_t motor_id, uint16_t speed, uint16_t target_position);
bool set_all_positions(const std::array<uint16_t, NUM_MOTORS>& positions);
bool set_all_forces(const std::array<uint16_t, NUM_MOTORS>& forces);
```

#### Sensor Reading

```cpp
std::array<float, NUM_ENCODER_CHANNELS> get_encoder_angles();
ImuData get_imu_orientation();
```

#### Calibration

```cpp
bool calibrate_all();
bool calibrate_encoders();
bool calibrate_imu();
```

#### Component Access

```cpp
MotorController& motor_controller();
EncoderReader& encoder_reader();
ImuReader& imu_reader();
UdpClient& udp_client();
```

### Configuration

```cpp
struct GloveConfig {
    std::string server_ip = "192.168.10.123";
    uint16_t server_port = 8080;
    std::chrono::milliseconds send_timeout{100};
    std::chrono::milliseconds receive_timeout{100};

    bool auto_start_threads = true;
    std::chrono::milliseconds motor_update_interval{10};    // 100Hz
    std::chrono::milliseconds encoder_update_interval{10};  // 100Hz
    std::chrono::milliseconds imu_update_interval{10};      // 100Hz

    std::string encoder_calibration_file = "encoder_calibration.bin";
    std::string imu_calibration_file = "imu_calibration.bin";
};
```

## Examples

The SDK includes three example programs:

1. **basic_motor_control**: Demonstrates basic motor control operations
2. **read_sensors**: Shows how to read encoder and IMU data
3. **servo_mode_demo**: High-frequency servo mode control example

Build and run examples:

```bash
cd build
./examples/basic_motor_control
./examples/read_sensors
./examples/servo_mode_demo
```

## Threading Model

The SDK uses background threads for asynchronous sensor data updates:

- **Motor Status Thread**: Polls motor status at configurable rate (default 100Hz)
- **Encoder Update Thread**: Reads encoder data at configurable rate (default 100Hz)
- **IMU Update Thread**: Reads IMU data at configurable rate (default 100Hz)

All cached data is protected by mutexes for thread safety. User calls to `get_cached_*()` methods are non-blocking and return a copy of the cached data.

## Error Handling

The SDK uses a non-throwing API with `std::optional` and `bool` return values:

- Network timeout: Returns `std::nullopt` or `false`, logs warning
- Network error: Returns `std::nullopt` or `false`, logs error
- Invalid packet: Returns `std::nullopt`, logs error
- Checksum mismatch: Returns `std::nullopt`, increments statistics counter

All errors are logged with sufficient context for debugging. The SDK does NOT automatically reconnect on network failure - user application is responsible for calling `shutdown()` and `initialize()` to reconnect.

## Calibration

### Encoder Calibration

Encoder calibration sets the current position as zero point:

```cpp
sdk.calibrate_encoders();
```

Calibration data is automatically saved to `encoder_calibration.bin` and loaded on next initialization.

### IMU Calibration

IMU calibration sets the current orientation as zero:

```cpp
sdk.calibrate_imu();
```

Calibration data is automatically saved to `imu_calibration.bin` and loaded on next initialization.

## Network Statistics

Monitor network performance:

```cpp
auto stats = sdk.get_network_statistics();
std::cout << "Packets sent: " << stats.packets_sent << std::endl;
std::cout << "Packets received: " << stats.packets_received << std::endl;
std::cout << "Timeouts: " << stats.timeouts << std::endl;
std::cout << "Checksum errors: " << stats.checksum_errors << std::endl;
```

## ROS2 Integration

This SDK is a pure C++ library with no ROS2 dependencies. For ROS2 integration, create a separate ROS2 package that:

1. Links against `libjoyson_glove_sdk.so`
2. Creates ROS2 node wrapping `GloveSDK`
3. Publishes/subscribes to ROS2 topics
4. Provides launch files and RViz configs

Example ROS2 node structure:

```cpp
class GloveNode : public rclcpp::Node {
public:
    GloveNode() : Node("glove_node") {
        sdk_.initialize();

        // Create publishers
        encoder_pub_ = create_publisher<sensor_msgs::msg::JointState>("encoder_data", 10);
        imu_pub_ = create_publisher<sensor_msgs::msg::Imu>("imu_data", 10);

        // Create timer for publishing
        timer_ = create_wall_timer(10ms, [this]() { publish_data(); });
    }

private:
    GloveSDK sdk_;
    rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr encoder_pub_;
    rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr imu_pub_;
    rclcpp::TimerBase::SharedPtr timer_;

    void publish_data() {
        // Publish encoder data
        auto encoder_msg = sensor_msgs::msg::JointState();
        auto angles = sdk_.get_encoder_angles();
        encoder_msg.position.assign(angles.begin(), angles.end());
        encoder_pub_->publish(encoder_msg);

        // Publish IMU data
        auto imu_msg = sensor_msgs::msg::Imu();
        auto imu_data = sdk_.get_imu_orientation();
        // Convert to quaternion and publish...
        imu_pub_->publish(imu_msg);
    }
};
```

## Troubleshooting

### Connection Failed

- Check network connection: `ping 192.168.10.123`
- Verify hardware is powered on
- Check firewall settings: `sudo ufw allow 8080/udp`

### High Packet Loss

- Check network statistics: `sdk.get_network_statistics()`
- Reduce update rate: `config.motor_update_interval = std::chrono::milliseconds(20);`
- Check network bandwidth and latency

### Servo Mode Not Working

- Ensure update rate is ≥20ms (≤50Hz)
- Check motor mode: `sdk.motor_controller().set_motor_mode(motor_id, MODE_SERVO)`
- Verify no timeout errors in network statistics

## License

[Add your license here]

## Contributing

[Add contribution guidelines here]

## Contact

[Add contact information here]

## Acknowledgments

Based on the WGS communication protocol specification from Feishu documentation.
