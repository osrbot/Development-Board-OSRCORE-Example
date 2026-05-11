# OSRCORE Development Board ESP-IDF Examples

Progressive ESP-IDF example collection for the OSRCORE development board (ESP32-S3), mirroring the structure of the RoboMaster C-type board tutorial.

## Hardware

| Item | Spec |
|------|------|
| MCU | ESP32-S3-WROOM-1U N8R16 |
| Flash | 8MB QIO 80MHz |
| PSRAM | 16MB Octal 80MHz |
| IMU | QMI8658 (I2C, 0x6B) |
| Magnetometer | QMC6309 (I2C, 0x7C) |
| LED | WS2812B × 1 (GPIO45, RMT) |
| Buzzer | Passive (GPIO42, LEDC) |
| ESC/Throttle | PWM (GPIO1, LEDC 50Hz) |
| Steering/Servo | PWM (GPIO2, LEDC 50Hz) |
| Encoder | Quadrature (GPIO3/GPIO9, PCNT) |
| RC Receiver | SBUS (UART0, GPIO44, inverted) |
| ADC | Battery voltage (GPIO4) |
| Console | USB CDC (USB-JTAG) |

## Examples

| # | Name | Key API | Description |
|---|------|---------|-------------|
| 01 | [blink_led](01_blink_led/) | RMT TX | WS2812B RGB LED color cycling |
| 02 | [buzzer](02_buzzer/) | LEDC | Buzzer melody playback |
| 03 | [pwm_servo](03_pwm_servo/) | LEDC | ESC/servo PWM control |
| 04 | [sbus_rc](04_sbus_rc/) | UART | SBUS RC receiver decoding |
| 05 | [imu_i2c](05_imu_i2c/) | I2C master | QMI8658 accel/gyro readout |
| 06 | [encoder](06_encoder/) | PCNT | Quadrature encoder speed measurement |
| 07 | [nvs](07_nvs/) | NVS Flash | Parameter persistence |
| 08 | [freertos](08_freertos/) | FreeRTOS | Multi-task architecture |
| 09 | [pid_motor](09_pid_motor/) | PCNT+LEDC | PID closed-loop speed control |
| 10 | [ahrs](10_ahrs/) | I2C+math | Madgwick AHRS attitude estimation |
| 11 | [full_example](11_full_example/) | all | Complete robot integration |

## Toolchain

- ESP-IDF v5.3+
- Target: `esp32s3`
- Console: USB CDC (USB-JTAG port)

## Build

```bash
cd <example_dir>
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```
