# Joyson Glove SDK - Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-02-27

### Added

#### Core Features
- Complete C++ SDK for Joyson exoskeleton glove
- UDP communication protocol implementation (192.168.10.123:8080)
- Motor control for 6 LAF actuators (position, force, speed, servo modes)
- Encoder data reading (16 channels, 0-4V → 0-360°)
- IMU orientation reading (roll, pitch, yaw)
- Thread-safe asynchronous data updates
- Calibration support with data persistence

#### Protocol Layer
- `protocol.hpp/cpp`: Complete protocol encoding/decoding
- Packet serialization/deserialization
- Checksum calculation and validation
- Support for all motor commands (read status, set mode, set position, set force, set speed)
- Support for encoder and IMU data reading
- Little-endian byte order handling

#### Communication Layer
- `udp_client.hpp/cpp`: UDP client with timeout handling
- Configurable send/receive timeouts (default 100ms)
- Network statistics tracking (packets sent/received, errors, timeouts)
- Thread-safe send/receive operations
- Connection management

#### Device Controllers
- `motor_controller.hpp/cpp`: Motor control interface
  - Single motor and batch control operations
  - Status caching with background thread updates (default 100Hz)
  - Support for all 4 motor modes (position, servo, speed, force)
  - Motor status reading (position, velocity, force, mode, state)
- `encoder_reader.hpp/cpp`: Encoder data reader
  - 16-channel voltage reading
  - Voltage-to-angle conversion (0V=0°, 4V=360°)
  - Zero-point calibration
  - Calibration data persistence (binary format)
  - Background thread updates (default 100Hz)
- `imu_reader.hpp/cpp`: IMU data reader
  - 3-axis orientation reading (roll, pitch, yaw)
  - Zero-orientation calibration
  - Drift compensation
  - Background thread updates (default 100Hz)

#### SDK Interface
- `glove_sdk.hpp/cpp`: Main SDK facade
  - Unified initialization/shutdown
  - Convenience methods for common operations
  - Component access (motor_controller, encoder_reader, imu_reader, udp_client)
  - Global calibration (calibrate_all)
  - Network statistics access

#### Examples
- `basic_motor_control.cpp`: Basic motor control demonstration
  - Initialize SDK
  - Set motor modes and positions
  - Read motor status
  - Display network statistics
- `read_sensors.cpp`: Sensor data reading demonstration
  - Background thread data updates
  - Calibration workflow
  - Real-time data display (10Hz)
  - Data freshness monitoring
- `servo_mode_demo.cpp`: High-frequency servo control demonstration
  - 50Hz control loop (20ms update rate)
  - Sinusoidal trajectory generation
  - Latency statistics
  - Performance monitoring

#### Tests
- `test_protocol.cpp`: Protocol layer unit tests
  - Packet serialization/deserialization
  - Checksum calculation
  - Command encoding
  - Response parsing
- `test_udp_client.cpp`: UDP client unit tests
  - Connection management
  - Send/receive operations
  - Timeout handling
  - Thread safety

#### Documentation
- `README.md`: Complete project documentation
  - Features overview
  - Hardware specifications
  - Installation instructions
  - Quick start guide
  - API reference
  - Examples
  - Troubleshooting
- `QUICKSTART.md`: Quick start guide
  - Step-by-step setup instructions
  - First program tutorial
  - Common issues and solutions
  - Performance optimization tips
- `API_REFERENCE.md`: Comprehensive API documentation
  - All classes and methods
  - Data structures
  - Configuration options
  - Error handling
  - Thread safety
  - Best practices
- `PROTOCOL.md`: Protocol specification
  - Packet format
  - Command details
  - Response format
  - Error handling
  - Performance metrics
- `PROJECT_SUMMARY.md`: Project implementation summary
  - Architecture overview
  - Implementation phases
  - Code statistics
  - Known limitations

#### Build System
- `CMakeLists.txt`: CMake build configuration
  - C++17 standard
  - Shared library build
  - Examples build
  - Tests build (Google Test)
  - Installation support
- `cmake/joyson_glove_sdkConfig.cmake.in`: CMake package config

#### Scripts
- `scripts/build.sh`: Automated build script
  - Dependency checking
  - Configurable build options (debug/release, examples, tests)
  - Parallel build support
  - Clean build option
  - Installation option
- `scripts/setup_network.sh`: Network configuration script
  - Interface detection
  - IP configuration
  - Connectivity testing
  - Firewall configuration
  - Configuration persistence

#### Miscellaneous
- `.gitignore`: Git ignore rules
- `CHANGELOG.md`: This file

### Technical Details

#### Architecture
- Layered architecture: Application → SDK → Device Controllers → Communication → Protocol
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
- Optional: spdlog (logging)
- Optional: Google Test (unit tests)

#### Platform Support
- Linux (Ubuntu 20.04+)
- POSIX socket API

### Known Limitations
- WiFi module: Not implemented (marked as TODO)
- LRA module: Not implemented (marked as TODO)
- Auto-reconnect: Not implemented (user must handle manually)
- Platform: Linux only (uses POSIX sockets)

### Future Work
- WiFi communication support
- LRA vibration feedback support
- ROS2 integration package
- Python bindings
- Windows/macOS support
- Auto-reconnect mechanism
- Advanced logging with spdlog
- Performance profiling tools
- Data visualization tools

---

## [Unreleased]

### Planned for 1.1.0
- [ ] ROS2 integration package
- [ ] Advanced logging with spdlog
- [ ] Performance profiling tools
- [ ] Data recording and playback
- [ ] Configuration file support (YAML/JSON)

### Planned for 1.2.0
- [ ] WiFi communication support
- [ ] LRA vibration feedback support
- [ ] Python bindings
- [ ] Auto-reconnect mechanism

### Planned for 2.0.0
- [ ] Windows support
- [ ] macOS support
- [ ] Advanced control algorithms (trajectory planning, force control)
- [ ] Multi-glove support
- [ ] Cloud integration

---

## Version History

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
