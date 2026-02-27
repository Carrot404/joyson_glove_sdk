# Joyson Glove SDK - API Reference

## Table of Contents

1. [Core Classes](#core-classes)
2. [Data Structures](#data-structures)
3. [Configuration](#configuration)
4. [Error Handling](#error-handling)
5. [Thread Safety](#thread-safety)

---

## Core Classes

### GloveSDK

Main SDK interface providing unified API for all components.

#### Constructor

```cpp
explicit GloveSDK(const GloveConfig& config = GloveConfig{});
```

Creates a new SDK instance with the specified configuration.

**Parameters:**
- `config`: Configuration structure (optional, uses defaults if not provided)

**Example:**
```cpp
GloveConfig config;
config.server_ip = "192.168.10.123";
config.auto_start_threads = true;
GloveSDK sdk(config);
```

#### Lifecycle Methods

##### initialize()

```cpp
bool initialize();
```

Initialize SDK and connect to hardware.

**Returns:** `true` if initialization successful, `false` otherwise

**Side Effects:**
- Creates UDP connection
- Initializes all motors to position mode
- Starts background threads (if `auto_start_threads` is true)

**Example:**
```cpp
if (!sdk.initialize()) {
    std::cerr << "Initialization failed" << std::endl;
    return 1;
}
```

##### shutdown()

```cpp
void shutdown();
```

Shutdown SDK and disconnect from hardware.

**Side Effects:**
- Stops all background threads
- Closes UDP connection
- Releases all resources

**Example:**
```cpp
sdk.shutdown();
```

##### is_initialized()

```cpp
bool is_initialized() const;
```

Check if SDK is initialized.

**Returns:** `true` if initialized, `false` otherwise

#### Motor Control Methods

##### set_motor_position()

```cpp
bool set_motor_position(uint8_t motor_id, uint16_t position);
```

Set target position for a single motor.

**Parameters:**
- `motor_id`: Motor ID (1-6)
- `position`: Target position in steps [0, 2000]

**Returns:** `true` if command sent successfully, `false` otherwise

**Example:**
```cpp
sdk.set_motor_position(1, 1000);  // Set motor 1 to position 1000
```

##### set_motor_force()

```cpp
bool set_motor_force(uint8_t motor_id, uint16_t force);
```

Set target force for a single motor.

**Parameters:**
- `motor_id`: Motor ID (1-6)
- `force`: Target force [0, 4095]

**Returns:** `true` if command sent successfully, `false` otherwise

**Example:**
```cpp
sdk.set_motor_force(2, 2048);  // Set motor 2 to force 2048
```

##### set_motor_speed()

```cpp
bool set_motor_speed(uint8_t motor_id, uint16_t speed, uint16_t target_position);
```

Set speed and target position for a single motor.

**Parameters:**
- `motor_id`: Motor ID (1-6)
- `speed`: Speed value
- `target_position`: Target position [0, 2000]

**Returns:** `true` if command sent successfully, `false` otherwise

**Example:**
```cpp
sdk.set_motor_speed(3, 500, 1500);  // Motor 3: speed 500, target 1500
```

##### set_all_positions()

```cpp
bool set_all_positions(const std::array<uint16_t, NUM_MOTORS>& positions);
```

Set target positions for all motors (batch operation).

**Parameters:**
- `positions`: Array of 6 target positions

**Returns:** `true` if all commands sent successfully, `false` otherwise

**Example:**
```cpp
std::array<uint16_t, NUM_MOTORS> positions = {500, 600, 700, 800, 900, 1000};
sdk.set_all_positions(positions);
```

##### set_all_forces()

```cpp
bool set_all_forces(const std::array<uint16_t, NUM_MOTORS>& forces);
```

Set target forces for all motors (batch operation).

**Parameters:**
- `forces`: Array of 6 target forces

**Returns:** `true` if all commands sent successfully, `false` otherwise

#### Sensor Reading Methods

##### get_encoder_angles()

```cpp
std::array<float, NUM_ENCODER_CHANNELS> get_encoder_angles();
```

Get cached encoder angles (non-blocking).

**Returns:** Array of 16 encoder angles in degrees [0, 360]

**Example:**
```cpp
auto angles = sdk.get_encoder_angles();
std::cout << "CH0: " << angles[0] << "°" << std::endl;
```

##### get_imu_orientation()

```cpp
ImuData get_imu_orientation();
```

Get cached IMU orientation (non-blocking).

**Returns:** IMU data structure with roll, pitch, yaw

**Example:**
```cpp
auto imu = sdk.get_imu_orientation();
std::cout << "Roll: " << imu.roll << "° Pitch: " << imu.pitch
          << "° Yaw: " << imu.yaw << "°" << std::endl;
```

#### Calibration Methods

##### calibrate_all()

```cpp
bool calibrate_all();
```

Calibrate all sensors (encoders and IMU).

**Returns:** `true` if calibration successful, `false` otherwise

**Side Effects:**
- Sets current encoder voltages as zero point
- Sets current IMU orientation as zero
- Saves calibration to files

**Example:**
```cpp
std::cout << "Hold glove in neutral position and press Enter..." << std::endl;
std::cin.get();
sdk.calibrate_all();
```

##### calibrate_encoders()

```cpp
bool calibrate_encoders();
```

Calibrate encoders only.

**Returns:** `true` if calibration successful, `false` otherwise

##### calibrate_imu()

```cpp
bool calibrate_imu();
```

Calibrate IMU only.

**Returns:** `true` if calibration successful, `false` otherwise

#### Component Access

##### motor_controller()

```cpp
MotorController& motor_controller();
const MotorController& motor_controller() const;
```

Get reference to motor controller.

**Returns:** Reference to MotorController instance

**Example:**
```cpp
auto status = sdk.motor_controller().get_motor_status(1);
```

##### encoder_reader()

```cpp
EncoderReader& encoder_reader();
const EncoderReader& encoder_reader() const;
```

Get reference to encoder reader.

**Returns:** Reference to EncoderReader instance

##### imu_reader()

```cpp
ImuReader& imu_reader();
const ImuReader& imu_reader() const;
```

Get reference to IMU reader.

**Returns:** Reference to ImuReader instance

##### udp_client()

```cpp
UdpClient& udp_client();
const UdpClient& udp_client() const;
```

Get reference to UDP client.

**Returns:** Reference to UdpClient instance

#### Statistics Methods

##### get_network_statistics()

```cpp
NetworkStatistics get_network_statistics() const;
```

Get network statistics.

**Returns:** NetworkStatistics structure

**Example:**
```cpp
auto stats = sdk.get_network_statistics();
std::cout << "Packets sent: " << stats.packets_sent << std::endl;
std::cout << "Success rate: "
          << (100.0 * stats.packets_received / stats.packets_sent) << "%" << std::endl;
```

##### reset_network_statistics()

```cpp
void reset_network_statistics();
```

Reset network statistics counters to zero.

---

## MotorController

Motor controller for LAF actuators.

### Key Methods

#### set_motor_mode()

```cpp
bool set_motor_mode(uint8_t motor_id, uint8_t mode);
```

Set motor control mode.

**Parameters:**
- `motor_id`: Motor ID (1-6)
- `mode`: Control mode
  - `MODE_POSITION` (0x00): Position control
  - `MODE_SERVO` (0x01): Servo mode (high-frequency)
  - `MODE_SPEED` (0x02): Speed control
  - `MODE_FORCE` (0x03): Force control

**Returns:** `true` if successful, `false` otherwise

**Example:**
```cpp
sdk.motor_controller().set_motor_mode(1, MODE_SERVO);
```

#### get_motor_status()

```cpp
std::optional<MotorStatus> get_motor_status(uint8_t motor_id);
```

Read motor status from hardware (blocking).

**Parameters:**
- `motor_id`: Motor ID (1-6)

**Returns:** MotorStatus if successful, `std::nullopt` otherwise

**Example:**
```cpp
auto status = sdk.motor_controller().get_motor_status(1);
if (status) {
    std::cout << "Position: " << status->position << std::endl;
    std::cout << "Velocity: " << status->velocity << std::endl;
    std::cout << "Force: " << status->force << std::endl;
}
```

#### get_cached_status()

```cpp
std::optional<MotorStatus> get_cached_status(uint8_t motor_id) const;
```

Get cached motor status (non-blocking).

**Parameters:**
- `motor_id`: Motor ID (1-6)

**Returns:** Cached MotorStatus

**Note:** Data is updated by background thread at configured rate (default 100Hz)

#### start_status_update_thread()

```cpp
void start_status_update_thread();
```

Start background thread for automatic status updates.

#### stop_status_update_thread()

```cpp
void stop_status_update_thread();
```

Stop background status update thread.

---

## EncoderReader

Encoder reader for 16-channel ADC data.

### Key Methods

#### read_encoders()

```cpp
std::optional<EncoderData> read_encoders();
```

Read encoder data from hardware (blocking).

**Returns:** EncoderData if successful, `std::nullopt` otherwise

#### get_cached_data()

```cpp
EncoderData get_cached_data() const;
```

Get cached encoder data (non-blocking).

**Returns:** Cached EncoderData

#### get_cached_angles()

```cpp
std::array<float, NUM_ENCODER_CHANNELS> get_cached_angles() const;
```

Get cached encoder angles (non-blocking).

**Returns:** Array of 16 angles in degrees

#### voltages_to_angles()

```cpp
std::array<float, NUM_ENCODER_CHANNELS> voltages_to_angles(
    const std::array<float, NUM_ENCODER_CHANNELS>& voltages) const;
```

Convert voltages to angles.

**Parameters:**
- `voltages`: Array of 16 voltages [0, 4V]

**Returns:** Array of 16 angles in degrees [0, 360]

**Conversion Formula:** `angle = (voltage / 4.0) * 360.0`

#### calibrate_zero_point()

```cpp
bool calibrate_zero_point();
```

Calibrate encoder zero point.

**Returns:** `true` if successful, `false` otherwise

**Side Effects:**
- Sets current voltages as zero point
- Saves calibration to file

#### load_calibration()

```cpp
bool load_calibration(const std::string& filename = "");
```

Load calibration from file.

**Parameters:**
- `filename`: Calibration file path (optional, uses default if empty)

**Returns:** `true` if successful, `false` otherwise

#### save_calibration()

```cpp
bool save_calibration(const std::string& filename = "") const;
```

Save calibration to file.

**Parameters:**
- `filename`: Calibration file path (optional, uses default if empty)

**Returns:** `true` if successful, `false` otherwise

---

## ImuReader

IMU reader for 3-axis orientation data.

### Key Methods

#### read_imu()

```cpp
std::optional<ImuData> read_imu();
```

Read IMU data from hardware (blocking).

**Returns:** ImuData if successful, `std::nullopt` otherwise

#### get_cached_data()

```cpp
ImuData get_cached_data() const;
```

Get cached IMU data (non-blocking).

**Returns:** Cached ImuData

#### calibrate_zero_orientation()

```cpp
bool calibrate_zero_orientation();
```

Calibrate IMU zero orientation.

**Returns:** `true` if successful, `false` otherwise

**Side Effects:**
- Sets current orientation as zero
- Saves calibration to file

---

## Data Structures

### MotorStatus

```cpp
struct MotorStatus {
    uint8_t motor_id;
    uint16_t position;      // Current position [0, 2000]
    int16_t velocity;       // Current velocity (steps/s)
    uint16_t force;         // Current force [0, 4095]
    uint8_t mode;           // Current mode
    uint8_t state;          // Motor state flags
    std::chrono::steady_clock::time_point timestamp;
};
```

### EncoderData

```cpp
struct EncoderData {
    std::array<float, NUM_ENCODER_CHANNELS> voltages;  // ADC voltages [0, 4V]
    std::chrono::steady_clock::time_point timestamp;
};
```

### ImuData

```cpp
struct ImuData {
    float roll;   // Roll angle in degrees
    float pitch;  // Pitch angle in degrees
    float yaw;    // Yaw angle in degrees
    std::chrono::steady_clock::time_point timestamp;
};
```

### NetworkStatistics

```cpp
struct NetworkStatistics {
    uint64_t packets_sent;
    uint64_t packets_received;
    uint64_t send_errors;
    uint64_t receive_errors;
    uint64_t timeouts;
    uint64_t checksum_errors;
};
```

---

## Configuration

### GloveConfig

```cpp
struct GloveConfig {
    // Network configuration
    std::string server_ip = "192.168.10.123";
    uint16_t server_port = 8080;
    std::chrono::milliseconds send_timeout{100};
    std::chrono::milliseconds receive_timeout{100};

    // Component configuration
    bool auto_start_threads = true;
    std::chrono::milliseconds motor_update_interval{10};    // 100Hz
    std::chrono::milliseconds encoder_update_interval{10};  // 100Hz
    std::chrono::milliseconds imu_update_interval{10};      // 100Hz

    // Calibration files
    std::string encoder_calibration_file = "encoder_calibration.bin";
    std::string imu_calibration_file = "imu_calibration.bin";
};
```

**Configuration Examples:**

```cpp
// Example 1: Custom IP and port
GloveConfig config;
config.server_ip = "192.168.1.100";
config.server_port = 9000;

// Example 2: Longer timeouts for slow networks
config.send_timeout = std::chrono::milliseconds(500);
config.receive_timeout = std::chrono::milliseconds(500);

// Example 3: Lower update rate to reduce network load
config.motor_update_interval = std::chrono::milliseconds(20);  // 50Hz
config.encoder_update_interval = std::chrono::milliseconds(20);
config.imu_update_interval = std::chrono::milliseconds(20);

// Example 4: Manual thread control
config.auto_start_threads = false;
GloveSDK sdk(config);
sdk.initialize();
sdk.encoder_reader().start_update_thread();  // Only start encoder thread
```

---

## Error Handling

The SDK uses a non-throwing API design with the following error handling patterns:

### Return Value Patterns

1. **Boolean return (`bool`)**: Indicates success/failure
   ```cpp
   if (!sdk.initialize()) {
       std::cerr << "Initialization failed" << std::endl;
   }
   ```

2. **Optional return (`std::optional<T>`)**: Returns value or nullopt on error
   ```cpp
   auto status = sdk.motor_controller().get_motor_status(1);
   if (!status) {
       std::cerr << "Failed to read motor status" << std::endl;
   }
   ```

### Error Logging

All errors are logged to stderr with context information:

```
[UdpClient] Failed to connect to 192.168.10.123:8080 - Connection refused
[MotorController] Invalid motor ID: 7
[EncoderReader] Failed to read encoders for calibration
```

### Error Recovery

The SDK does NOT automatically reconnect on network failure. User application is responsible for:

```cpp
// Detect connection loss
auto stats = sdk.get_network_statistics();
if (stats.timeouts > 100) {
    std::cerr << "Too many timeouts, reconnecting..." << std::endl;
    sdk.shutdown();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    sdk.initialize();
}
```

---

## Thread Safety

### Thread-Safe Operations

All SDK operations are thread-safe:

```cpp
// Safe to call from multiple threads
std::thread t1([&]() { sdk.set_motor_position(1, 1000); });
std::thread t2([&]() { sdk.set_motor_position(2, 1500); });
t1.join();
t2.join();
```

### Background Threads

The SDK uses background threads for asynchronous updates:

- **Motor Status Thread**: Updates motor status cache at configured rate
- **Encoder Update Thread**: Updates encoder data cache at configured rate
- **IMU Update Thread**: Updates IMU data cache at configured rate

### Data Freshness

Check data age to ensure freshness:

```cpp
auto age = sdk.encoder_reader().get_data_age();
if (age > std::chrono::milliseconds(100)) {
    std::cerr << "Warning: Encoder data is stale (" << age.count() << " ms old)" << std::endl;
}
```

---

## Constants

```cpp
// Hardware limits
constexpr uint8_t NUM_MOTORS = 6;
constexpr uint8_t NUM_ENCODER_CHANNELS = 16;
constexpr uint16_t MOTOR_POSITION_MAX = 2000;
constexpr uint16_t MOTOR_FORCE_MAX = 4095;
constexpr float ENCODER_VOLTAGE_MAX = 4.0f;
constexpr float ENCODER_ANGLE_MAX = 360.0f;

// Motor modes
constexpr uint8_t MODE_POSITION = 0x00;
constexpr uint8_t MODE_SERVO = 0x01;
constexpr uint8_t MODE_SPEED = 0x02;
constexpr uint8_t MODE_FORCE = 0x03;
```

---

## Best Practices

### 1. Always Check Return Values

```cpp
// Good
if (!sdk.initialize()) {
    return 1;
}

// Bad
sdk.initialize();  // Ignoring return value
```

### 2. Use Cached Data for Real-Time Applications

```cpp
// Good: Non-blocking, uses cached data
auto angles = sdk.get_encoder_angles();

// Bad: Blocking, sends network request
auto data = sdk.encoder_reader().read_encoders();
```

### 3. Batch Operations When Possible

```cpp
// Good: Single network request
sdk.set_all_positions(positions);

// Bad: 6 network requests
for (int i = 1; i <= 6; ++i) {
    sdk.set_motor_position(i, positions[i-1]);
}
```

### 4. Monitor Network Statistics

```cpp
auto stats = sdk.get_network_statistics();
double success_rate = 100.0 * stats.packets_received / stats.packets_sent;
if (success_rate < 95.0) {
    std::cerr << "Warning: Low success rate (" << success_rate << "%)" << std::endl;
}
```

### 5. Calibrate Before Use

```cpp
sdk.initialize();
std::cout << "Hold glove in neutral position..." << std::endl;
std::this_thread::sleep_for(std::chrono::seconds(2));
sdk.calibrate_all();
```

---

## Version Information

- **SDK Version**: 1.0.0
- **Protocol Version**: WGS Communication Protocol
- **C++ Standard**: C++17
- **Supported Platforms**: Linux (Ubuntu 20.04+)

---

## See Also

- [README.md](README.md) - Project overview and installation
- [QUICKSTART.md](QUICKSTART.md) - Quick start guide
- [examples/](examples/) - Example programs
