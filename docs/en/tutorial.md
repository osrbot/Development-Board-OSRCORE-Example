# OSRCORE Embedded Software Tutorial

> Hardware: OSRCORE development board, ESP32-S3-WROOM-1U N8R16  
> Toolchain: ESP-IDF v5.3+

This English tutorial mirrors the Chinese documentation structure and provides the same two-language browsing experience in VitePress. The detailed Chinese tutorial remains available at [中文教程](/tutorial_zh).

---

## Chapter 0: ESP-IDF Toolchain and OSRCORE Hardware

### Key Points

- Installing ESP-IDF through the official scripts or VS Code extension
- Understanding the project directory layout and CMake files
- Using common `idf.py` commands: `build`, `flash`, `monitor`, and `menuconfig`
- Reviewing OSRCORE GPIO assignments
- Enabling USB CDC through USB Serial/JTAG as the debug console

### Overview

OSRCORE is a compact robot control board based on ESP32-S3. It integrates a WS2812B RGB LED, passive buzzer, QMI8658 IMU, quadrature encoder interface, SBUS receiver input, ESC/servo PWM outputs, battery voltage sensing, and a USB CDC console.

The recommended first workflow is:

```bash
git clone https://github.com/osrbot/Development-Board-OSRCORE-Example.git
cd Development-Board-OSRCORE-Example/01_blink_led
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

### Hardware Resource Summary

| Peripheral | GPIO | Driver / Protocol | Notes |
|---|---:|---|---|
| WS2812B LED | 45 | RMT TX | GRB byte order |
| Passive buzzer | 42 | LEDC | PWM tone output |
| ESC throttle | 1 | LEDC | 50 Hz RC PWM |
| Servo steering | 2 | LEDC | 50 Hz RC PWM |
| Encoder EA | 3 | PCNT | Edge input |
| Encoder EB | 9 | PCNT | Level input |
| I2C SDA | 10 | I2C | QMI8658 + QMC6309 |
| I2C SCL | 11 | I2C | 400 kHz |
| SBUS RX | 44 | UART0 | 100 kbaud, 8E2, inverted |
| Battery ADC | 4 | ADC1 | Voltage divider |
| Battery enable | 16 | GPIO | Active high |

---

## Chapter 1: WS2812B RGB LED (RMT)

The WS2812B LED uses a single-wire NRZ protocol. ESP32-S3's RMT peripheral can generate the required timing precisely without continuous CPU involvement.

| Symbol | High Time | Low Time | Meaning |
|---|---:|---:|---|
| 0 bit | 400 ns | 850 ns | Logic 0 |
| 1 bit | 800 ns | 450 ns | Logic 1 |
| Reset | — | ≥ 50 µs | Frame end |

The example sends colors in GRB order:

```c
static void led_set(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t grb[3] = { g, r, b };
    rmt_transmit_config_t tx_cfg = { .loop_count = 0 };
    rmt_transmit(s_channel, s_encoder, grb, sizeof(grb), &tx_cfg);
    rmt_tx_wait_all_done(s_channel, portMAX_DELAY);
}
```

---

## Chapter 2: Passive Buzzer (LEDC PWM)

A passive buzzer needs an external square wave. The LEDC peripheral is used to output a PWM waveform. Changing the PWM frequency changes the musical tone.

```c
static void buzzer_tone(uint32_t freq_hz, uint32_t duration_ms)
{
    ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_1, freq_hz);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, 512);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
}
```

---

## Chapter 3: Servo and ESC Control

Standard RC PWM uses a 20 ms period, or 50 Hz. The pulse width maps to the command value:

| Pulse Width | Meaning |
|---:|---|
| 1000 µs | Minimum |
| 1500 µs | Neutral |
| 2000 µs | Maximum |

Most ESCs require a neutral signal for one to three seconds after power-up before they are armed.

```c
#define US_TO_DUTY(us) ((us) * 80)

static void set_pulse(ledc_channel_t ch, uint32_t pulse_us)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, US_TO_DUTY(pulse_us));
    ledc_update_duty(LEDC_LOW_SPEED_MODE, ch);
}
```

---

## Chapter 4: SBUS RC Receiver

SBUS is a digital RC protocol with 25-byte frames, 16 analog channels, and failsafe flags. OSRCORE receives SBUS on UART0 GPIO44.

| Field | Description |
|---|---|
| Byte 0 | Frame header `0x0F` |
| Bytes 1–22 | 16 channels, 11 bits each |
| Byte 23 | Flags: lost frame and failsafe |
| Byte 24 | Frame end `0x00` |

```c
uart_config_t cfg = {
    .baud_rate  = 100000,
    .data_bits  = UART_DATA_8_BITS,
    .parity     = UART_PARITY_EVEN,
    .stop_bits  = UART_STOP_BITS_2,
    .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
};
```

---

## Chapter 5: QMI8658 IMU over I2C

The QMI8658 provides 3-axis acceleration and 3-axis angular velocity through I2C. ESP-IDF v5.x uses a bus-device model for the I2C master driver.

```c
i2c_master_bus_config_t bus_cfg = {
    .i2c_port      = I2C_NUM_0,
    .sda_io_num    = 10,
    .scl_io_num    = 11,
    .clk_source    = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
};
```

Typical conversions:

```text
acceleration_g = raw_int16 / 4096.0
gyro_dps       = raw_int16 / 16.0
temperature_c  = raw_int16 / 256.0 + 25.0
```

---

## Chapter 6: Quadrature Encoder Speed Measurement

The encoder outputs two phase-shifted signals. PCNT counts the edge input and uses the level input to determine direction.

```text
pulses_per_rev = PPR × gear_ratio
wheel_distance_per_pulse = wheel_circumference / pulses_per_rev
speed_mps = delta_pulses × wheel_distance_per_pulse / delta_time
```

This provides feedback for the motor speed control loop.

---

## Chapter 7: NVS Parameter Persistence

NVS is used to store runtime parameters such as PID gains. It keeps configuration data across reboots and allows tuning without recompilation.

Typical values:

| Parameter | Meaning |
|---|---|
| `kp` | Proportional gain |
| `ki` | Integral gain |
| `kd` | Derivative gain |
| `target_speed` | Desired motor speed |

---

## Chapter 8: FreeRTOS Multitasking

The full robot example separates I/O, control, and telemetry into independent tasks. This keeps real-time control responsive and makes the system easier to maintain.

Typical task split:

| Task | Responsibility |
|---|---|
| RC task | Receive and parse SBUS frames |
| Control task | Encoder feedback, PID, ESC output |
| Telemetry task | Print logs and monitor system state |

---

## Chapter 9: PID Motor Speed Control

The speed loop uses encoder feedback and adjusts ESC PWM output through a PID controller.

```text
error = target_speed - measured_speed
output = kp * error + ki * integral + kd * derivative
```

The output is finally constrained and mapped to a safe PWM pulse width.

---

## Chapter 10: Madgwick AHRS Attitude Estimation

Madgwick AHRS fuses accelerometer and gyroscope data to estimate orientation. The algorithm updates a quaternion and then converts it to Euler angles for debugging or control.

Main pipeline:

1. Read IMU raw data.
2. Convert raw values to physical units.
3. Run the Madgwick update step.
4. Convert quaternion to roll, pitch, and yaw.

---

## Chapter 11: Complete Robot Example

The final example combines the previous chapters:

- SBUS remote control input
- Encoder speed measurement
- PID speed loop
- Servo and ESC PWM output
- IMU attitude reading
- FreeRTOS task separation
- USB CDC telemetry output

This chapter is the integration point for turning peripheral examples into a complete robot control application.
