# OSRCORE 开发板 — ESP-IDF 示例集

面向 **OSRCORE** 开发板（ESP32-S3）的渐进式 ESP-IDF 示例集，结构参照 RoboMaster C 型开发板教程系列。

## 硬件概览

| 组件 | 规格 |
|------|------|
| 主控 | ESP32-S3-WROOM-1U N8R16（240 MHz，8 MB Flash，16 MB PSRAM） |
| IMU | QMI8658 六轴（I2C，地址 0x6B） |
| 磁力计 | QMC6309 三轴（I2C，地址 0x7C） |
| LED | WS2812B RGB × 1（GPIO45，RMT） |
| 蜂鸣器 | 无源蜂鸣器（GPIO42，LEDC） |
| 电调 / 油门 | RC PWM 输出（GPIO1，LEDC 50 Hz） |
| 转向 / 舵机 | RC PWM 输出（GPIO2，LEDC 50 Hz） |
| 编码器 | 正交编码器（GPIO3 CLK / GPIO9 DIR，PCNT） |
| 遥控接收机 | SBUS（UART0，GPIO44 RX，信号反相） |
| 电池 ADC | 分压采样（GPIO4，ADC1） |
| 电池使能 | MOS 开关（GPIO16，高电平使能） |
| 控制台 | USB CDC（USB-JTAG 口，无需额外 UART） |
| USB Hub | CH339F（Type-C 16P，扩展 4× USB） |
| 供电 | 9–26 V 电池 → TPS54540 5 V / AMS1117 3.3 V |
| 扩展接口 | 20 针排针（SPI、I2C、UART、GPIO、5 V、3.3 V） |
| CAN | CAN 总线收发器（预留） |
| MicroSD | SPI 卡槽（预留） |

### 引脚定义

| 信号 | GPIO | 备注 |
|------|------|------|
| 油门（电调） | 1 | LEDC CH0，50 Hz，14-bit |
| 转向（舵机） | 2 | LEDC CH1，50 Hz，14-bit |
| 编码器 CLK（EA） | 3 | PCNT 边沿输入 |
| 电池 ADC | 4 | ADC1 CH3，分压电路 |
| 编码器 DIR（EB） | 9 | PCNT 电平输入 |
| I2C SDA | 10 | QMI8658 + QMC6309 |
| I2C SCL | 11 | 400 kHz |
| 电池使能 | 16 | 高电平使能 MOS |
| 蜂鸣器 | 42 | LEDC CH2，TIMER1 |
| SBUS RX | 44 | UART0，100 kbaud，8E2，反相 |
| SBUS TX | 43 | UART0 TX（反相） |
| WS2812B | 45 | RMT TX |

## 示例列表

| # | 目录 | 核心 API | 演示内容 |
|---|------|---------|---------|
| 01 | [01_blink_led](01_blink_led/) | RMT TX | WS2812B RGB LED 颜色循环，自定义 RMT 编码器 |
| 02 | [02_buzzer](02_buzzer/) | LEDC | 无源蜂鸣器旋律，动态调频 |
| 03 | [03_pwm_servo](03_pwm_servo/) | LEDC | 电调解锁 + 舵机扫描（1000–2000 µs） |
| 04 | [04_sbus_rc](04_sbus_rc/) | UART | SBUS 帧解码，16 通道，failsafe 检测 |
| 05 | [05_imu_i2c](05_imu_i2c/) | I2C master | QMI8658 加速度/陀螺仪/温度寄存器读取 |
| 06 | [06_encoder](06_encoder/) | PCNT | 正交编码器溢出累积，速度计算 |
| 07 | [07_nvs](07_nvs/) | NVS Flash | PID 参数掉电保存与恢复 |
| 08 | [08_freertos](08_freertos/) | FreeRTOS | 双核任务绑定、Queue、Mutex |
| 09 | [09_pid_motor](09_pid_motor/) | PCNT + LEDC | 速度闭环：编码器 → PID → 电调 |
| 10 | [10_ahrs](10_ahrs/) | I2C + math | Madgwick 6-DOF AHRS，四元数 → 欧拉角 |
| 11 | [11_full_example](11_full_example/) | 全部 | 完整机器人：三任务双核，SBUS + 串口控制 |

## 工具链

| 工具 | 版本 |
|------|------|
| ESP-IDF | v5.3+ |
| 目标芯片 | `esp32s3` |
| 编译器 | xtensa-esp32s3-elf-gcc（IDF 内置） |
| 控制台 | USB CDC（USB-JTAG 口，Linux/Mac 免驱） |

## 快速上手

```bash
# 1. 安装 ESP-IDF（如未安装）
#    https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32s3/get-started/

# 2. 克隆本仓库
git clone https://github.com/osrbot/Development-Board-OSRCORE-Example.git
cd Development-Board-OSRCORE-Example

# 3. 编译并烧录任意示例
cd 01_blink_led
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

> 首次编译时，`idf.py set-target esp32s3` 会从 `sdkconfig.defaults` 生成 `sdkconfig`。每个示例均已预置适配 OSRCORE 硬件的 `sdkconfig.defaults`。

## 教程文档

完整中文教程（对标 RoboMaster C 型开发板教程 PDF 结构）：

**[docs/tutorial_zh.md](docs/tutorial_zh.md)**

| 章节 | 内容 |
|------|------|
| 第 0 章 | OSRCORE 开发板与 ESP-IDF 入门 |
| 第 1 章 | WS2812B LED 控制（RMT） |
| 第 2 章 | 蜂鸣器（LEDC） |
| 第 3 章 | PWM 舵机与电调（LEDC） |
| 第 4 章 | SBUS 遥控接收（UART） |
| 第 5 章 | QMI8658 IMU 读取（I2C） |
| 第 6 章 | 正交编码器测速（PCNT） |
| 第 7 章 | NVS 参数持久化 |
| 第 8 章 | FreeRTOS 多任务架构 |
| 第 9 章 | PID 电机速度闭环控制 |
| 第 10 章 | Madgwick AHRS 姿态解算 |
| 第 11 章 | 完整机器人示例 |

## 许可证

MIT
