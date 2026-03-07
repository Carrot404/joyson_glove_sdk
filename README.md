# Joyson Glove SDK

C++ SDK for controlling Joyson exoskeleton glove hardware with LAF actuators, encoders, and IMU sensors.

## Features

- **Motor Control**: Control 6 LAF actuators in multiple modes (position, servo, speed, force, voltage, speed-force)
- **Sensor Reading**: Read 16-channel encoder data and 3-axis IMU orientation
- **Thread-Safe**: All operations are thread-safe with mutex protection
- **Unified Update Thread**: Single background thread polls all sensors at configurable rate (default 10Hz)
- **Calibration Support**: Zero-point calibration for encoders and IMU
- **Non-Blocking API**: Cached data access for real-time performance
- **Data Freshness**: Monitor data age for each sensor component
- **ROS2-Friendly**: Clean C++ interface easy to wrap in ROS2 nodes

## Hardware Specifications

- **Motors**: 6 LAF actuators (ID 1-6)
  - Modes: Position, Servo, Speed, Force, Voltage, Speed-Force
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
- Linux (tested on Ubuntu 24.04+)
- Google Test (required when `BUILD_TESTS=ON`, default ON)

## Installation

### Install Dependencies

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential cmake libgtest-dev

# Arch Linux
sudo pacman -S base-devel cmake gtest
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
    GloveConfig config;
    config.auto_start_thread = false;  // Manual control
    GloveSDK sdk(config);

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
        std::cout << "Motor 1 position: " << status->actual_position << std::endl;
        std::cout << "Motor 1 force: " << status->force_value << std::endl;
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
    // Create SDK with unified background thread enabled
    GloveConfig config;
    config.auto_start_thread = true;
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

    // Check data freshness
    auto enc_age = sdk.encoder_reader().get_data_age();
    std::cout << "Encoder data age: " << enc_age.count() << " ms" << std::endl;

    sdk.shutdown();
    return 0;
}
```

### Standalone Component Testing

Each component (motor, encoder, IMU) can be used independently for testing:

```cpp
#include "joyson_glove/motor_controller.hpp"
#include "joyson_glove/udp_client.hpp"

using namespace joyson_glove;

int main() {
    // Configure UDP client
    UdpConfig udp_config;
    udp_config.server_ip = "192.168.10.123";
    udp_config.server_port = 8080;
    udp_config.local_port = 8080;

    auto udp_client = std::make_shared<UdpClient>(udp_config);
    if (!udp_client->connect()) return 1;

    // Create motor controller independently
    MotorControllerConfig motor_config;
    motor_config.auto_start_thread = false;

    MotorController motor(udp_client, motor_config);
    motor.initialize();

    // Direct blocking read
    auto status = motor.get_motor_status(1);
    if (status) {
        std::cout << "Position: " << status->actual_position << std::endl;
    }

    udp_client->disconnect();
    return 0;
}
```

## API Reference

### GloveSDK

Main SDK interface providing unified API for all components.

#### Initialization & Lifecycle

```cpp
GloveSDK(const GloveConfig& config = GloveConfig{});
bool initialize();
void shutdown();
bool is_initialized() const;

// Unified update thread control
void start();
void stop();
bool is_running() const;
```

#### Motor Control

```cpp
bool set_motor_mode(uint8_t motor_id, uint8_t mode);
bool set_all_modes(uint8_t mode);
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

#### Network Statistics

```cpp
NetworkStatistics get_network_statistics();
void reset_network_statistics();
```

#### Component Access

```cpp
MotorController& motor_controller();
EncoderReader& encoder_reader();
ImuReader& imu_reader();
UdpClient& udp_client();
const GloveConfig& get_config() const;
```

### Configuration

```cpp
struct GloveConfig {
    // Network configuration
    std::string server_ip = "192.168.10.123";
    uint16_t server_port = 8080;
    uint16_t local_port = 8080;  // Local port to bind (required for hardware)
    std::chrono::milliseconds send_timeout{100};
    std::chrono::milliseconds receive_timeout{100};

    // Unified thread configuration
    bool auto_start_thread = true;
    std::chrono::milliseconds update_interval{100};  // 10Hz unified update rate
};
```

### Data Structures

#### MotorStatus

```cpp
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
```

#### NetworkStatistics

```cpp
struct NetworkStatistics {
    uint64_t packets_sent = 0;
    uint64_t packets_received = 0;
    uint64_t send_errors = 0;
    uint64_t receive_errors = 0;
    uint64_t timeouts = 0;
    uint64_t checksum_errors = 0;
};
```

#### Motor Modes

```cpp
constexpr uint8_t MODE_POSITION    = 0x00;
constexpr uint8_t MODE_SERVO       = 0x01;
constexpr uint8_t MODE_SPEED       = 0x02;
constexpr uint8_t MODE_FORCE       = 0x03;
constexpr uint8_t MODE_VOLTAGE     = 0x04;
constexpr uint8_t MODE_SPEED_FORCE = 0x05;
```

## Examples

The SDK includes five example programs:

1. **basic_motor_control**: Basic motor control operations using GloveSDK
2. **read_sensors**: Reading encoder and IMU data with background updates
3. **test_motor_controller**: Standalone motor controller testing (direct component usage)
4. **test_encoder_reader**: Standalone encoder reader testing with calibration
5. **test_imu_reader**: Standalone IMU reader testing with calibration

Build and run examples:

```bash
cd build

# High-level SDK examples
./basic_motor_control
./read_sensors

# Standalone component tests (support optional IP/port arguments)
./test_motor_controller [server_ip] [port]
./test_encoder_reader [server_ip] [port]
./test_imu_reader [server_ip] [port]
```

## Threading Model

The SDK uses a **unified background thread** for asynchronous sensor data updates:

- **Unified Update Thread**: Polls motor status, encoder data, and IMU data in a single loop at a configurable rate (default 10Hz)
- Thread is controlled via `start()` / `stop()` / `is_running()` on GloveSDK
- Set `config.auto_start_thread = true` to auto-start on `initialize()`

Each component also supports its own independent thread for standalone usage:

- **MotorController**: `start_status_update_thread()` / `stop_status_update_thread()`
- **EncoderReader**: `start_update_thread()` / `stop_update_thread()`
- **ImuReader**: `start_update_thread()` / `stop_update_thread()`

All cached data is protected by mutexes for thread safety. Data freshness can be monitored via `get_data_age()` / `get_status_age()` methods.

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

// Or via component directly
sdk.encoder_reader().calibrate_zero_point();

// Access calibration data programmatically
auto cal = sdk.encoder_reader().get_calibration();
sdk.encoder_reader().set_calibration(cal);
```

### IMU Calibration

IMU calibration sets the current orientation as zero:

```cpp
sdk.calibrate_imu();

// Or via component directly
sdk.imu_reader().calibrate_zero_orientation();

// Access calibration data programmatically
auto cal = sdk.imu_reader().get_calibration();
sdk.imu_reader().set_calibration(cal);
```

## Network Statistics

Monitor network performance:

```cpp
auto stats = sdk.get_network_statistics();
std::cout << "Packets sent: " << stats.packets_sent << std::endl;
std::cout << "Packets received: " << stats.packets_received << std::endl;
std::cout << "Send errors: " << stats.send_errors << std::endl;
std::cout << "Receive errors: " << stats.receive_errors << std::endl;
std::cout << "Timeouts: " << stats.timeouts << std::endl;
std::cout << "Checksum errors: " << stats.checksum_errors << std::endl;

// Reset statistics
sdk.reset_network_statistics();
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
- Ensure local port binding is correct: `config.local_port = 8080`

### High Packet Loss

- Check network statistics: `sdk.get_network_statistics()`
- Reduce update rate: `config.update_interval = std::chrono::milliseconds(200);`
- Check network bandwidth and latency

## License

MIT License. See [LICENSE](LICENSE) for details.

## Contributing

[Add contribution guidelines here]

## Contact

[Add contact information here]

## Acknowledgments

[Add acknowledgments information here]
