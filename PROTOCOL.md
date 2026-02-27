# Joyson Glove SDK - Protocol Specification

## 通讯协议规范

本文档详细描述了 Joyson 外骨骼手套的 UDP 通讯协议。

---

## 1. 协议概述

### 1.1 基本信息

- **传输协议**: UDP
- **服务器地址**: 192.168.10.123
- **服务器端口**: 8080
- **字节序**: Little-Endian (小端序)
- **校验方式**: 累加和 (Sum Checksum)

### 1.2 数据包格式

所有数据包遵循统一的格式:

```
+--------+--------+--------+--------+--------+--------+--------+--------+--------+
| Header | Length | Length | Module | Target | Command|  Body  |Checksum|Checksum|  Tail  |
|  (1B)  | Low(1B)| High(1B)|  ID(1B)|  (1B)  |  (1B)  | (N bytes)| Low(1B)| High(1B)|  (1B)  |
+--------+--------+--------+--------+--------+--------+--------+--------+--------+
  0xEE                                                                              0x7E
```

**字段说明**:
- **Header**: 固定值 `0xEE`
- **Length**: 数据包总长度 (包括 Header 和 Tail),2 字节小端序
- **Module ID**: 模块标识 (0x01=电机, 0x02=编码器, 0x03=IMU)
- **Target**: 目标设备 ID (电机 ID 1-6, 传感器通常为 0x00)
- **Command**: 命令码
- **Body**: 命令参数或响应数据 (长度可变)
- **Checksum**: 校验和,2 字节小端序 (所有字节累加和的低 16 位)
- **Tail**: 固定值 `0x7E`

### 1.3 校验和计算

```cpp
uint16_t checksum = 0;
for (size_t i = 0; i < packet_length - 3; ++i) {  // 不包括 checksum 和 tail
    checksum += packet[i];
}
checksum = checksum & 0xFFFF;  // 取低 16 位
```

---

## 2. 电机控制协议

### 2.1 模块信息

- **Module ID**: `0x01`
- **Target**: 电机 ID (1-6)

### 2.2 命令列表

| 命令码 | 命令名称 | 功能描述 |
|--------|----------|----------|
| 0x00   | READ_STATUS | 读取电机状态 |
| 0x20   | READ_STATUS_ALT | 读取电机状态 (备用) |
| 0x30   | SET_MODE | 设置电机模式 |
| 0x31   | SET_POSITION | 设置目标位置 |
| 0x32   | SET_SPEED | 设置速度和目标位置 |
| 0x33   | SET_FORCE | 设置目标力 |

### 2.3 电机模式

| 模式值 | 模式名称 | 描述 |
|--------|----------|------|
| 0x00   | POSITION | 位置控制模式 |
| 0x01   | SERVO    | 伺服模式 (高频控制, ≥20ms 更新) |
| 0x02   | SPEED    | 速度控制模式 |
| 0x03   | FORCE    | 力控制模式 |

### 2.4 寄存器地址

| 寄存器 | 地址 | 数据类型 | 范围 | 描述 |
|--------|------|----------|------|------|
| MODE   | 0x25 | uint8_t  | 0-3  | 电机模式 |
| FORCE  | 0x27 | uint16_t | 0-4095 | 目标力 |
| SPEED  | 0x28 | uint16_t | -    | 速度值 |
| POSITION | 0x29 | uint16_t | 0-2000 | 目标位置 |

---

## 3. 命令详细说明

### 3.1 读取电机状态 (0x00)

**请求数据包** (15 字节):

```
Offset | Field    | Value | Description
-------|----------|-------|-------------
0      | Header   | 0xEE  | 固定头
1-2    | Length   | 0x0F 0x00 | 长度 15 (小端序)
3      | Module   | 0x01  | 电机模块
4      | Target   | 0x01-0x06 | 电机 ID
5      | Command  | 0x00  | 读取状态命令
6-7    | Body     | (空)  | 无参数
8-9    | Checksum | XX XX | 校验和
10     | Tail     | 0x7E  | 固定尾
```

**响应数据包** (25 字节):

```
Offset | Field    | Size | Type    | Description
-------|----------|------|---------|-------------
0      | Header   | 1    | uint8_t | 0xEE
1-2    | Length   | 2    | uint16_t| 25
3      | Module   | 1    | uint8_t | 0x01
4      | Target   | 1    | uint8_t | 电机 ID
5      | Command  | 1    | uint8_t | 0x00
6-7    | Position | 2    | uint16_t| 当前位置 [0, 2000]
8-9    | Velocity | 2    | int16_t | 当前速度 (steps/s)
10-11  | Force    | 2    | uint16_t| 当前力 [0, 4095]
12     | Mode     | 1    | uint8_t | 当前模式
13     | State    | 1    | uint8_t | 状态标志
14-15  | Checksum | 2    | uint16_t| 校验和
16     | Tail     | 1    | uint8_t | 0x7E
```

**示例**:

```
请求: EE 0F 00 01 01 00 00 00 7E
      ^^       ^^    ^^
      头       电机1  读状态

响应: EE 19 00 01 01 00 E8 03 00 00 00 08 00 00 XX XX 7E
                        ^^^^^ ^^^^^ ^^^^^ ^^ ^^
                        位置  速度  力    模式 状态
```

### 3.2 设置电机模式 (0x30)

**请求数据包** (17 字节):

```
Offset | Field    | Value | Description
-------|----------|-------|-------------
0      | Header   | 0xEE  | 固定头
1-2    | Length   | 0x11 0x00 | 长度 17
3      | Module   | 0x01  | 电机模块
4      | Target   | 0x01-0x06 | 电机 ID
5      | Command  | 0x30  | 设置模式命令
6      | Register | 0x25  | 模式寄存器地址
7-8    | Value    | XX XX | 模式值 (小端序)
9-10   | Checksum | XX XX | 校验和
11     | Tail     | 0x7E  | 固定尾
```

**示例 - 设置电机 1 为伺服模式**:

```
请求: EE 11 00 01 01 30 25 01 00 XX XX 7E
                        ^^ ^^^^^
                        寄存器 模式=1
```

### 3.3 设置电机位置 (0x31)

**请求数据包** (17 字节):

```
Offset | Field    | Value | Description
-------|----------|-------|-------------
0      | Header   | 0xEE  | 固定头
1-2    | Length   | 0x11 0x00 | 长度 17
3      | Module   | 0x01  | 电机模块
4      | Target   | 0x01-0x06 | 电机 ID
5      | Command  | 0x31  | 设置位置命令
6      | Register | 0x29  | 位置寄存器地址
7-8    | Position | XX XX | 目标位置 [0, 2000] (小端序)
9-10   | Checksum | XX XX | 校验和
11     | Tail     | 0x7E  | 固定尾
```

**示例 - 设置电机 2 到位置 1000**:

```
请求: EE 11 00 01 02 31 29 E8 03 XX XX 7E
                        ^^ ^^^^^
                        寄存器 位置=1000
```

### 3.4 设置电机力 (0x33)

**请求数据包** (17 字节):

```
Offset | Field    | Value | Description
-------|----------|-------|-------------
0      | Header   | 0xEE  | 固定头
1-2    | Length   | 0x11 0x00 | 长度 17
3      | Module   | 0x01  | 电机模块
4      | Target   | 0x01-0x06 | 电机 ID
5      | Command  | 0x33  | 设置力命令
6      | Register | 0x27  | 力寄存器地址
7-8    | Force    | XX XX | 目标力 [0, 4095] (小端序)
9-10   | Checksum | XX XX | 校验和
11     | Tail     | 0x7E  | 固定尾
```

### 3.5 设置电机速度 (0x32)

**请求数据包** (19 字节):

```
Offset | Field    | Value | Description
-------|----------|-------|-------------
0      | Header   | 0xEE  | 固定头
1-2    | Length   | 0x13 0x00 | 长度 19
3      | Module   | 0x01  | 电机模块
4      | Target   | 0x01-0x06 | 电机 ID
5      | Command  | 0x32  | 设置速度命令
6      | Register1| 0x28  | 速度寄存器地址
7-8    | Speed    | XX XX | 速度值 (小端序)
9      | Register2| 0x29  | 位置寄存器地址
10-11  | Position | XX XX | 目标位置 (小端序)
12-13  | Checksum | XX XX | 校验和
14     | Tail     | 0x7E  | 固定尾
```

---

## 4. 编码器协议

### 4.1 模块信息

- **Module ID**: `0x02`
- **Target**: `0x00` (读取所有通道)

### 4.2 读取所有编码器 (0x00)

**请求数据包** (9 字节):

```
Offset | Field    | Value | Description
-------|----------|-------|-------------
0      | Header   | 0xEE  | 固定头
1-2    | Length   | 0x09 0x00 | 长度 9
3      | Module   | 0x02  | 编码器模块
4      | Target   | 0x00  | 所有通道
5      | Command  | 0x00  | 读取命令
6-7    | Checksum | XX XX | 校验和
8      | Tail     | 0x7E  | 固定尾
```

**响应数据包** (39 字节):

```
Offset | Field    | Size | Type    | Description
-------|----------|------|---------|-------------
0      | Header   | 1    | uint8_t | 0xEE
1-2    | Length   | 2    | uint16_t| 39
3      | Module   | 1    | uint8_t | 0x02
4      | Target   | 1    | uint8_t | 0x00
5      | Command  | 1    | uint8_t | 0x00
6-9    | CH0      | 4    | float   | 通道 0 电压 [0, 4V]
10-13  | CH1      | 4    | float   | 通道 1 电压
...    | ...      | ...  | ...     | ...
70-73  | CH15     | 4    | float   | 通道 15 电压
74-75  | Checksum | 2    | uint16_t| 校验和
76     | Tail     | 1    | uint8_t | 0x7E
```

**电压到角度转换**:

```
angle_degrees = (voltage / 4.0) * 360.0
```

其中:
- 0V → 0°
- 4V → 360°

---

## 5. IMU 协议

### 5.1 模块信息

- **Module ID**: `0x03`
- **Target**: `0x00`

### 5.2 读取 IMU 数据 (0x00)

**请求数据包** (7 字节):

```
Offset | Field    | Value | Description
-------|----------|-------|-------------
0      | Header   | 0xEE  | 固定头
1-2    | Length   | 0x07 0x00 | 长度 7
3      | Module   | 0x03  | IMU 模块
4      | Target   | 0x00  |
5      | Command  | 0x00  | 读取命令
6-7    | Checksum | XX XX | 校验和
8      | Tail     | 0x7E  | 固定尾
```

**响应数据包** (19 字节):

```
Offset | Field    | Size | Type    | Description
-------|----------|------|---------|-------------
0      | Header   | 1    | uint8_t | 0xEE
1-2    | Length   | 2    | uint16_t| 19
3      | Module   | 1    | uint8_t | 0x03
4      | Target   | 1    | uint8_t | 0x02 (固件 bug, 应为 0x03)
5      | Command  | 1    | uint8_t | 0x00
6-9    | Roll     | 4    | float   | 横滚角 (度)
10-13  | Pitch    | 4    | float   | 俯仰角 (度)
14-17  | Yaw      | 4    | float   | 偏航角 (度)
18-19  | Checksum | 2    | uint16_t| 校验和
20     | Tail     | 1    | uint8_t | 0x7E
```

**注意**: 固件存在 bug,响应中的 Target 字段显示为 `0x02` 而非 `0x03`,解析时应忽略此字段,仅检查 Module ID。

---

## 6. 错误处理

### 6.1 超时

- **发送超时**: 默认 100ms
- **接收超时**: 默认 100ms

超时后返回错误,不重试。

### 6.2 校验和错误

如果接收到的数据包校验和不匹配:
1. 丢弃该数据包
2. 增加 `checksum_errors` 计数
3. 返回错误

### 6.3 数据包格式错误

如果数据包格式不正确 (Header/Tail 错误, 长度不匹配等):
1. 丢弃该数据包
2. 增加 `receive_errors` 计数
3. 返回错误

---

## 7. 性能指标

### 7.1 延迟

- **单次请求-响应**: 约 5-20ms (取决于网络)
- **批量操作**: 6 个电机约 30-120ms

### 7.2 吞吐量

- **最大更新频率**: 100Hz (10ms 间隔)
- **伺服模式**: 50Hz (20ms 间隔,协议要求)

### 7.3 可靠性

- **目标成功率**: >99%
- **可接受丢包率**: <1%

---

## 8. 协议示例

### 8.1 完整交互示例

**场景**: 设置电机 1 到位置 1000,然后读取状态

```
1. 设置位置命令
   发送: EE 11 00 01 01 31 29 E8 03 XX XX 7E
   接收: EE 0D 00 01 01 31 XX XX 7E (确认)

2. 读取状态命令
   发送: EE 0F 00 01 01 00 XX XX 7E
   接收: EE 19 00 01 01 00 E8 03 00 00 00 08 00 00 XX XX 7E
         位置=1000 ^^^^^ 速度=0 ^^^^^ 力=2048 ^^^^^
```

### 8.2 伺服模式示例

**场景**: 50Hz 伺服控制循环

```cpp
// 设置伺服模式
send_packet(set_mode(motor_id=1, mode=SERVO));

// 50Hz 控制循环
while (running) {
    auto start = now();

    // 计算目标位置
    uint16_t target = calculate_target();

    // 发送位置命令
    send_packet(set_position(motor_id=1, position=target));

    // 等待 20ms
    sleep_until(start + 20ms);
}
```

---

## 9. 调试技巧

### 9.1 使用 Wireshark 抓包

```bash
sudo wireshark -i eth0 -f "udp port 8080"
```

### 9.2 手动发送测试包

```bash
# 使用 netcat 发送十六进制数据
echo -ne '\xEE\x0F\x00\x01\x01\x00\x00\x00\x7E' | nc -u 192.168.10.123 8080
```

### 9.3 校验和计算工具

```python
def calculate_checksum(data):
    checksum = sum(data[:-3]) & 0xFFFF
    return checksum.to_bytes(2, 'little')
```

---

## 10. 参考资料

- **原始文档**: WGS 通讯协议测试文档 (Feishu)
- **SDK 实现**: `src/protocol.cpp`
- **测试代码**: `tests/test_protocol.cpp`

---

**版本**: 1.0
**最后更新**: 2026-02-27
**维护者**: Joyson Glove SDK Team
