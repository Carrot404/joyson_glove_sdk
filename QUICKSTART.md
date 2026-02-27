# Joyson Glove SDK - Quick Start Guide

## 快速开始指南

### 1. 环境准备

确保你的系统满足以下要求:

```bash
# 检查 C++ 编译器版本
g++ --version  # 需要 GCC 7+ 或 Clang 5+

# 检查 CMake 版本
cmake --version  # 需要 3.16+
```

### 2. 安装依赖

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential cmake libspdlog-dev

# 可选: 安装 Google Test (用于单元测试)
sudo apt-get install libgtest-dev
```

### 3. 编译项目

```bash
# 进入项目目录
cd joyson_glove_sdk

# 创建构建目录
mkdir build && cd build

# 配置项目
cmake ..

# 编译 (使用所有 CPU 核心)
make -j$(nproc)

# 查看生成的文件
ls -lh
# 你应该看到:
# - libjoyson_glove_sdk.so (共享库)
# - basic_motor_control (示例程序)
# - read_sensors (示例程序)
# - servo_mode_demo (示例程序)
```

### 4. 硬件连接

1. 确保手套硬件已上电
2. 通过网线连接到你的电脑
3. 配置网络接口:

```bash
# 查看网络接口
ip addr

# 配置静态 IP (假设接口是 eth0)
sudo ip addr add 192.168.10.100/24 dev eth0
sudo ip link set eth0 up

# 测试连接
ping 192.168.10.123
```

### 5. 运行示例程序

#### 示例 1: 基础电机控制

```bash
./basic_motor_control
```

这个示例会:
- 初始化 SDK
- 设置所有电机到位置模式
- 发送位置命令
- 读取电机状态
- 显示网络统计信息

#### 示例 2: 读取传感器数据

```bash
./read_sensors
```

这个示例会:
- 启动后台线程自动更新传感器数据
- 提示你校准传感器 (按 Enter 校准)
- 以 10Hz 频率显示编码器和 IMU 数据
- 运行 10 秒后自动退出

#### 示例 3: 伺服模式演示

```bash
./servo_mode_demo
```

这个示例会:
- 设置电机到伺服模式
- 以 50Hz 频率发送正弦轨迹命令
- 显示控制延迟统计
- 运行 10 秒后退出

### 6. 集成到你的项目

#### 方法 1: 直接链接

```cmake
# 在你的 CMakeLists.txt 中
find_package(joyson_glove_sdk REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app joyson_glove::joyson_glove_sdk)
```

#### 方法 2: 手动链接

```cmake
# 在你的 CMakeLists.txt 中
include_directories(/path/to/joyson_glove_sdk/include)
link_directories(/path/to/joyson_glove_sdk/build)

add_executable(my_app main.cpp)
target_link_libraries(my_app joyson_glove_sdk pthread)
```

### 7. 编写你的第一个程序

创建 `my_first_app.cpp`:

```cpp
#include "joyson_glove/glove_sdk.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace joyson_glove;

int main() {
    // 创建 SDK 实例
    GloveSDK sdk;

    // 初始化
    if (!sdk.initialize()) {
        std::cerr << "初始化失败!" << std::endl;
        return 1;
    }

    std::cout << "SDK 初始化成功!" << std::endl;

    // 设置电机 1 到位置 1000
    if (sdk.set_motor_position(1, 1000)) {
        std::cout << "电机 1 位置命令发送成功" << std::endl;
    }

    // 等待 1 秒
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // 读取电机状态
    auto status = sdk.motor_controller().get_motor_status(1);
    if (status) {
        std::cout << "电机 1 当前位置: " << status->position << std::endl;
    }

    // 读取编码器数据
    auto angles = sdk.get_encoder_angles();
    std::cout << "编码器通道 0 角度: " << angles[0] << "°" << std::endl;

    // 读取 IMU 数据
    auto imu = sdk.get_imu_orientation();
    std::cout << "IMU Roll: " << imu.roll << "° Pitch: " << imu.pitch
              << "° Yaw: " << imu.yaw << "°" << std::endl;

    // 关闭 SDK
    sdk.shutdown();
    std::cout << "SDK 已关闭" << std::endl;

    return 0;
}
```

编译并运行:

```bash
g++ -std=c++17 my_first_app.cpp -o my_first_app \
    -I../include -L. -ljoyson_glove_sdk -lpthread

# 设置库路径
export LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH

# 运行
./my_first_app
```

### 8. 常见问题排查

#### 问题 1: 连接失败

```
[UdpClient] Failed to connect to 192.168.10.123:8080
```

**解决方法**:
1. 检查网络连接: `ping 192.168.10.123`
2. 检查防火墙: `sudo ufw status`
3. 检查硬件是否上电

#### 问题 2: 超时错误

```
[UdpClient] Receive timeout
```

**解决方法**:
1. 增加超时时间:
```cpp
GloveConfig config;
config.receive_timeout = std::chrono::milliseconds(500);
GloveSDK sdk(config);
```

2. 检查网络延迟: `ping -c 10 192.168.10.123`

#### 问题 3: 校验和错误

```
[UdpClient] Failed to deserialize packet
```

**解决方法**:
1. 检查网络质量
2. 查看网络统计: `sdk.get_network_statistics()`
3. 如果错误率 >5%, 检查网线和网络设备

#### 问题 4: 编译错误

```
fatal error: joyson_glove/glove_sdk.hpp: No such file or directory
```

**解决方法**:
```bash
# 确保包含路径正确
g++ -I/path/to/joyson_glove_sdk/include ...
```

### 9. 性能优化建议

#### 减少网络负载

```cpp
// 方法 1: 降低更新频率
GloveConfig config;
config.motor_update_interval = std::chrono::milliseconds(20);  // 50Hz
config.encoder_update_interval = std::chrono::milliseconds(20);
config.imu_update_interval = std::chrono::milliseconds(20);

// 方法 2: 禁用不需要的后台线程
config.auto_start_threads = false;
GloveSDK sdk(config);
sdk.initialize();

// 只启动需要的线程
sdk.encoder_reader().start_update_thread();
```

#### 批量控制电机

```cpp
// 不推荐: 逐个控制 (6 次网络请求)
for (int i = 1; i <= 6; ++i) {
    sdk.set_motor_position(i, positions[i-1]);
}

// 推荐: 批量控制 (1 次网络请求)
std::array<uint16_t, NUM_MOTORS> positions = {500, 600, 700, 800, 900, 1000};
sdk.set_all_positions(positions);
```

#### 使用缓存数据

```cpp
// 不推荐: 阻塞式读取 (每次都发送网络请求)
auto status = sdk.motor_controller().get_motor_status(1);

// 推荐: 读取缓存 (非阻塞, 后台线程自动更新)
auto status = sdk.motor_controller().get_cached_status(1);
```

### 10. 下一步

- 阅读完整 API 文档: [README.md](README.md)
- 查看示例代码: `examples/` 目录
- 运行单元测试: `make test` (需要安装 Google Test)
- 集成到 ROS2: 创建 ROS2 wrapper 包

### 11. 获取帮助

如果遇到问题:
1. 查看日志输出 (SDK 会打印详细错误信息)
2. 检查网络统计: `sdk.get_network_statistics()`
3. 查看 GitHub Issues
4. 联系技术支持

---

**祝你使用愉快! 🎉**
