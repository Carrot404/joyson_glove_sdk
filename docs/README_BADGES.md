# Joyson Glove SDK

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B17)
[![Platform](https://img.shields.io/badge/Platform-Linux-green.svg)](https://www.linux.org/)
[![Build Status](https://img.shields.io/badge/Build-Passing-brightgreen.svg)](https://github.com)
[![Version](https://img.shields.io/badge/Version-1.1.0-blue.svg)](CHANGELOG.md)

Complete C++ SDK for controlling the Joyson exoskeleton glove hardware.

[English](README.md) | [中文文档](README_CN.md)

---

## Quick Start

### Installation

```bash
# Clone repository
git clone <repository-url>
cd joyson_glove_sdk

# Install dependencies
sudo apt-get install build-essential cmake libgtest-dev

# Build
./scripts/build.sh

# Install (optional)
sudo ./scripts/install.sh
```

### Minimal Example

```cpp
#include "joyson_glove/glove_sdk.hpp"

int main() {
    joyson_glove::GloveSDK sdk;

    // Initialize
    if (!sdk.initialize()) {
        return 1;
    }

    // Control motors
    sdk.set_motor_position(1, 1000);

    // Read sensors
    auto angles = sdk.get_encoder_angles();
    auto imu = sdk.get_imu_orientation();

    // Shutdown
    sdk.shutdown();
    return 0;
}
```

---

## Features

- **Motor control**: 6 LAF actuators, 6 modes (position, servo, speed, force, voltage, speed-force)
- **Sensor data**: 16-channel encoder + 3-axis IMU
- **Thread-safe**: All operations protected with mutexes and atomics
- **Unified update thread**: Single background thread polls all components (default 10Hz)
- **Calibration**: Encoder and IMU zero-point calibration (in-memory)
- **Non-blocking API**: Cached data access for real-time performance
- **ROS2 friendly**: Clean C++ interface, easy to integrate

---

## Documentation

- [Quick Start Guide](QUICKSTART.md) - Installation and first steps
- [API Reference](API_REFERENCE.md) - Complete API documentation
- [Protocol Specification](PROTOCOL.md) - UDP communication protocol
- [Troubleshooting](TROUBLESHOOTING.md) - Common issues and solutions
- [Contributing](CONTRIBUTING.md) - Development guidelines
- [Changelog](CHANGELOG.md) - Version history

---

## Hardware Specifications

| Component | Specification | Details |
|-----------|---------------|---------|
| **Motors** | 6 LAF actuators | ID 1-6, position range [0, 2000] |
| **Encoders** | 16-channel ADC | 0-4V -> 0-360 degrees |
| **IMU** | 3-axis orientation | Roll, Pitch, Yaw |
| **Communication** | UDP | 192.168.10.123:8080 |

---

## System Requirements

- **OS**: Linux (Ubuntu 20.04+)
- **Compiler**: GCC 7+ or Clang 5+ (C++17 support)
- **CMake**: 3.16+
- **Dependencies**: pthread (required), Google Test (required for tests)

---

## Project Structure

```
joyson_glove_sdk/
├── include/joyson_glove/    # Public headers
├── src/                     # Implementation files
├── examples/                # Example programs
├── tests/                   # Unit tests
├── docs/                    # Documentation
├── scripts/                 # Utility scripts
└── cmake/                   # CMake configuration
```

---

## Example Programs

### 1. Basic Motor Control

```bash
./build/basic_motor_control
```

Demonstrates SDK initialization, motor mode/position control, status reading, and network statistics.

### 2. Sensor Data Reading

```bash
./build/read_sensors
```

Demonstrates unified background thread updates, sensor calibration, and real-time data display.

### 3. Motor Controller Test

```bash
./build/test_motor_controller
```

Standalone motor controller test for verifying individual motor communication.

### 4. Encoder Reader Test

```bash
./build/test_encoder_reader
```

Standalone encoder test showing all 16 ADC channels.

### 5. IMU Reader Test

```bash
./build/test_imu_reader
```

Standalone IMU test for roll/pitch/yaw verification.

---

## Build Tools

```bash
# Automated build
./scripts/build.sh

# Debug mode
./scripts/build.sh --debug

# Clean build
./scripts/build.sh --clean

# Network configuration
sudo ./scripts/setup_network.sh
```

---

## Testing

```bash
# Build tests
cmake -DBUILD_TESTS=ON ..
make

# Run all tests
ctest --output-on-failure

# Run individual tests
./test_protocol
./test_udp_client
```

---

## Performance

| Metric | Value |
|--------|-------|
| Default update rate | 10Hz (100ms) |
| Configurable range | 1-100Hz |
| Typical latency | 5-20ms per request |
| Target success rate | >99% |

---

## Roadmap

### v1.1.0 (Current)
- [x] Unified thread architecture
- [x] Enhanced protocol (dual-layer checksum, mixed endianness)
- [x] Standalone component test examples
- [x] Input validation helpers
- [x] Bug fixes (endianness, packet length, local port binding)

### v1.2.0 (Planned)
- [ ] ROS2 integration package
- [ ] Performance profiling tools
- [ ] Data recording and playback

### v1.3.0 (Planned)
- [ ] WiFi communication support
- [ ] LRA vibration feedback
- [ ] Python bindings

### v2.0.0 (Future)
- [ ] Windows/macOS support
- [ ] Advanced control algorithms
- [ ] Multi-glove support

---

## Contributing

Contributions welcome! See [Contributing Guide](CONTRIBUTING.md)

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## Authors

Joyson Glove SDK Team

## Acknowledgments

- Based on WGS communication protocol specification
- Inspired by best practices from ROS, OpenCV, and other robotics SDKs

---

*Last updated: 2026-03-07 | Version 1.1.0*
