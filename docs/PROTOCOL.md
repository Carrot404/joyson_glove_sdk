# Joyson Glove SDK - Protocol Specification

## 通讯协议规范

本文档详细描述了 Joyson 外骨骼手套的 UDP 通讯协议。

---

## 1. 协议概述

### 1.1 基本信息

- **传输协议**: UDP
- **服务器地址**: 192.168.10.123
- **服务器端口**: 8080
- **字节序**: 混合字节序 (详见各模块说明)
  - 电机数据 (uint16_t): Little-Endian
  - 编码器 ADC 值 (uint16_t): **Big-Endian**
  - IMU 浮点数 (float32): **Big-Endian**
- **校验方式**: 累加和 (Sum Checksum)

### 1.2 数据包格式

所有数据包遵循统一的格式:

```
+--------+--------+--------+--------+--------+--------+--------+--------+
| Header | Length | Module | Target | Command|  Body  |Checksum|  Tail  |
|  (1B)  |  (1B)  | ID(1B) |  (1B)  |  (1B)  |(N bytes)|  (1B) |  (1B)  |
+--------+--------+--------+--------+--------+--------+--------+--------+
  0xEE               0x01                                         0x7E
```

**字段说明**:
- **Header**: 固定值 `0xEE`
- **Length**: 从 Target 到 Body 末尾的数据字节数 (不含 Header、Length、Module ID、Checksum、Tail), 1 字节, 即数据包总长度 - 5
- **Module ID**: 当前模块 ID, 通常为 `0x01`
- **Target**: 板子上的模块标识
  - `0x01` - 电机 (Motor)
  - `0x02` - 编码器 ADC (Encoder)
  - `0x03` - IMU
  - `0x04` - LRA
- **Command**: 命令码
- **Body**: 命令参数或响应数据 (长度可变)
- **Checksum**: 校验和, 1 字节
- **Tail**: 固定值 `0x7E`

### 1.3 校验和计算

**外层包校验和**:

```cpp
uint8_t checksum = 0;
for (size_t i = 0; i < packet_length - 2; ++i) {  // excluding checksum and tail
    checksum += packet[i];
}
checksum = checksum & 0xFF;
```

**电缸内层包校验和** (不含帧头 0x55 0xAA):

```cpp
uint8_t motor_checksum = 0;
for (size_t i = 2; i < inner_packet_length - 1; ++i) {  // excluding header(2B) and checksum
    motor_checksum += inner_packet[i];
}
motor_checksum = motor_checksum & 0xFF;
```

---

## 2. 电机控制协议

### 2.1 模块信息

- **Module ID**: `0x01` (固定)
- **Target**: `0x01` (电机模块)
- **Motor ID**: 1-6 (在内层电缸子协议中指定)

### 2.2 电缸内层子协议

电机命令的 Body 字段包含一个内嵌的电缸子协议数据包:

**请求帧格式**:

```
+--------+--------+--------+--------+--------+--------+--------+
| Motor Header    |  Len   |Motor ID|  Data  | Motor  |
|  0x55  |  0xAA  |  (1B)  |  (1B)  |(N bytes)|Checksum|
+--------+--------+--------+--------+--------+--------+
```

**响应帧格式**:

```
+--------+--------+--------+--------+--------+--------+--------+
| Motor Header    |  Len   |Motor ID|  Data  | Motor  |
|  0xAA  |  0x55  |  (1B)  |  (1B)  |(N bytes)|Checksum|
+--------+--------+--------+--------+--------+--------+
```

**字段说明**:
- **Motor Header**: 请求 `0x55 0xAA`, 响应 `0xAA 0x55`
- **Len**: 从 Motor ID (不含) 到 Motor Checksum (不含) 的数据字节数
- **Motor ID**: 电机编号 1-6
- **Data**: 命令参数或响应数据 (长度可变)
- **Motor Checksum**: 校验和 = (Len + Motor ID + Data) 累加和 & 0xFF

### 2.3 命令列表

| 命令码 | 命令名称 | 功能描述 |
|--------|----------|----------|
| 0x10   | READ_MOTOR_ID | 读取电机 ID |
| 0x20   | READ_STATUS | 读取电机状态 |
| 0x30   | SET_MODE | 设置电机模式 |
| 0x31   | SET_POSITION | 设置目标位置 |
| 0x32   | SET_SPEED | 设置速度和目标位置 |
| 0x33   | SET_FORCE | 设置目标力 |

### 2.4 电机模式

| 模式值 | 模式名称 | 描述 |
|--------|----------|------|
| 0x00   | POSITION | 位置控制模式 |
| 0x01   | SERVO    | 伺服模式 (高频控制, ≥20ms 更新) |
| 0x02   | SPEED    | 速度控制模式 |
| 0x03   | FORCE    | 力控制模式 |
| 0x04   | VOLTAGE  | 电压控制模式 |
| 0x05   | SPEED_FORCE | 速度力控模式 |

### 2.5 寄存器地址

| 寄存器 | 地址 | 数据类型 | 范围 | 描述 |
|--------|------|----------|------|------|
| MODE   | 0x25 | uint8_t  | 0-3  | 电机模式 |
| FORCE  | 0x27 | uint16_t | 0-4095 | 目标力 |
| SPEED  | 0x28 | uint16_t | -    | 速度值 |
| POSITION | 0x29 | uint16_t | 0-2000 | 目标位置 |

---

## 3. 命令详细说明

### 3.0 读取电机 ID (0x10)

**请求数据包** (16 字节):

```
Offset | Field       | Value     | Description
-------|-------------|-----------|-------------
0      | Header      | 0xEE      | 固定头
1      | Length      | 0x0B      | 长度 (16 - 5 = 11)
2      | Module ID   | 0x01      | 模块 ID
3      | Target      | 0x01      | 电机模块
4      | Command     | 0x10      | 读取电机 ID 命令
5-6    | Motor Header| 0x55 0xAA | 电缸帧头
7      | Motor Len   | 0x04      | 电缸数据长度
8      | Motor ID    | 0x01-0x06 | 电机编号
9      | Instr Type  | 0x31      | 指令类型
10     | Reg Addr Lo | 0x16      | 寄存器地址 (低字节)
11     | Reg Addr Hi | 0x00      | 寄存器地址 (高字节)
12     | Reg Count   | 0x01      | 寄存器数量
13     | Motor Chk   | XX        | 电缸校验和
14     | Checksum    | XX        | 外层校验和
15     | Tail        | 0x7E      | 固定尾
```

**示例 - 读取电机 1 ID**:

```
请求: EE 0B 01 01 10 55 AA 04 01 31 16 00 01 4D A4 7E
      ^^          ^^ ^^       ^^ ^^ ^^ ^^^^^ ^^
      头        Target Cmd  Len MotID 指令 寄存器  数量
```

**响应数据包** (15 字节):

```
Offset | Field       | Size | Type     | Description
-------|-------------|------|----------|-------------
0      | Header      | 1    | uint8_t  | 0xEE
1      | Length      | 1    | uint8_t  | 0x0A (10)
2      | Module ID   | 1    | uint8_t  | 0x01
3-4    | Motor Header| 2    | uint8_t[]| 0xAA 0x55 (响应帧头)
5      | Motor Len   | 1    | uint8_t  | 0x05
6      | Motor ID    | 1    | uint8_t  | 电机编号
7      | Instr Type  | 1    | uint8_t  | 0x31 (指令类型)
8      | Reg Addr Lo | 1    | uint8_t  | 0x16 (寄存器地址低字节)
9      | Reg Addr Hi | 1    | uint8_t  | 0x00 (寄存器地址高字节)
10     | Reg Value Lo| 1    | uint8_t  | 寄存器数值 (低字节)
11     | Reg Value Hi| 1    | uint8_t  | 寄存器数值 (高字节)
12     | Motor Chk   | 1    | uint8_t  | 电缸校验和
13     | Checksum    | 1    | uint8_t  | 外层校验和
14     | Tail        | 1    | uint8_t  | 0x7E
```

**示例 - 读取电机 1 ID 响应** (假设寄存器值为 0x0001):

```
响应: EE 0A 01 AA 55 05 01 31 16 00 01 00 4E 94 7E
      ^^       ^^^^^    ^^ ^^ ^^ ^^^^^ ^^^^^
      头      响应帧头 MotID 指令 寄存器  数值=1
```

### 3.1 读取电机状态 (0x20)

**请求数据包** (15 字节):

```
Offset | Field       | Value     | Description
-------|-------------|-----------|-------------
0      | Header      | 0xEE      | 固定头
1      | Length      | 0x0A      | 长度 (15 - 5 = 10)
2      | Module ID   | 0x01      | 模块 ID
3      | Target      | 0x01      | 电机模块
4      | Command     | 0x20      | 读取电机状态命令
5-6    | Motor Header| 0x55 0xAA | 电缸帧头
7      | Motor Len   | 0x03      | 电缸数据长度
8      | Motor ID    | 0x01-0x06 | 电机编号
9      | Instr Type  | 0x30      | 上位机指令
10     | Reg Addr Lo | 0x00      | 寄存器地址 (低字节)
11     | Reg Addr Hi | 0x00      | 寄存器地址 (高字节)
12     | Motor Chk   | XX        | 电缸校验和
13     | Checksum    | XX        | 外层校验和
14     | Tail        | 0x7E      | 固定尾
```

**响应数据包** (25 字节):

```
Offset | Field       | Size | Type     | Description
-------|-------------|------|----------|-------------
0      | Header      | 1    | uint8_t  | 0xEE
1      | Length      | 1    | uint8_t  | 0x14 (20)
2      | Module ID   | 1    | uint8_t  | 0x01
3-4    | Motor Header| 2    | uint8_t[]| 0xAA 0x55 (响应帧头)
5      | Motor Len   | 1    | uint8_t  | 0x0F (15)
6      | Motor ID    | 1    | uint8_t  | 电机编号
7      | Instr Type  | 1    | uint8_t  | 0x30 (指令类型)
8-9    | Reserved    | 2    | uint8_t[]| 0x00 0x00 (保留帧)
10-11  | Target Pos  | 2    | uint16_t | 目标位置 (小端序)
12-13  | Actual Pos  | 2    | uint16_t | 实际位置 (小端序)
14-15  | Actual Cur  | 2    | uint16_t | 实际电流 (小端序)
16-17  | Force Value | 2    | uint16_t | 力传感器数值 (小端序)
18-19  | Force Raw   | 2    | uint16_t | 力传感器原始值 (小端序)
20     | Temperature | 1    | uint8_t  | 温度
21     | Fault Code  | 1    | uint8_t  | 故障码
22     | Motor Chk   | 1    | uint8_t  | 电缸校验和
23     | Checksum    | 1    | uint8_t  | 外层校验和
24     | Tail        | 1    | uint8_t  | 0x7E
```

**示例** (读取电机 1 状态):

```
请求: EE 0A 01 01 20 55 AA 03 01 30 00 00 34 81 7E
      ^^          ^^ ^^       ^^ ^^ ^^ ^^^^^
      头        Target Cmd  Len MotID 指令 寄存器
```

### 3.2 设置电机模式 (0x30)

**请求数据包** (17 字节):

```
Offset | Field       | Value     | Description
-------|-------------|-----------|-------------
0      | Header      | 0xEE      | 固定头
1      | Length      | 0x0C      | 长度 (17 - 5 = 12)
2      | Module ID   | 0x01      | 模块 ID
3      | Target      | 0x01      | 电机模块
4      | Command     | 0x30      | 设置模式命令
5-6    | Motor Header| 0x55 0xAA | 电缸帧头
7      | Motor Len   | 0x05      | 电缸数据长度
8      | Motor ID    | 0x01-0x06 | 电机编号
9      | CMD (R/W)   | 0x32      | 读写指令
10     | Reg Addr Lo | 0x25      | 控制模式寄存器地址 (低字节)
11     | Reg Addr Hi | 0x00      | 控制模式寄存器地址 (高字节)
12     | Mode Lo     | XX        | 模式值 (低字节)
13     | Mode Hi     | XX        | 模式值 (高字节)
14     | Motor Chk   | XX        | 电缸校验和
15     | Checksum    | XX        | 外层校验和
16     | Tail        | 0x7E      | 固定尾
```

**模式值**:
- `0x0000` - 位置控制模式 (POSITION)
- `0x0001` - 伺服模式 (SERVO)
- `0x0002` - 速度控制模式 (SPEED)
- `0x0003` - 力控制模式 (FORCE)
- `0x0004` - 电压控制模式 (VOLTAGE)
- `0x0005` - 速度力控模式 (SPEED_FORCE)

**响应数据包** (25 字节):

```
Offset | Field       | Size | Type     | Description
-------|-------------|------|----------|-------------
0      | Header      | 1    | uint8_t  | 0xEE
1      | Length      | 1    | uint8_t  | 0x14 (20)
2      | Module ID   | 1    | uint8_t  | 0x01
3-4    | Motor Header| 2    | uint8_t[]| 0xAA 0x55 (响应帧头)
5      | Motor Len   | 1    | uint8_t  | 0x0F (15)
6      | Motor ID    | 1    | uint8_t  | 电机编号
7      | CMD (R/W)   | 1    | uint8_t  | 读写指令
8-9    | Reg Addr    | 2    | uint16_t | 控制模式寄存器地址 (小端序)
10-11  | Target Pos  | 2    | uint16_t | 目标位置 (小端序)
12-13  | Actual Pos  | 2    | uint16_t | 实际位置 (小端序)
14-15  | Actual Cur  | 2    | uint16_t | 实际电流 (小端序)
16-17  | Force Value | 2    | uint16_t | 力传感器数值 (小端序)
18-19  | Force Raw   | 2    | uint16_t | 力传感器原始值 (小端序)
20     | Temperature | 1    | uint8_t  | 温度
21     | Fault Code  | 1    | uint8_t  | 故障码
22     | Motor Chk   | 1    | uint8_t  | 电缸校验和
23     | Checksum    | 1    | uint8_t  | 外层校验和
24     | Tail        | 1    | uint8_t  | 0x7E
```

**示例 - 设置电机 1 为位置控制模式**:

```
请求: EE 0C 01 01 30 55 AA 05 01 32 25 00 00 00 5D E5 7E
      ^^          ^^ ^^       ^^ ^^ ^^ ^^^^^ ^^^^^
      头        Target Cmd  Len MotID CMD 寄存器  模式=0
```

### 3.3 设置电机位置 (0x31)

**请求数据包** (17 字节):

```
Offset | Field       | Value     | Description
-------|-------------|-----------|-------------
0      | Header      | 0xEE      | 固定头
1      | Length      | 0x0C      | 长度 (17 - 5 = 12)
2      | Module ID   | 0x01      | 模块 ID
3      | Target      | 0x01      | 电机模块
4      | Command     | 0x31      | 设置位置命令
5-6    | Motor Header| 0x55 0xAA | 电缸帧头
7      | Motor Len   | 0x05      | 电缸数据长度
8      | Motor ID    | 0x01-0x06 | 电机编号
9      | CMD (R/W)   | 0x32      | 读写指令
10     | Reg Addr Lo | 0x29      | 位置寄存器地址 (低字节)
11     | Reg Addr Hi | 0x00      | 位置寄存器地址 (高字节)
12     | Position Lo | XX        | 目标位置 (低字节) [0, 2000]
13     | Position Hi | XX        | 目标位置 (高字节)
14     | Motor Chk   | XX        | 电缸校验和
15     | Checksum    | XX        | 外层校验和
16     | Tail        | 0x7E      | 固定尾
```

**响应数据包** (25 字节):

```
Offset | Field       | Size | Type     | Description
-------|-------------|------|----------|-------------
0      | Header      | 1    | uint8_t  | 0xEE
1      | Length      | 1    | uint8_t  | 0x14 (20)
2      | Module ID   | 1    | uint8_t  | 0x01
3-4    | Motor Header| 2    | uint8_t[]| 0xAA 0x55 (响应帧头)
5      | Motor Len   | 1    | uint8_t  | 0x0F (15)
6      | Motor ID    | 1    | uint8_t  | 电机编号
7      | CMD (R/W)   | 1    | uint8_t  | 读写指令
8-9    | Reg Addr    | 2    | uint16_t | 位置寄存器地址 (小端序)
10-11  | Target Pos  | 2    | uint16_t | 目标位置 (小端序)
12-13  | Actual Pos  | 2    | uint16_t | 实际位置 (小端序)
14-15  | Actual Cur  | 2    | uint16_t | 实际电流 (小端序)
16-17  | Force Value | 2    | uint16_t | 力传感器数值 (小端序)
18-19  | Force Raw   | 2    | uint16_t | 力传感器原始值 (小端序)
20     | Temperature | 1    | uint8_t  | 温度
21     | Fault Code  | 1    | uint8_t  | 故障码
22     | Motor Chk   | 1    | uint8_t  | 电缸校验和
23     | Checksum    | 1    | uint8_t  | 外层校验和
24     | Tail        | 1    | uint8_t  | 0x7E
```

**示例 - 设置电机 1 到位置 1000**:

```
请求: EE 0C 01 01 31 55 AA 05 01 32 29 00 E8 03 4C C4 7E
      ^^          ^^ ^^       ^^ ^^ ^^ ^^^^^ ^^^^^
      头        Target Cmd  Len MotID CMD 寄存器 位置=1000
```

### 3.4 设置电机力 (0x33)

**请求数据包** (17 字节):

```
Offset | Field       | Value     | Description
-------|-------------|-----------|-------------
0      | Header      | 0xEE      | 固定头
1      | Length      | 0x0C      | 长度 (17 - 5 = 12)
2      | Module ID   | 0x01      | 模块 ID
3      | Target      | 0x01      | 电机模块
4      | Command     | 0x33      | 设置力命令
5-6    | Motor Header| 0x55 0xAA | 电缸帧头
7      | Motor Len   | 0x05      | 电缸数据长度
8      | Motor ID    | 0x01-0x06 | 电机编号
9      | CMD (R/W)   | 0x32      | 读写指令
10     | Reg Addr Lo | 0x27      | 力寄存器地址 (低字节)
11     | Reg Addr Hi | 0x00      | 力寄存器地址 (高字节)
12     | Force Lo    | XX        | 目标力 (低字节) [0, 4095]
13     | Force Hi    | XX        | 目标力 (高字节)
14     | Motor Chk   | XX        | 电缸校验和
15     | Checksum    | XX        | 外层校验和
16     | Tail        | 0x7E      | 固定尾
```

**响应数据包** (25 字节):

```
Offset | Field       | Size | Type     | Description
-------|-------------|------|----------|-------------
0      | Header      | 1    | uint8_t  | 0xEE
1      | Length      | 1    | uint8_t  | 0x14 (20)
2      | Module ID   | 1    | uint8_t  | 0x01
3-4    | Motor Header| 2    | uint8_t[]| 0xAA 0x55 (响应帧头)
5      | Motor Len   | 1    | uint8_t  | 0x0F (15)
6      | Motor ID    | 1    | uint8_t  | 电机编号
7      | CMD (R/W)   | 1    | uint8_t  | 读写指令
8-9    | Reg Addr    | 2    | uint16_t | 力寄存器地址 (小端序)
10-11  | Target Pos  | 2    | uint16_t | 目标位置 (小端序)
12-13  | Actual Pos  | 2    | uint16_t | 实际位置 (小端序)
14-15  | Actual Cur  | 2    | uint16_t | 实际电流 (小端序)
16-17  | Force Value | 2    | uint16_t | 力传感器数值 (小端序)
18-19  | Force Raw   | 2    | uint16_t | 力传感器原始值 (小端序)
20     | Temperature | 1    | uint8_t  | 温度
21     | Fault Code  | 1    | uint8_t  | 故障码
22     | Motor Chk   | 1    | uint8_t  | 电缸校验和
23     | Checksum    | 1    | uint8_t  | 外层校验和
24     | Tail        | 1    | uint8_t  | 0x7E
```

**示例 - 设置电机 1 目标力为 2048**:

```
请求: EE 0C 01 01 33 55 AA 05 01 32 27 00 00 08 67 FC 7E
      ^^          ^^ ^^       ^^ ^^ ^^ ^^^^^ ^^^^^
      头        Target Cmd  Len MotID CMD 寄存器 力=2048
```

### 3.5 设置电机速度位置 (0x32)

**请求数据包** (19 字节):

```
Offset | Field       | Value     | Description
-------|-------------|-----------|-------------
0      | Header      | 0xEE      | 固定头
1      | Length      | 0x0E      | 长度 (19 - 5 = 14)
2      | Module ID   | 0x01      | 模块 ID
3      | Target      | 0x01      | 电机模块
4      | Command     | 0x32      | 设置速度位置命令
5-6    | Motor Header| 0x55 0xAA | 电缸帧头
7      | Motor Len   | 0x07      | 电缸数据长度
8      | Motor ID    | 0x01-0x06 | 电机编号
9      | CMD (R/W)   | 0x32      | 读写指令
10     | Reg Addr Lo | 0x28      | 速度寄存器地址 (低字节)
11     | Reg Addr Hi | 0x00      | 速度寄存器地址 (高字节)
12     | Speed Lo    | XX        | 目标速度 (低字节)
13     | Speed Hi    | XX        | 目标速度 (高字节)
14     | Position Lo | XX        | 目标位置 (低字节) [0, 2000]
15     | Position Hi | XX        | 目标位置 (高字节)
16     | Motor Chk   | XX        | 电缸校验和
17     | Checksum    | XX        | 外层校验和
18     | Tail        | 0x7E      | 固定尾
```

> **注意**: 速度模式需同时指定目标速度和目标位置，因此比其他设置命令多 2 字节。

**响应数据包** (25 字节):

```
Offset | Field       | Size | Type     | Description
-------|-------------|------|----------|-------------
0      | Header      | 1    | uint8_t  | 0xEE
1      | Length      | 1    | uint8_t  | 0x14 (20)
2      | Module ID   | 1    | uint8_t  | 0x01
3-4    | Motor Header| 2    | uint8_t[]| 0xAA 0x55 (响应帧头)
5      | Motor Len   | 1    | uint8_t  | 0x0F (15)
6      | Motor ID    | 1    | uint8_t  | 电机编号
7      | CMD (R/W)   | 1    | uint8_t  | 读写指令
8-9    | Reg Addr    | 2    | uint16_t | 速度寄存器地址 (小端序)
10-11  | Target Pos  | 2    | uint16_t | 目标位置 (小端序)
12-13  | Actual Pos  | 2    | uint16_t | 实际位置 (小端序)
14-15  | Actual Cur  | 2    | uint16_t | 实际电流 (小端序)
16-17  | Force Value | 2    | uint16_t | 力传感器数值 (小端序)
18-19  | Force Raw   | 2    | uint16_t | 力传感器原始值 (小端序)
20     | Temperature | 1    | uint8_t  | 温度
21     | Fault Code  | 1    | uint8_t  | 故障码
22     | Motor Chk   | 1    | uint8_t  | 电缸校验和
23     | Checksum    | 1    | uint8_t  | 外层校验和
24     | Tail        | 1    | uint8_t  | 0x7E
```

**示例 - 设置电机 1 速度 500, 位置 1000**:

```
请求: EE 0E 01 01 32 55 AA 07 01 32 28 00 F4 01 E8 03 42 B3 7E
      ^^          ^^ ^^       ^^ ^^ ^^ ^^^^^ ^^^^^ ^^^^^
      头        Target Cmd  Len MotID CMD 寄存器 速度=500 位置=1000
```

---

## 4. 编码器协议

### 4.1 模块信息

- **Module ID**: `0x01` (固定)
- **Target**: `0x02` (编码器模块)

### 4.2 读取所有编码器 (0x00)

**请求数据包** (9 字节):

```
Offset | Field       | Value     | Description
-------|-------------|-----------|-------------
0      | Header      | 0xEE      | 固定头
1      | Length      | 0x04      | 长度 (9 - 5 = 4)
2      | Module ID   | 0x01      | 模块 ID
3      | Target      | 0x02      | 编码器模块
4      | Command     | 0x00      | 读取命令
5      | ADC_Index   | 0xFF      | ADC 索引 (0xFF=全部)
6      | ADC_Channel | 0xFF      | ADC 通道 (0xFF=全部)
7      | Checksum    | XX        | 校验和
8      | Tail        | 0x7E      | 固定尾
```

**响应数据包** (39 字节):

```
Offset | Field       | Size | Type     | Description
-------|-------------|------|----------|-------------
0      | Header      | 1    | uint8_t  | 0xEE
1      | Length      | 1    | uint8_t  | 0x22 (34) ⚠️ 见下方 BUG 说明
2      | Module ID   | 1    | uint8_t  | 0x01
3      | Target      | 1    | uint8_t  | 0x02 (编码器模块)
4      | Command     | 1    | uint8_t  | 0x00
5-6    | CH0         | 2    | uint16_t | 通道 0 ADC 值 (大端序)
7-8    | CH1         | 2    | uint16_t | 通道 1 ADC 值 (大端序)
9-10   | CH2         | 2    | uint16_t | 通道 2 ADC 值 (大端序)
11-12  | CH3         | 2    | uint16_t | 通道 3 ADC 值 (大端序)
13-14  | CH4         | 2    | uint16_t | 通道 4 ADC 值 (大端序)
15-16  | CH5         | 2    | uint16_t | 通道 5 ADC 值 (大端序)
17-18  | CH6         | 2    | uint16_t | 通道 6 ADC 值 (大端序)
19-20  | CH7         | 2    | uint16_t | 通道 7 ADC 值 (大端序)
21-22  | CH8         | 2    | uint16_t | 通道 8 ADC 值 (大端序)
23-24  | CH9         | 2    | uint16_t | 通道 9 ADC 值 (大端序)
25-26  | CH10        | 2    | uint16_t | 通道 10 ADC 值 (大端序)
27-28  | CH11        | 2    | uint16_t | 通道 11 ADC 值 (大端序)
29-30  | CH12        | 2    | uint16_t | 通道 12 ADC 值 (大端序)
31-32  | CH13        | 2    | uint16_t | 通道 13 ADC 值 (大端序)
33-34  | CH14        | 2    | uint16_t | 通道 14 ADC 值 (大端序)
35-36  | CH15        | 2    | uint16_t | 通道 15 ADC 值 (大端序)
37     | Checksum    | 1    | uint8_t  | 校验和
38     | Tail        | 1    | uint8_t  | 0x7E
```

> ⚠️ **已知 BUG**: 固件返回的 Length 字段值错误，多加了 3。正确值应为 `0x22` (34)，实际返回 `0x25` (37)。解析时需注意此偏差，待固件修复。

---

## 5. IMU 协议

### 5.1 模块信息

- **Module ID**: `0x01` (固定)
- **Target**: `0x03` (IMU 模块)

### 5.2 读取 IMU 数据 (0x00)

**请求数据包** (7 字节):

```
Offset | Field       | Value     | Description
-------|-------------|-----------|-------------
0      | Header      | 0xEE      | 固定头
1      | Length      | 0x02      | 长度 (7 - 5 = 2)
2      | Module ID   | 0x01      | 模块 ID
3      | Target      | 0x03      | IMU 模块
4      | Command     | 0x00      | 读取命令
5      | Checksum    | XX        | 校验和
6      | Tail        | 0x7E      | 固定尾
```

**响应数据包** (19 字节):

```
Offset | Field       | Size | Type     | Description
-------|-------------|------|----------|-------------
0      | Header      | 1    | uint8_t  | 0xEE
1      | Length      | 1    | uint8_t  | 0x0E (14) ⚠️ 见下方 BUG 说明
2      | Module ID   | 1    | uint8_t  | 0x01
3      | Target      | 1    | uint8_t  | 0x03 (IMU 模块) ⚠️ 实际值可能不正确
4      | Command     | 1    | uint8_t  | 0x00
5-8    | Roll        | 4    | float    | 横滚角 (度, 大端序)
9-12   | Pitch       | 4    | float    | 俯仰角 (度, 大端序)
13-16  | Yaw         | 4    | float    | 偏航角 (度, 大端序)
17     | Checksum    | 1    | uint8_t  | 校验和
18     | Tail        | 1    | uint8_t  | 0x7E
```

> ⚠️ **已知 BUG (1)**: 固件返回的 Length 字段值错误，多加了 3。正确值应为 `0x0E` (14)，实际返回 `0x11` (17)。解析时需注意此偏差，待固件修复。
>
> ⚠️ **已知 BUG (2)**: 响应中的 Target 字段值不正确，解析时建议仅依据请求中的 Target 字段判断模块类型。

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
   发送: EE 0A 01 01 31 55 AA 03 01 29 E8 03 18 5A 7E
         ^^    ^^ ^^ ^^                ^^ ^^ ^^^^^
         头   Mod Tgt Cmd            MotID 寄存器 位置=1000
   接收: EE XX 01 01 31 AA 55 XX XX ... XX XX 7E (确认)

2. 读取状态命令
   发送: EE 07 01 01 00 55 AA 00 01 01 F8 7E
   接收: EE 0F 01 01 00 AA 55 08 01 E8 03 00 00 00 08 00 00 FC F6 7E
                              ^^^^^ 位置=1000 速度=0 力=2048
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
# 使用 netcat 发送读取电机 1 状态命令
echo -ne '\xEE\x07\x01\x01\x00\x55\xAA\x00\x01\x01\xF8\x7E' | nc -u 192.168.10.123 8080
```

### 9.3 校验和计算工具

```python
def calculate_outer_checksum(data):
    """Calculate outer packet checksum (excluding checksum and tail)"""
    checksum = sum(data[:-2]) & 0xFF
    return bytes([checksum])

def calculate_motor_checksum(inner_data):
    """Calculate inner motor packet checksum (excluding header 0x55/0xAA and checksum)"""
    checksum = sum(inner_data[2:-1]) & 0xFF
    return bytes([checksum])
```

---

## 10. 参考资料

- **原始文档**: WGS 通讯协议测试文档 (Feishu)
- **SDK 实现**: `src/protocol.cpp`
- **测试代码**: `tests/test_protocol.cpp`

---

**版本**: 1.0
**最后更新**: 2026-02-28
**维护者**: Joyson Glove SDK Team
