# Contributing to Joyson Glove SDK

Thank you for your interest in contributing to the Joyson Glove SDK! This document provides guidelines and instructions for contributing.

## Table of Contents

1. [Code of Conduct](#code-of-conduct)
2. [Getting Started](#getting-started)
3. [Development Workflow](#development-workflow)
4. [Coding Standards](#coding-standards)
5. [Testing Guidelines](#testing-guidelines)
6. [Documentation](#documentation)
7. [Submitting Changes](#submitting-changes)
8. [Review Process](#review-process)

---

## Code of Conduct

### Our Pledge

We are committed to providing a welcoming and inclusive environment for all contributors.

### Expected Behavior

- Be respectful and considerate
- Use welcoming and inclusive language
- Accept constructive criticism gracefully
- Focus on what is best for the community
- Show empathy towards other community members

### Unacceptable Behavior

- Harassment, discrimination, or offensive comments
- Trolling, insulting, or derogatory remarks
- Publishing others' private information
- Any conduct that could be considered inappropriate in a professional setting

---

## Getting Started

### Prerequisites

1. **Development Environment**:
   - Linux (Ubuntu 20.04+ recommended)
   - C++17 compiler (GCC 7+ or Clang 5+)
   - CMake 3.16+
   - Git

2. **Optional Tools**:
   - spdlog (for logging)
   - Google Test (for unit tests)
   - clang-format (for code formatting)
   - clang-tidy (for static analysis)

### Setting Up Development Environment

```bash
# Clone repository
git clone <repository-url>
cd joyson_glove_sdk

# Install dependencies
sudo apt-get update
sudo apt-get install build-essential cmake libspdlog-dev libgtest-dev

# Install development tools
sudo apt-get install clang-format clang-tidy

# Build project
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run tests
ctest --output-on-failure
```

---

## Development Workflow

### 1. Fork and Clone

```bash
# Fork the repository on GitHub
# Clone your fork
git clone https://github.com/YOUR_USERNAME/joyson_glove_sdk.git
cd joyson_glove_sdk

# Add upstream remote
git remote add upstream https://github.com/ORIGINAL_OWNER/joyson_glove_sdk.git
```

### 2. Create a Branch

```bash
# Update your fork
git checkout main
git pull upstream main

# Create a feature branch
git checkout -b feature/your-feature-name
# or
git checkout -b fix/your-bug-fix
```

Branch naming conventions:
- `feature/` - New features
- `fix/` - Bug fixes
- `docs/` - Documentation updates
- `refactor/` - Code refactoring
- `test/` - Test additions or modifications

### 3. Make Changes

- Write clean, readable code
- Follow coding standards (see below)
- Add tests for new functionality
- Update documentation as needed
- Commit frequently with clear messages

### 4. Commit Changes

```bash
# Stage changes
git add .

# Commit with descriptive message
git commit -m "feat: add support for WiFi communication"
```

Commit message format:
```
<type>: <subject>

<body>

<footer>
```

Types:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes (formatting, etc.)
- `refactor`: Code refactoring
- `test`: Test additions or modifications
- `chore`: Build process or auxiliary tool changes

Example:
```
feat: add WiFi communication support

- Implement WiFi client class
- Add WiFi configuration options
- Update documentation

Closes #123
```

### 5. Push and Create Pull Request

```bash
# Push to your fork
git push origin feature/your-feature-name

# Create pull request on GitHub
```

---

## Coding Standards

### C++ Style Guide

We follow the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) with some modifications.

#### Naming Conventions

```cpp
// Classes: PascalCase
class MotorController { };

// Functions/Methods: snake_case
void read_sensor_data();

// Variables: snake_case
int motor_position;

// Constants: UPPER_SNAKE_CASE
constexpr int MAX_MOTORS = 6;

// Private members: trailing underscore
class Example {
private:
    int value_;
    std::mutex mutex_;
};

// Namespaces: snake_case
namespace joyson_glove { }
```

#### Code Formatting

Use `clang-format` with the provided configuration:

```bash
# Format single file
clang-format -i src/protocol.cpp

# Format all source files
find src include -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i
```

#### Header Guards

Use `#pragma once`:

```cpp
#pragma once

namespace joyson_glove {
// ...
}
```

#### Include Order

```cpp
// 1. Corresponding header (for .cpp files)
#include "joyson_glove/protocol.hpp"

// 2. C system headers
#include <sys/socket.h>

// 3. C++ standard library headers
#include <iostream>
#include <vector>
#include <memory>

// 4. Other library headers
#include <spdlog/spdlog.h>

// 5. Project headers
#include "joyson_glove/udp_client.hpp"
```

#### Comments

```cpp
/**
 * Brief description of the function
 *
 * Detailed description if needed.
 *
 * @param motor_id Motor ID (1-6)
 * @param position Target position [0, 2000]
 * @return true if successful, false otherwise
 */
bool set_motor_position(uint8_t motor_id, uint16_t position);

// Single-line comment for implementation details
int calculate_checksum() {
    // Explain complex logic
    return sum & 0xFFFF;
}
```

#### Error Handling

```cpp
// Use std::optional for functions that may fail
std::optional<MotorStatus> get_motor_status(uint8_t motor_id);

// Use bool for simple success/failure
bool initialize();

// Log errors with context
if (!connect()) {
    std::cerr << "[UdpClient] Failed to connect to "
              << config_.server_ip << ":" << config_.server_port
              << " - " << strerror(errno) << std::endl;
    return false;
}
```

#### Thread Safety

```cpp
// Always protect shared data with mutexes
class ThreadSafeClass {
public:
    void set_value(int value) {
        std::lock_guard<std::mutex> lock(mutex_);
        value_ = value;
    }

    int get_value() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return value_;
    }

private:
    mutable std::mutex mutex_;
    int value_;
};
```

#### Resource Management

```cpp
// Use RAII for resource management
class UdpClient {
public:
    UdpClient() { /* acquire resources */ }
    ~UdpClient() { disconnect(); /* release resources */ }

    // Disable copy, allow move
    UdpClient(const UdpClient&) = delete;
    UdpClient& operator=(const UdpClient&) = delete;
    UdpClient(UdpClient&&) noexcept;
    UdpClient& operator=(UdpClient&&) noexcept;
};
```

---

## Testing Guidelines

### Unit Tests

Write unit tests for all new functionality:

```cpp
#include <gtest/gtest.h>
#include "joyson_glove/protocol.hpp"

TEST(ProtocolTest, PacketSerialization) {
    Packet packet;
    packet.header = PACKET_HEADER;
    packet.length = 9;
    packet.module_id = MODULE_MOTOR;
    packet.target = 1;
    packet.command = CMD_READ_STATUS;
    packet.tail = PACKET_TAIL;

    auto serialized = packet.serialize();
    EXPECT_EQ(serialized.size(), 9);
    EXPECT_EQ(serialized[0], PACKET_HEADER);
}
```

### Integration Tests

Test with real hardware when possible:

```cpp
TEST(IntegrationTest, MotorControl) {
    GloveSDK sdk;
    ASSERT_TRUE(sdk.initialize());

    EXPECT_TRUE(sdk.set_motor_position(1, 1000));

    auto status = sdk.motor_controller().get_motor_status(1);
    ASSERT_TRUE(status.has_value());
    EXPECT_NEAR(status->position, 1000, 50);  // Allow 50 steps tolerance

    sdk.shutdown();
}
```

### Running Tests

```bash
# Build tests
cmake -DBUILD_TESTS=ON ..
make

# Run all tests
ctest --output-on-failure

# Run specific test
./test_protocol
```

---

## Documentation

### Code Documentation

- Document all public APIs with Doxygen-style comments
- Explain complex algorithms and design decisions
- Include usage examples for non-trivial functions

### User Documentation

When adding new features, update:
- `README.md` - Overview and quick start
- `API_REFERENCE.md` - Detailed API documentation
- `QUICKSTART.md` - Tutorial-style guide
- `PROTOCOL.md` - Protocol specification (if applicable)

### Examples

Add example programs for new features:

```cpp
// examples/new_feature_demo.cpp
#include "joyson_glove/glove_sdk.hpp"
#include <iostream>

int main() {
    // Demonstrate new feature
    joyson_glove::GloveSDK sdk;
    sdk.initialize();

    // Use new feature
    sdk.new_feature();

    sdk.shutdown();
    return 0;
}
```

---

## Submitting Changes

### Pull Request Checklist

Before submitting a pull request, ensure:

- [ ] Code follows style guidelines
- [ ] All tests pass
- [ ] New tests added for new functionality
- [ ] Documentation updated
- [ ] Commit messages are clear and descriptive
- [ ] No merge conflicts with main branch
- [ ] Code compiles without warnings

### Pull Request Template

```markdown
## Description
Brief description of changes

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Breaking change
- [ ] Documentation update

## Testing
Describe testing performed

## Checklist
- [ ] Code follows style guidelines
- [ ] Tests added/updated
- [ ] Documentation updated
- [ ] All tests pass

## Related Issues
Closes #123
```

---

## Review Process

### What to Expect

1. **Initial Review**: Maintainers will review within 1-2 weeks
2. **Feedback**: You may receive requests for changes
3. **Iteration**: Make requested changes and push updates
4. **Approval**: Once approved, changes will be merged

### Review Criteria

- Code quality and style
- Test coverage
- Documentation completeness
- Performance impact
- Backward compatibility
- Security considerations

---

## Questions?

If you have questions:
- Check existing documentation
- Search existing issues
- Create a new issue with the `question` label
- Contact maintainers

---

## License

By contributing, you agree that your contributions will be licensed under the same license as the project.

---

Thank you for contributing to Joyson Glove SDK! 🎉
