# OSRCORE Development Board — ESP-IDF Examples

Progressive ESP-IDF example collection for the **OSRCORE** development board (ESP32-S3), structured as a step-by-step embedded tutorial series.

## Hardware Overview

| Item | Spec |
|------|------|
| MCU | ESP32-S3-WROOM-1U N8R16 (240 MHz, 8 MB Flash, 16 MB PSRAM) |
| IMU | QMI8658 6-DOF (I2C, addr 0x6B) |
| Magnetometer | QMC6309 3-axis (I2C, addr 0x7C) |
| LED | WS2812B RGB × 1 (GPIO45, RMT) |
| Buzzer | Passive buzzer (GPIO42, LEDC) |
| ESC / Throttle | RC PWM output (GPIO1, LEDC 50 Hz) |
| Steering / Servo | RC PWM output (GPIO2, LEDC 50 Hz) |
| Encoder | Quadrature (GPIO3 CLK / GPIO9 DIR, PCNT) |
| RC Receiver | SBUS (UART0, GPIO44 RX, inverted) |
| Battery ADC | Voltage divider (GPIO4, ADC1) |
| Battery Enable | MOS switch (GPIO16) |
| Console | USB CDC via USB-JTAG (no extra UART needed) |
| USB Hub | CH339F (Type-C 16P, expands to 4× USB) |
| Power | 9–26 V battery in → TPS54540 5 V / AMS1117 3.3 V |
| Expansion | 20-pin header (SPI, I2C, UART, GPIO, 5 V, 3.3 V) |
| CAN | CAN bus transceiver (reserved) |
| MicroSD | SPI slot (reserved) |

### Pin Map

| Signal | GPIO | Notes |
|--------|------|-------|
| Throttle (ESC) | 1 | LEDC CH0, 50 Hz, 14-bit |
| Steering (Servo) | 2 | LEDC CH1, 50 Hz, 14-bit |
| Encoder CLK (EA) | 3 | PCNT edge input |
| Battery ADC | 4 | ADC1 CH3, voltage divider |
| Encoder DIR (EB) | 9 | PCNT level input |
| I2C SDA | 10 | QMI8658 + QMC6309 |
| I2C SCL | 11 | 400 kHz |
| Battery Enable | 16 | Active-high MOS gate |
| Buzzer | 42 | LEDC CH2, TIMER1 |
| SBUS RX | 44 | UART0, 100 kbaud, 8E2, inverted |
| SBUS TX | 43 | UART0 TX (inverted) |
| WS2812B | 45 | RMT TX |

## Examples

| # | Directory | Key API | What it demonstrates |
|---|-----------|---------|----------------------|
| 01 | [01_blink_led](01_blink_led/) | RMT TX | WS2812B RGB LED color cycling with custom encoder |
| 02 | [02_buzzer](02_buzzer/) | LEDC | Passive buzzer melody using frequency control |
| 03 | [03_pwm_servo](03_pwm_servo/) | LEDC | ESC arming + servo sweep (1000–2000 µs) |
| 04 | [04_sbus_rc](04_sbus_rc/) | UART | SBUS frame decode, 16-channel, failsafe detection |
| 05 | [05_imu_i2c](05_imu_i2c/) | I2C master | QMI8658 accel/gyro/temp readout via register map |
| 06 | [06_encoder](06_encoder/) | PCNT | Quadrature encoder with overflow accumulation, speed calc |
| 07 | [07_nvs](07_nvs/) | NVS Flash | PID parameter persistence across reboots |
| 08 | [08_freertos](08_freertos/) | FreeRTOS | Dual-core task pinning, Queue, Mutex |
| 09 | [09_pid_motor](09_pid_motor/) | PCNT + LEDC | Closed-loop speed control: encoder → PID → ESC |
| 10 | [10_ahrs](10_ahrs/) | I2C + math | Madgwick 6-DOF AHRS, quaternion → Euler angles |
| 11 | [11_full_example](11_full_example/) | all | Complete robot: 3-task dual-core, SBUS + serial control |

## Toolchain

| Tool | Version |
|------|---------|
| ESP-IDF | v5.3+ |
| Target | `esp32s3` |
| Compiler | xtensa-esp32s3-elf-gcc (bundled with IDF) |
| Console | USB CDC (USB-JTAG port, no driver needed on Linux/Mac) |

## Quick Start

```bash
# 1. Install ESP-IDF (if not already)
#    https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/

# 2. Clone this repo
git clone https://github.com/osrbot/Development-Board-OSRCORE-Example.git
cd Development-Board-OSRCORE-Example

# 3. Build and flash any example
cd 01_blink_led
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

> On first build, `idf.py set-target esp32s3` generates `sdkconfig` from `sdkconfig.defaults`. Each example ships its own `sdkconfig.defaults` pre-configured for the OSRCORE board.

## Tutorial Document

A full Chinese tutorial document covering all 12 chapters is available at:

**[docs/tutorial_zh.md](docs/tutorial_zh.md)**

Chapters:

| # | Chapter |
|---|---------|
| 0 | OSRCORE 开发板与 ESP-IDF 入门 |
| 1 | WS2812B LED 控制（RMT） |
| 2 | 蜂鸣器（LEDC） |
| 3 | PWM 舵机与电调（LEDC） |
| 4 | SBUS 遥控接收（UART） |
| 5 | QMI8658 IMU 读取（I2C） |
| 6 | 正交编码器测速（PCNT） |
| 7 | NVS 参数持久化 |
| 8 | FreeRTOS 多任务架构 |
| 9 | PID 电机速度闭环控制 |
| 10 | Madgwick AHRS 姿态解算 |
| 11 | 完整机器人示例 |

## License

MIT
