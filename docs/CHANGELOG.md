# Joyson Glove SDK - Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0] - 2026-03-07

### Added

#### Unified Thread Architecture
- Unified background thread model: GloveSDK manages a single thread that polls all components (motors, encoders, IMU)
- `GloveSDK::start()` / `stop()` / `is_running()` for explicit thread lifecycle control
- `update_once()` method on MotorController, EncoderReader, ImuReader for single-cycle manual updates

#### New Convenience Methods
- `GloveSDK::set_motor_mode()` - direct motor mode control from SDK facade
- `GloveSDK::set_all_modes()` - set all motors to the same mode
- `MotorController::set_all_modes()` - batch mode setting at controller level
- `MotorController::get_all_cached_status()` - get all motor statuses at once
- `EncoderReader::adc_to_voltages()` - ADC raw value to voltage conversion
- `EncoderReader::get_calibration()` / `set_calibration()` - programmatic calibration access
- `ImuReader::get_calibration()` / `set_calibration()` - programmatic calibration access

#### Calibration Data Structures
- `EncoderCalibration` struct with zero voltages, scale factors, and calibration status
- `ImuCalibration` struct with roll/pitch/yaw offsets and calibration status

#### Enhanced Motor Status
- `MotorStatus` now includes: `target_position`, `actual_position`, `actual_current`, `force_value`, `force_raw`, `temperature`, `fault_code`

#### Protocol Enhancements
- `CMD_READ_MOTOR_ID` (0x10) - read motor ID command
- `TARGET_LRA` (0x04) - LRA module target constant
- `MODE_VOLTAGE` (0x04) and `MODE_SPEED_FORCE` (0x05) - additional motor modes
- Inner motor sub-protocol with dedicated headers (`0x55 0xAA` request, `0xAA 0x55` response)
- `Packet::calculate_motor_checksum()` - inner motor checksum calculation
- `Packet::validate_motor_checksum()` - inner motor checksum validation
- Motor register constants: `REG_MOTOR_ID`, `REG_MODE`, `REG_FORCE`, `REG_SPEED`, `REG_POSITION`

#### Input Validation
- `MotorController::is_valid_mode()`, `is_valid_force()`, `is_valid_position()` validation helpers

#### New Examples
- `test_motor_controller.cpp` - standalone motor controller test
- `test_encoder_reader.cpp` - standalone encoder reader test
- `test_imu_reader.cpp` - standalone IMU reader test

#### Other
- Local port binding (`GloveConfig::local_port`) for hardware communication
- Copyright headers with file descriptions on all source files
- `GloveSDK::get_config()` method to access current configuration

### Changed

#### Breaking Changes
- **GloveConfig**: Replaced per-component update intervals (`motor_update_interval`, `encoder_update_interval`, `imu_update_interval`) with unified `update_interval` (default 100ms / 10Hz)
- **GloveConfig**: Renamed `auto_start_threads` to `auto_start_thread` (singular)
- **GloveConfig**: Removed `encoder_calibration_file` and `imu_calibration_file` fields (calibration is now in-memory only)
- **Copy/Move semantics**: GloveSDK, MotorController, EncoderReader, ImuReader all disable copy and move operations
- **Packet format**: Length field changed from `uint16_t` to `uint8_t`; checksum field changed from `uint16_t` to `uint8_t`
- **Module ID**: Fixed to constant `0x01` (previously configurable)
- **Motor initialization**: Motors now initialize to force mode (previously position mode)
- **Default update rate**: Changed from 100Hz (10ms) to 10Hz (100ms) to match hardware capabilities
- **EncoderData**: Now stores `adc_values` (raw) and `voltages` (converted) instead of voltages only

#### Non-Breaking Changes
- Enhanced thread safety with `memory_order_acquire` on all atomic loads
- Improved error messages with component-specific prefixes

### Removed
- `EncoderReader::load_calibration()` / `save_calibration()` - file-based calibration persistence
- `ImuReader::load_calibration()` / `save_calibration()` - file-based calibration persistence
- `servo_mode_demo.cpp` example (functionality covered by other examples)
- spdlog dependency (removed from build system entirely)

### Fixed
- Correct packet length calculation in protocol encoding
- Correct endianness handling for IMU (big-endian float32) and encoder (big-endian uint16) data parsing
- Added local port binding to UDP client (required for hardware communication)
- Added motor inner-packet checksum validation
- Skip length and checksum validation for IMU/encoder responses (firmware bug workaround)

---

## [1.0.0] - 2026-02-27

### Added

#### Core Features
- Complete C++ SDK for Joyson exoskeleton glove
- UDP communication protocol implementation (192.168.10.123:8080)
- Motor control for 6 LAF actuators (position, force, speed, servo modes)
- Encoder data reading (16 channels, 0-4V -> 0-360 degrees)
- IMU orientation reading (roll, pitch, yaw)
- Thread-safe asynchronous data updates
- Calibration support with data persistence

#### Protocol Layer
- `protocol.hpp/cpp`: Complete protocol encoding/decoding
- Packet serialization/deserialization
- Checksum calculation and validation
- Support for all motor commands (read status, set mode, set position, set force, set speed)
- Support for encoder and IMU data reading

#### Communication Layer
- `udp_client.hpp/cpp`: UDP client with timeout handling
- Configurable send/receive timeouts (default 100ms)
- Network statistics tracking (packets sent/received, errors, timeouts)
- Thread-safe send/receive operations
- Connection management

#### Device Controllers
- `motor_controller.hpp/cpp`: Motor control interface
  - Single motor and batch control operations
  - Status caching with background thread updates
  - Support for all 4 motor modes (position, servo, speed, force)
  - Motor status reading (position, velocity, force, mode, state)
- `encoder_reader.hpp/cpp`: Encoder data reader
  - 16-channel voltage reading
  - Voltage-to-angle conversion (0V=0 degrees, 4V=360 degrees)
  - Zero-point calibration
  - Calibration data persistence (binary format)
- `imu_reader.hpp/cpp`: IMU data reader
  - 3-axis orientation reading (roll, pitch, yaw)
  - Zero-orientation calibration
  - Drift compensation

#### SDK Interface
- `glove_sdk.hpp/cpp`: Main SDK facade
  - Unified initialization/shutdown
  - Convenience methods for common operations
  - Component access (motor_controller, encoder_reader, imu_reader, udp_client)
  - Global calibration (calibrate_all)
  - Network statistics access

#### Examples
- `basic_motor_control.cpp`: Basic motor control demonstration
- `read_sensors.cpp`: Sensor data reading demonstration
- `servo_mode_demo.cpp`: High-frequency servo control demonstration

#### Tests
- `test_protocol.cpp`: Protocol layer unit tests
- `test_udp_client.cpp`: UDP client unit tests

#### Documentation
- `README.md`: Complete project documentation
- `QUICKSTART.md`: Quick start guide
- `API_REFERENCE.md`: Comprehensive API documentation
- `PROTOCOL.md`: Protocol specification

#### Build System
- `CMakeLists.txt`: CMake build configuration (C++17, shared library, examples, tests)
- `cmake/joyson_glove_sdkConfig.cmake.in`: CMake package config

#### Scripts
- `scripts/build.sh`: Automated build script
- `scripts/setup_network.sh`: Network configuration script

### Technical Details

#### Architecture
- Layered architecture: Application -> SDK -> Device Controllers -> Communication -> Protocol
- Thread-safe design with mutex protection
- Asynchronous data updates with background threads
- Non-blocking cached data access
- Error handling with `std::optional` and `bool` return values

#### Performance
- Default update rate: 100Hz (10ms interval)
- Servo mode support: 50Hz (20ms interval)
- Typical latency: 5-20ms per request-response
- Target success rate: >99%

#### Dependencies
- C++17 compiler (GCC 7+, Clang 5+)
- CMake 3.16+
- POSIX threads
- Google Test (required for unit tests)

#### Platform Support
- Linux (Ubuntu 20.04+)
- POSIX socket API

### Known Limitations
- WiFi module: Not implemented
- LRA module: Not implemented
- Auto-reconnect: Not implemented (user must handle manually)
- Platform: Linux only (uses POSIX sockets)

---

## [Unreleased]

### Planned for 1.2.0
- [ ] ROS2 integration package
- [ ] Advanced logging with spdlog
- [ ] Performance profiling tools
- [ ] Data recording and playback
- [ ] Configuration file support (YAML/JSON)

### Planned for 1.3.0
- [ ] WiFi communication support
- [ ] LRA vibration feedback support
- [ ] Python bindings
- [ ] Auto-reconnect mechanism

### Planned for 2.0.0
- [ ] Windows support
- [ ] macOS support
- [ ] Advanced control algorithms (trajectory planning, force control)
- [ ] Multi-glove support

---

## Version History

- **1.1.0** (2026-03-07): Unified thread architecture, enhanced protocol, bug fixes
- **1.0.0** (2026-02-27): Initial release with core functionality

---

## Contributing

Please read [CONTRIBUTING.md](CONTRIBUTING.md) for details on our code of conduct and the process for submitting pull requests.

## License

This project is licensed under the [LICENSE](LICENSE) file in the root directory.

## Authors

- Joyson Glove SDK Team

## Acknowledgments

- Based on WGS communication protocol specification
- Inspired by best practices from ROS, OpenCV, and other robotics SDKs
