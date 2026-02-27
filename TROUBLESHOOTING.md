# Joyson Glove SDK - Troubleshooting Guide

## 常见问题和解决方案

本文档列出了使用 Joyson Glove SDK 时可能遇到的常见问题及其解决方案。

---

## 目录

1. [编译问题](#编译问题)
2. [网络连接问题](#网络连接问题)
3. [运行时错误](#运行时错误)
4. [性能问题](#性能问题)
5. [硬件问题](#硬件问题)
6. [调试技巧](#调试技巧)

---

## 编译问题

### 问题 1: CMake 版本过低

**错误信息**:
```
CMake Error: CMake 3.16 or higher is required. You are running version 3.10.2
```

**解决方案**:
```bash
# 方法 1: 从官方源安装最新版本
sudo apt-get remove cmake
sudo snap install cmake --classic

# 方法 2: 从源码编译
wget https://github.com/Kitware/CMake/releases/download/v3.25.0/cmake-3.25.0.tar.gz
tar -xzf cmake-3.25.0.tar.gz
cd cmake-3.25.0
./bootstrap && make -j$(nproc) && sudo make install
```

### 问题 2: 找不到 spdlog

**错误信息**:
```
Could not find a package configuration file provided by "spdlog"
```

**解决方案**:
```bash
# 安装 spdlog
sudo apt-get install libspdlog-dev

# 或者禁用 spdlog (SDK 会使用简单的 std::cout/cerr)
cmake -DBUILD_EXAMPLES=ON ..
```

### 问题 3: std::clamp 未定义

**错误信息**:
```
error: 'clamp' is not a member of 'std'
```

**解决方案**:
这个问题已经在代码中修复。如果仍然遇到,请确保:
1. 使用 C++17 或更高版本
2. 包含 `<algorithm>` 头文件

```cpp
#include <algorithm>
// ...
std::clamp(value, min, max);
```

### 问题 4: 链接错误

**错误信息**:
```
undefined reference to `pthread_create'
```

**解决方案**:
```bash
# 确保链接了 pthread 库
g++ -o my_app main.cpp -ljoyson_glove_sdk -lpthread
```

---

## 网络连接问题

### 问题 1: 无法连接到手套

**错误信息**:
```
[UdpClient] Failed to connect to 192.168.10.123:8080 - Connection refused
```

**诊断步骤**:

1. **检查硬件连接**:
```bash
# 检查网线是否连接
ip link show

# 查看接口状态 (应该是 UP)
ip link show eth0
```

2. **检查网络配置**:
```bash
# 查看 IP 地址
ip addr show eth0

# 应该看到类似:
# inet 192.168.10.100/24 scope global eth0
```

3. **测试连通性**:
```bash
# Ping 手套
ping -c 3 192.168.10.123

# 如果失败,检查:
# - 手套是否上电
# - 网线是否正常
# - IP 地址是否正确
```

4. **配置网络**:
```bash
# 使用提供的脚本
sudo ./scripts/setup_network.sh

# 或手动配置
sudo ip addr add 192.168.10.100/24 dev eth0
sudo ip link set eth0 up
```

### 问题 2: 连接超时

**错误信息**:
```
[UdpClient] Receive timeout
```

**解决方案**:

1. **增加超时时间**:
```cpp
GloveConfig config;
config.send_timeout = std::chrono::milliseconds(500);
config.receive_timeout = std::chrono::milliseconds(500);
GloveSDK sdk(config);
```

2. **检查网络延迟**:
```bash
# 测试延迟
ping -c 10 192.168.10.123

# 应该看到延迟 < 10ms
# 如果延迟 > 50ms, 检查网络设备
```

3. **检查防火墙**:
```bash
# 查看防火墙状态
sudo ufw status

# 允许 UDP 8080 端口
sudo ufw allow 8080/udp
```

### 问题 3: 高丢包率

**错误信息**:
```
Warning: Low success rate (85.3%)
```

**诊断步骤**:

1. **查看网络统计**:
```cpp
auto stats = sdk.get_network_statistics();
double success_rate = 100.0 * stats.packets_received / stats.packets_sent;
std::cout << "Success rate: " << success_rate << "%" << std::endl;
std::cout << "Timeouts: " << stats.timeouts << std::endl;
std::cout << "Checksum errors: " << stats.checksum_errors << std::endl;
```

2. **降低更新频率**:
```cpp
GloveConfig config;
config.motor_update_interval = std::chrono::milliseconds(20);  // 50Hz
config.encoder_update_interval = std::chrono::milliseconds(20);
config.imu_update_interval = std::chrono::milliseconds(20);
```

3. **检查网络质量**:
```bash
# 使用 iperf 测试带宽
sudo apt-get install iperf3

# 在手套端运行服务器 (如果可能)
iperf3 -s

# 在 PC 端运行客户端
iperf3 -c 192.168.10.123 -u -b 10M
```

---

## 运行时错误

### 问题 1: Segmentation Fault

**可能原因**:
1. 未初始化 SDK
2. 访问空指针
3. 线程竞争

**解决方案**:

1. **确保初始化**:
```cpp
GloveSDK sdk;
if (!sdk.initialize()) {
    std::cerr << "Initialization failed" << std::endl;
    return 1;
}
// 使用 SDK...
sdk.shutdown();
```

2. **检查返回值**:
```cpp
auto status = sdk.motor_controller().get_motor_status(1);
if (!status) {
    std::cerr << "Failed to read motor status" << std::endl;
    return;
}
// 使用 status...
```

3. **使用调试器**:
```bash
# 编译 Debug 版本
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# 使用 gdb 调试
gdb ./basic_motor_control
(gdb) run
# 如果崩溃
(gdb) backtrace
```

### 问题 2: 内存泄漏

**诊断工具**:
```bash
# 使用 valgrind 检测内存泄漏
sudo apt-get install valgrind

valgrind --leak-check=full --show-leak-kinds=all ./basic_motor_control
```

**常见原因**:
1. 未调用 `shutdown()`
2. 线程未正确停止

**解决方案**:
```cpp
// 确保在退出前调用 shutdown
GloveSDK sdk;
sdk.initialize();

// ... 使用 SDK ...

sdk.shutdown();  // 重要!
```

### 问题 3: 线程死锁

**症状**:
- 程序挂起
- CPU 使用率低
- 无响应

**诊断**:
```bash
# 使用 gdb 附加到进程
gdb -p <pid>
(gdb) thread apply all bt

# 查看所有线程的调用栈
```

**解决方案**:
1. 检查是否有循环等待
2. 确保 mutex 使用正确
3. 避免在持有锁时调用阻塞操作

---

## 性能问题

### 问题 1: 更新频率低于预期

**症状**:
```
Expected: 100Hz
Actual: 50Hz
```

**诊断**:
```cpp
// 测量实际更新频率
auto last_time = std::chrono::steady_clock::now();
int count = 0;

for (int i = 0; i < 1000; ++i) {
    auto data = sdk.get_encoder_angles();
    count++;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_time);
    if (elapsed.count() >= 1) {
        std::cout << "Update rate: " << count << " Hz" << std::endl;
        count = 0;
        last_time = now;
    }
}
```

**解决方案**:
1. 降低更新频率配置
2. 减少网络负载
3. 使用更快的网络连接

### 问题 2: 高 CPU 使用率

**诊断**:
```bash
# 使用 top 查看 CPU 使用率
top -p $(pgrep basic_motor_control)

# 使用 perf 分析性能
sudo perf record -g ./basic_motor_control
sudo perf report
```

**解决方案**:
1. 降低后台线程更新频率
2. 使用缓存数据而非实时查询
3. 批量操作而非单个操作

### 问题 3: 延迟过高

**诊断**:
```cpp
// 测量延迟
auto start = std::chrono::steady_clock::now();
sdk.set_motor_position(1, 1000);
auto end = std::chrono::steady_clock::now();

auto latency = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
std::cout << "Latency: " << latency.count() << " µs" << std::endl;
```

**解决方案**:
1. 检查网络延迟
2. 使用批量操作
3. 优化代码路径

---

## 硬件问题

### 问题 1: 电机不响应

**检查清单**:
1. 电机是否上电
2. 电机 ID 是否正确 (1-6)
3. 电机模式是否正确
4. 位置值是否在范围内 [0, 2000]

**诊断代码**:
```cpp
// 读取电机状态
for (uint8_t motor_id = 1; motor_id <= 6; ++motor_id) {
    auto status = sdk.motor_controller().get_motor_status(motor_id);
    if (status) {
        std::cout << "Motor " << (int)motor_id << ": "
                  << "Mode=" << (int)status->mode << " "
                  << "State=" << (int)status->state << " "
                  << "Position=" << status->position << std::endl;
    } else {
        std::cout << "Motor " << (int)motor_id << ": No response" << std::endl;
    }
}
```

### 问题 2: 编码器数据异常

**症状**:
- 电压值为 0 或 4V
- 数据不变化
- 数据跳变

**诊断**:
```cpp
// 读取原始电压
auto data = sdk.encoder_reader().get_cached_data();
for (size_t i = 0; i < 16; ++i) {
    std::cout << "CH" << i << ": " << data.voltages[i] << " V" << std::endl;
}

// 检查数据年龄
auto age = sdk.encoder_reader().get_data_age();
std::cout << "Data age: " << age.count() << " ms" << std::endl;
```

**解决方案**:
1. 检查传感器连接
2. 重新校准
3. 检查数据更新频率

### 问题 3: IMU 数据漂移

**症状**:
- 角度持续增加或减少
- 静止时数据变化

**解决方案**:
```cpp
// 重新校准 IMU
std::cout << "Keep glove still for calibration..." << std::endl;
std::this_thread::sleep_for(std::chrono::seconds(2));
sdk.calibrate_imu();
std::cout << "Calibration complete" << std::endl;
```

---

## 调试技巧

### 1. 启用详细日志

```cpp
// 在代码中添加更多日志
#define DEBUG_LOG
#ifdef DEBUG_LOG
    std::cout << "[DEBUG] Sending packet: " << packet_info << std::endl;
#endif
```

### 2. 使用网络监控工具

```bash
# 使用 tcpdump 抓包
sudo tcpdump -i eth0 -X udp port 8080

# 使用 Wireshark
sudo wireshark -i eth0 -f "udp port 8080"

# 使用提供的监控脚本
sudo python3 scripts/monitor_network.py
```

### 3. 检查数据包内容

```cpp
// 打印数据包十六进制
void print_packet(const std::vector<uint8_t>& data) {
    std::cout << "Packet (" << data.size() << " bytes): ";
    for (auto byte : data) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << (int)byte << " ";
    }
    std::cout << std::dec << std::endl;
}
```

### 4. 单步测试

```cpp
// 逐步测试每个功能
int main() {
    GloveSDK sdk;

    // 步骤 1: 初始化
    std::cout << "Step 1: Initialize..." << std::endl;
    if (!sdk.initialize()) {
        std::cerr << "Failed" << std::endl;
        return 1;
    }
    std::cout << "OK" << std::endl;

    // 步骤 2: 读取电机状态
    std::cout << "Step 2: Read motor status..." << std::endl;
    auto status = sdk.motor_controller().get_motor_status(1);
    if (!status) {
        std::cerr << "Failed" << std::endl;
        return 1;
    }
    std::cout << "OK: Position=" << status->position << std::endl;

    // ... 继续测试其他功能

    sdk.shutdown();
    return 0;
}
```

### 5. 使用断言

```cpp
#include <cassert>

// 在关键位置添加断言
assert(motor_id >= 1 && motor_id <= 6);
assert(position <= MOTOR_POSITION_MAX);
assert(sdk.is_initialized());
```

---

## 获取帮助

如果以上方法都无法解决问题:

1. **查看文档**:
   - README.md
   - API_REFERENCE.md
   - PROTOCOL.md

2. **查看示例代码**:
   - examples/basic_motor_control.cpp
   - examples/read_sensors.cpp
   - examples/servo_mode_demo.cpp

3. **运行测试**:
```bash
cd build
ctest --output-on-failure
```

4. **提交 Issue**:
   - 包含完整的错误信息
   - 包含系统信息 (OS, 编译器版本)
   - 包含最小可复现示例
   - 包含网络统计信息

5. **联系技术支持**:
   - 提供详细的问题描述
   - 提供日志文件
   - 提供网络抓包 (如果相关)

---

**最后更新**: 2026-02-27
