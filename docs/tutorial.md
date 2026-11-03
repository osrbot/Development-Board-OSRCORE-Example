# OSRCORE Embedded Software Tutorial

> Target hardware: OSRCORE development board, ESP32-S3-WROOM-1U N16R8
> Toolchain: ESP-IDF v5.3+

---

## Chapter 0: ESP-IDF Toolchain and OSRCORE Hardware

### 0.1 Key Points

- ESP-IDF installation methods: official scripts and the VS Code extension
- ESP-IDF project structure and the role of `CMakeLists.txt`
- Common `idf.py` commands: `build`, `flash`, `monitor`, and `menuconfig`
- OSRCORE peripheral and GPIO resource overview
- USB CDC console configuration through USB Serial/JTAG

### 0.2 Course Content

This chapter explains how to set up the ESP-IDF development environment on Linux, macOS, and Windows, then introduces the hardware resources of the OSRCORE board. After this chapter, you should be able to build, flash, and monitor a minimal ESP-IDF application.

OSRCORE is a compact mobile-robot control board based on the ESP32-S3 dual-core 240 MHz MCU. It integrates a WS2812B RGB LED, a passive buzzer, a QMI8658 six-axis IMU, a quadrature encoder interface, SBUS receiver input, ESC and servo PWM outputs, battery voltage sensing, and a USB CDC debug console. These peripherals cover the basic requirements of a small wheeled robot.

### 0.3 Basic Learning

#### ESP-IDF Installation

The recommended installation method is the official script:

```bash
# Linux / macOS
git clone --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf
cd ~/esp/esp-idf
./install.sh esp32s3
. ./export.sh          # Run this every time a new terminal is opened
```

On Windows, install ESP-IDF using the official Windows Installer and run commands in the ESP-IDF CMD terminal.

#### Project Structure

```text
my_project/
├── CMakeLists.txt          # Top level: cmake_minimum_required + project()
├── sdkconfig.defaults      # Default menuconfig options
└── main/
    ├── CMakeLists.txt      # idf_component_register(SRCS "main.c")
    └── main.c
```

A minimal top-level `CMakeLists.txt` looks like this:

```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(my_project)
```

#### Common idf.py Commands

| Command | Description |
|---|---|
| `idf.py set-target esp32s3` | Set the target chip. Required for the first build. |
| `idf.py menuconfig` | Open the configuration menu. |
| `idf.py build` | Build the project. |
| `idf.py flash` | Flash the firmware to the board. |
| `idf.py monitor` | Open the serial monitor. |
| `idf.py flash monitor` | Flash and then immediately monitor. |
| `idf.py fullclean` | Remove all build artifacts. |

Specify the serial port when needed:

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

#### OSRCORE Hardware Resource Overview

| Peripheral | GPIO | Driver / Protocol | Notes |
|---|---:|---|---|
| WS2812B LED | GPIO45 | RMT TX | GRB byte order |
| Passive buzzer | GPIO42 | LEDC TIMER1 CH2 | 10-bit, 50% duty |
| ESC throttle | GPIO1 | LEDC TIMER0 CH0 | 50 Hz, 14-bit |
| Steering servo | GPIO2 | LEDC TIMER0 CH1 | 50 Hz, 14-bit |
| Encoder EA | GPIO3 | PCNT edge | 512 PPR |
| Encoder EB | GPIO9 | PCNT level | Gear ratio 10.55 |
| I2C SDA | GPIO10 | I2C_NUM_0 | 400 kHz |
| I2C SCL | GPIO11 | I2C_NUM_0 | 400 kHz |
| QMI8658 IMU | I2C 0x6B | I2C | ±8g / ±2048 dps |
| SBUS RX | GPIO44 | UART0 | 100k, 8E2, inverted |
| Battery ADC | GPIO4 | ADC1 CH3 | Voltage divider |
| Battery enable | GPIO16 | GPIO output | Active high |
| USB CDC | Internal | USB Serial/JTAG | Console |

#### USB CDC Console Configuration

Add the following to `sdkconfig.defaults`:

```text
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y
CONFIG_ESP_CONSOLE_SECONDARY_NONE=y
```

Or initialize USB Serial/JTAG in code:

```c
usb_serial_jtag_driver_config_t cfg = {
    .rx_buffer_size = 256,
    .tx_buffer_size = 256,
};
usb_serial_jtag_driver_install(&cfg);
esp_vfs_usb_serial_jtag_use_driver();
```

### 0.4 Program Study

The entry point of an ESP-IDF application is `app_main()`. It runs in a FreeRTOS task. A minimal OSRCORE application is:

```c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "Hello OSRCORE!");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

`ESP_LOGI` prints a timestamped log line with a tag. Log levels are ordered as `LOGV < LOGD < LOGI < LOGW < LOGE`.

### 0.5 Summary

This chapter covered ESP-IDF installation, the basic `idf.py` workflow, and the OSRCORE hardware resource map. The following chapters focus on one peripheral at a time and gradually build a complete robot control system.

---

## Chapter 1: WS2812B RGB LED (RMT)

> 📂 **Example Code**: [01_blink_led](https://github.com/osrbot/Development-Board-OSRCORE-Example/tree/main/01_blink_led)

### 1.1 Key Points

- WS2812B single-wire timing: T0H, T0L, T1H, T1L, and reset
- ESP-IDF v5.x RMT TX API usage
- Custom `rmt_encoder_t` implementation
- GRB byte order and color mapping

### 1.2 Course Content

WS2812B is an RGB LED with an integrated driver IC. It receives GRB color data through a single data line. ESP32-S3's RMT peripheral can generate the precise pulse timing required by WS2812B without keeping the CPU busy.

The example cycles through red, green, blue, and off, switching color every 500 ms.

### 1.3 Basic Learning

#### WS2812B Timing

WS2812B uses NRZ coding. Each bit is represented by a high-level duration followed by a low-level duration.

| Symbol | High Level | Low Level | Meaning |
|---|---:|---:|---|
| 0 bit | 400 ns | 850 ns | Logic 0 |
| 1 bit | 800 ns | 450 ns | Logic 1 |
| Reset | — | ≥ 50 µs | Frame end |

Each LED receives 24 bits in GRB order, MSB first. In a daisy chain, each LED consumes the first 24 bits and forwards the rest.

With an RMT resolution of 80 MHz, one tick is 12.5 ns:

```text
T0H = 32 ticks  T0L = 68 ticks
T1H = 64 ticks  T1L = 36 ticks
Reset = 4000 ticks (50 µs)
```

#### RMT Encoder Architecture

ESP-IDF v5.x RMT uses composable encoders:

- `rmt_bytes_encoder`: expands bytes into RMT symbols bit by bit.
- `rmt_copy_encoder`: copies a predefined symbol sequence, such as the reset pulse.
- A custom encoder combines both to create a complete WS2812B frame.

### 1.4 Program Study

Create the TX channel and byte encoder:

```c
rmt_tx_channel_config_t ch_cfg = {
    .gpio_num          = 45,
    .clk_src           = RMT_CLK_SRC_DEFAULT,
    .resolution_hz     = 80000000,
    .mem_block_symbols = 64,
    .trans_queue_depth = 4,
};
rmt_new_tx_channel(&ch_cfg, &s_channel);

rmt_bytes_encoder_config_t bcfg = {
    .bit0 = { .level0=1, .duration0=32, .level1=0, .duration1=68 },
    .bit1 = { .level0=1, .duration0=64, .level1=0, .duration1=36 },
    .flags.msb_first = 1,
};
rmt_new_bytes_encoder(&bcfg, &ws->bytes_encoder);
rmt_enable(s_channel);
```

Send one RGB color in GRB order:

```c
static void led_set(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t grb[3] = { g, r, b };
    rmt_transmit_config_t tx_cfg = { .loop_count = 0 };
    rmt_transmit(s_channel, s_encoder, grb, sizeof(grb), &tx_cfg);
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(s_channel, -1));
}
```

### 1.5 Summary

This chapter explained WS2812B timing and the ESP-IDF RMT TX API. The same RMT encoder approach can be extended from one onboard LED to a full LED strip.

---

## Chapter 2: Passive Buzzer (LEDC PWM Tone Control)

### Goal

Drive the GPIO42 passive buzzer with a configurable tone. Only the board and a USB data cable are required.

### Simulation and hardware

Frequency is 220–1760 Hz; duration is 100–1000 ms, followed by 100 ms of silence. Defaults are 440 Hz and 300 ms. Enable browser audio for the simulation. Hardware uses LEDC with a 50% duty cycle.

Open Connect board, build the current parameters, and download. Slider changes require a new build before they affect hardware. Source: `02_buzzer/main/main.c`.

### Exercise

Double the frequency and compare pitch, then change only duration. The browser audio setting does not mute the physical board.

### Download

Connect USB, build current parameters, download, then reconnect logs. Wi-Fi needs initial USB provisioning.

### Troubleshooting

For hardware, check the running project and parameters. For browser audio, check its sound toggle and volume.

---

## Chapter 3: Servo and ESC Control

### Goal

Set a fixed servo pulse instead of sweeping automatically. GPIO2 produces 50 Hz PWM with a 1000–2000 µs pulse (1500 µs default). GPIO1 remains at the 1500 µs ESC neutral signal.

### Wiring and operation

Use EX2 row 8: GPIO2, VCC_ESC, GND. Power the servo according to its rating and share ground with the board. VCC_ESC is separate from the ordinary 5 V supply domain. Check connector orientation in the board view.

Start at 1500 µs and make small adjustments. Browser angles approximate a typical servo; actual travel depends on the device. Build and download to change the physical pulse. Moving a slider alone does not drive hardware.

Source: `03_pwm_servo/main/main.c`. Check the pulse in the device log.

### Exercise

Compare one parameter change at a time and record the observed hardware result.

### Code

PWM setup and duty conversion are in main.c. OSR_SERVO_PULSE controls GPIO2.

### Verification

Check pulse width before attaching a mechanical load. The servo determines whether 1500 µs corresponds to its center.

---

## Chapter 4: SBUS RC Receiver

> 📂 **Example Code**: [04_sbus_rc](https://github.com/osrbot/Development-Board-OSRCORE-Example/tree/main/04_sbus_rc)

### 4.1 Key Points

- SBUS frame format: 25 bytes, 100000 baud, 8E2, inverted signal
- 16-channel 11-bit bitfield unpacking
- ESP-IDF UART configuration and line inversion
- Failsafe and lost-frame flag handling

### 4.2 Course Content

SBUS is a digital RC protocol widely used in RC vehicles and robots. It transmits 16 analog channels and two digital channels through a single serial line. Each frame is 25 bytes. OSRCORE receives SBUS on UART0 GPIO44.

### 4.3 Basic Learning

#### SBUS Frame Format

```text
Byte 0:  0x0F frame header
Byte 1-22: packed channel data, 16 channels × 11 bits = 176 bits
Byte 23: flags
  bit2 = failsafe
  bit3 = lost_frame
Byte 24: 0x00 frame end
```

Typical channel values range from about 240 to 1810, with neutral near 1024.

#### Bitfield Unpacking

The 16 channels are tightly packed into 22 bytes:

```c
ch[0] = ((buf[1]      | buf[2]  << 8) & 0x07FF);
ch[1] = ((buf[2] >> 3 | buf[3]  << 5) & 0x07FF);
ch[2] = ((buf[3] >> 6 | buf[4]  << 2 | buf[5] << 10) & 0x07FF);
// Continue the same pattern for all 16 channels.
```

#### UART Configuration

SBUS uses 100000 baud, even parity, two stop bits, and inverted signaling. ESP-IDF can handle inversion with `uart_set_line_inverse()`.

### 4.4 Program Study

UART initialization:

```c
uart_config_t cfg = {
    .baud_rate  = 100000,
    .data_bits  = UART_DATA_8_BITS,
    .parity     = UART_PARITY_EVEN,
    .stop_bits  = UART_STOP_BITS_2,
    .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
};
uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);
uart_param_config(UART_NUM_0, &cfg);
uart_set_pin(UART_NUM_0, 43, 44, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
uart_set_line_inverse(UART_NUM_0, UART_SIGNAL_RXD_INV | UART_SIGNAL_TXD_INV);
```

Frame synchronization and parsing:

```c
uint8_t byte;
do {
    uart_read_bytes(UART_NUM_0, &byte, 1, portMAX_DELAY);
} while (byte != 0x0F);

buf[0] = byte;
int got = uart_read_bytes(UART_NUM_0, buf + 1, 24, pdMS_TO_TICKS(10));
if (got == 24 && sbus_parse(buf, &data)) {
    if (!data.failsafe) {
        // Process channel values here.
    }
}
```

Convert SBUS channel values to PWM pulse width:

```c
#define SBUS_TO_US(v) ((uint32_t)(((v) - 240) * 1000 / 1570 + 1000))
```

### 4.5 Summary

This chapter covered SBUS framing, channel unpacking, UART 8E2 configuration, and failsafe handling. SBUS input becomes the remote-control source for the full robot example.

---

## Chapter 5: QMI8658 IMU over I2C

> 📂 **Example Code**: [05_imu_i2c](https://github.com/osrbot/Development-Board-OSRCORE-Example/tree/main/05_imu_i2c)

### 5.1 Key Points

- ESP-IDF v5.x I2C master bus-device API
- QMI8658 register map and initialization sequence
- Accelerometer and gyroscope ranges and sensitivities
- Raw data conversion to physical units

### 5.2 Course Content

QMI8658 is a six-axis IMU that integrates a three-axis accelerometer and a three-axis gyroscope. It communicates with ESP32-S3 through I2C. OSRCORE configures it for high-rate motion sensing, providing input data for attitude estimation.

### 5.3 Basic Learning

#### I2C Master API Structure

ESP-IDF v5.x uses a bus-device model:

- `i2c_new_master_bus()` creates an I2C bus handle.
- `i2c_master_bus_add_device()` attaches a device to the bus.
- `i2c_master_transmit()` and `i2c_master_transmit_receive()` perform transfers.

#### Important QMI8658 Registers

| Register | Address | Description |
|---|---:|---|
| WHO_AM_I | 0x00 | Chip ID, expected `0x05` |
| CTRL1 | 0x02 | SPI/I2C configuration |
| CTRL2 | 0x03 | Accelerometer range and ODR |
| CTRL3 | 0x04 | Gyroscope range and ODR |
| CTRL7 | 0x08 | Enable accelerometer and gyroscope |
| AX_L | 0x35 | Accelerometer X low byte, start of data block |

#### Unit Conversion

```text
acceleration_g = raw_int16 / 4096.0     // ±8g, 4096 LSB/g
gyro_dps       = raw_int16 / 16.0       // ±2048 dps, 16 LSB/dps
temperature_c  = raw_int16 / 256.0 + 25.0
```

### 5.4 Program Study

Create the I2C bus and add the device:

```c
i2c_master_bus_config_t bus_cfg = {
    .i2c_port      = I2C_NUM_0,
    .sda_io_num    = 10,
    .scl_io_num    = 11,
    .clk_source    = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
};
i2c_master_bus_handle_t bus;
ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

i2c_device_config_t dev_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address  = 0x6B,
    .scl_speed_hz    = 400000,
};
i2c_master_dev_handle_t dev;
ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &dev));
```

Read registers by writing the register address and then reading data:

```c
static esp_err_t qmi_read(i2c_master_dev_handle_t dev,
                          uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_transmit_receive(dev, &reg, 1, buf, len, 100);
}
```

Print converted IMU data:

```c
qmi8658_data_t d;
if (qmi8658_read(&d)) {
    printf("ax=%.3f ay=%.3f az=%.3f  gx=%.2f gy=%.2f gz=%.2f  T=%.1f\n",
           d.ax, d.ay, d.az, d.gx, d.gy, d.gz, d.temp);
}
```

### 5.5 Summary

This chapter explained the ESP-IDF I2C master API and how to initialize and read the QMI8658 IMU. The IMU data is used later by the Madgwick AHRS algorithm.

---

## Chapter 6: Quadrature Encoder Speed Measurement

> 📂 **Example Code**: [06_encoder](https://github.com/osrbot/Development-Board-OSRCORE-Example/tree/main/06_encoder)

### 6.1 Key Points

- Quadrature encoder principle and A/B phase direction detection
- ESP-IDF PCNT new API configuration
- Overflow callback and accumulated software count
- Converting pulse count to wheel linear speed

### 6.2 Course Content

OSRCORE uses a DC motor with a gearbox and a 512 PPR quadrature encoder. PCNT counts encoder pulses, and the software converts pulse differences into wheel speed using the gear ratio and wheel diameter. This speed feedback is used by the PID controller.

### 6.3 Basic Learning

#### Quadrature Encoder Principle

A quadrature encoder outputs two square waves with a 90-degree phase shift:

- Forward rotation: phase A leads phase B.
- Reverse rotation: phase B leads phase A.

PCNT can count edges on phase A while using phase B as the direction control level.

#### Speed Conversion

```text
pulses_per_rev = PPR × gear_ratio = 512 × 10.55 = 5401.6
wheel_circumference = 2π × 0.0425 = 0.2670 m
distance_per_pulse = 0.2670 / 5401.6 = 4.943×10⁻⁵ m/pulse
speed_mps = delta_pulses × distance_per_pulse / delta_time
```

#### Overflow Handling

The PCNT hardware counter has a limited range. For high-speed rotation or long sampling intervals, the counter may overflow. Registering an `on_reach` callback allows the software to accumulate the overflow amount into a larger signed counter.

### 6.4 Program Study

Create the PCNT unit and channel:

```c
pcnt_unit_config_t unit_config = {
    .high_limit = 32767,
    .low_limit  = -32768,
};
pcnt_unit_handle_t unit;
pcnt_new_unit(&unit_config, &unit);

pcnt_chan_config_t chan_config = {
    .edge_gpio_num  = 3,
    .level_gpio_num = 9,
};
pcnt_channel_handle_t chan;
pcnt_new_channel(unit, &chan_config, &chan);
```

Configure edge and level actions:

```c
pcnt_channel_set_edge_action(chan,
                             PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                             PCNT_CHANNEL_EDGE_ACTION_HOLD);
pcnt_channel_set_level_action(chan,
                              PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                              PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
```

Compute speed from pulse differences:

```c
int64_t now_count = encoder_get_count();
int64_t delta = now_count - last_count;
float speed = delta * DIST_PER_PULSE / dt;
last_count = now_count;
```

### 6.5 Summary

This chapter showed how to use PCNT for encoder counting and how to convert pulses into linear speed. Encoder speed feedback is the key input for the motor PID loop.

---

## Chapter 7: NVS Parameter Persistence

### Goal

Save one Kp value and read it after reboot. This lesson does not run a robot controller.

### Procedure

1. Set Kp (default 2.5), build, and download.
2. Read `Loaded Kp=...`; an existing saved value takes precedence.
3. Use Save Kp or send `save` to store the value compiled into this firmware. Check `Saved Kp=...`.
4. Reboot, reconnect, and verify the loaded value.

Browser storage and board NVS are independent. The firmware uses the `lesson7` namespace. USB downloads preserve NVS. After changing a parameter, build and download before saving it.

Source: `07_nvs/main/main.c`. The example uses `nvs_set_blob()`, `nvs_commit()`, and `nvs_get_blob()`.

### Storage scope

Only lesson7/kp is accessed; other lesson and Wi-Fi namespaces are preserved.

### Troubleshooting

Confirm the running project is 07_nvs before saving. Building alone does not change the flashed program or stored value.

### Verification

Save and reboot with two different Kp values and confirm the second replaces the first.

---

## Chapter 8: FreeRTOS Multitasking

### Goal

Observe two independent periodic tasks without robot drivers or sensor initialization.

### Simulation and hardware

Task A defaults to 500 ms and B to 1000 ms. Change their periods, run the browser timeline, then build and download. Real logs show task names and increasing counters.

`vTaskDelayUntil()` schedules relative to the previous wake time. The browser models periods, not dual-core scheduling or execution time. Simultaneous prints have no guaranteed order.

Try equal periods, then 300 ms and 700 ms. Source: `08_freertos/main/main.c`.

### Exercise

Compare one parameter change at a time and record the observed hardware result.

### Code

Each task counts and prints. Main creates the tasks; OSR_TASK_A_MS and OSR_TASK_B_MS supply their periods.

### Verification

Compare count ratios over a longer interval rather than inferring missed tasks from adjacent log lines.

---

## Chapter 9: PID Motor Speed Control

> 📂 **Example Code**: [09_pid_motor](https://github.com/osrbot/Development-Board-OSRCORE-Example/tree/main/09_pid_motor)

### 9.1 Key Points

- Closed-loop control structure: target, feedback, error, and output
- Encoder speed as feedback
- PID parameter meaning and tuning direction
- Mapping controller output to ESC PWM pulse width

### 9.2 Course Content

Open-loop throttle control cannot compensate for battery voltage, load, or ground friction. A closed-loop speed controller compares target speed with encoder-measured speed and adjusts the ESC command using PID.

### 9.3 Basic Learning

#### PID Formula

```text
error = target_speed - measured_speed
integral += error × dt
derivative = (error - last_error) / dt
output = kp × error + ki × integral + kd × derivative
```

#### Parameter Meaning

| Parameter | Effect |
|---|---|
| `kp` | Main response strength. Too high causes oscillation. |
| `ki` | Eliminates steady-state error. Too high causes windup. |
| `kd` | Damps fast changes. Too high amplifies noise. |

### 9.4 Program Study

A simple PID step function:

```c
static float pid_update(pid_t *pid, float target, float measured, float dt)
{
    float error = target - measured;
    pid->integral += error * dt;
    float derivative = (error - pid->last_error) / dt;
    pid->last_error = error;

    return pid->kp * error + pid->ki * pid->integral + pid->kd * derivative;
}
```

Map PID output to a safe ESC command:

```c
float out = pid_update(&speed_pid, target_speed, measured_speed, dt);
uint32_t pulse = 1500 + (int32_t)out;
pulse = CLAMP(pulse, 1000, 2000);
set_pulse(CH_THROTTLE, pulse);
```

Safety handling should set the throttle to neutral when failsafe is active or when the measured data is invalid.

### 9.5 Summary

The PID loop connects encoder feedback to ESC output. It is the first step from peripheral examples toward a controllable robot motion system.

---

## Chapter 10: Complementary Attitude Filter

### Goal

Fuse gyroscope integration and gravity angles with a complementary filter to obtain Roll and Pitch. The basic lesson does not enable the heater or wait for a temperature threshold.

### Model

Each update combines `alpha × integrated gyro angle + (1-alpha) × gravity angle`, with alpha=0.95 by default. Elapsed time is measured using `esp_timer_get_time()` and includes I²C reads and averaging; it is not assumed to be exactly 5 ms.

Linear acceleration disturbs the gravity estimate. Gravity alone cannot provide absolute heading.

### Procedure

Keep the board still during startup bias calibration, then tilt it slowly. Compare different alpha values. Browser angles are simulated inputs; hardware reads the onboard QMI8658 over GPIO10/11.

Source: `10_ahrs/main/main.c`. Historical Madgwick and heater files remain as advanced references but are not built in this lesson.

### Parameter scope

Alpha closer to one emphasizes gyro integration per update. Reassess the weight if sample frequency changes.

### Verification

Test flat, slow side-to-side tilt, and slow forward/back tilt. Errors during rapid translation are a model limitation.

---

## Chapter 12: Complete Robot Example

> 📂 **Example Code**: [12_full_example](https://github.com/osrbot/Development-Board-OSRCORE-Example/tree/main/12_full_example)

### 12.1 Key Points

- Integrating all previous peripherals into one project
- Multi-task architecture on ESP32-S3
- SBUS remote control and serial debug control
- Encoder feedback, PID control, ESC output, servo output, and IMU telemetry
- Safety behavior when failsafe or signal loss occurs

### 12.2 Course Content

The full example combines the previous chapters into a complete robot application. Instead of testing one peripheral at a time, the firmware runs multiple tasks and coordinates input, control, and output.

### 12.3 Basic Learning

#### System Architecture

```text
SBUS receiver  ─┐
Serial command ─┼─> command state ─> control task ─> ESC / servo PWM
Encoder feedback ────────────────┘
IMU task ─────────────────────────> telemetry / attitude state
```

#### Task Responsibilities

| Task | Responsibility |
|---|---|
| SBUS task | Decode remote-control frames and failsafe state |
| Control task | Read target command, read encoder speed, run PID, output PWM |
| IMU task | Read QMI8658 and run AHRS update |
| Telemetry task | Print status over USB CDC |

#### Safety Rules

A robot control program should always have safe fallback behavior:

- If SBUS failsafe is active, set throttle to neutral.
- If no command is received for a timeout period, stop the motor.
- If encoder feedback is invalid, limit or disable closed-loop output.
- Keep servo and ESC outputs within safe pulse limits.

### 12.4 Program Study

A simplified main initialization flow:

```c
void app_main(void)
{
    board_io_init();
    nvs_params_load(&params);
    led_init();
    buzzer_init();
    pwm_init();
    sbus_init();
    encoder_init();
    imu_init();

    xTaskCreatePinnedToCore(sbus_task, "sbus", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(control_task, "ctrl", 4096, NULL, 6, NULL, 1);
    xTaskCreatePinnedToCore(imu_task, "imu", 4096, NULL, 4, NULL, 0);
    xTaskCreatePinnedToCore(telemetry_task, "tele", 4096, NULL, 3, NULL, 0);
}
```

A simplified control loop:

```c
while (1) {
    command_t cmd = command_get_latest();
    float speed = encoder_get_speed_mps();

    if (cmd.failsafe) {
        set_pulse(CH_THROTTLE, 1500);
        set_pulse(CH_STEERING, 1500);
    } else {
        float out = pid_update(&pid, cmd.target_speed, speed, CONTROL_DT);
        uint32_t throttle = clamp_pwm(1500 + (int32_t)out);
        uint32_t steering = clamp_pwm(cmd.steering_us);
        set_pulse(CH_THROTTLE, throttle);
        set_pulse(CH_STEERING, steering);
    }

    vTaskDelay(pdMS_TO_TICKS(10));
}
```

### 12.5 Summary

This final chapter integrates LED, buzzer, SBUS, I2C IMU, encoder, NVS, FreeRTOS, PID, AHRS, and PWM output into a complete OSRCORE robot firmware architecture. The project can be used as a base for further robot applications such as autonomous driving, telemetry streaming, and higher-level navigation.

---

## Chapter 11: QMC6309 Magnetometer and Compass Heading

> 📂 **Example Code**: [11_mag_compass](https://github.com/osrbot/Development-Board-OSRCORE-Example/tree/main/11_mag_compass)

### 11.1 Key Points

- QMC6309 3-axis magnetometer principle (Hall effect, I2C, address 0x7C)
- Hard-iron interference: fixed offset from permanent magnets on the PCB, corrected with min/max calibration
- Soft-iron interference: ferromagnetic materials distort the field into an ellipse, corrected with a scaling matrix
- Tilt-compensated heading: project horizontal field components onto the horizontal plane using roll/pitch
- Complementary filter fusing gyroscope integration and magnetometer absolute heading

### 11.2 Course Content

This chapter demonstrates the complete QMC6309 workflow: initialization, raw data reading, hard/soft iron calibration, tilt-compensated heading calculation, and USB CDC console interaction. After this chapter you can integrate the magnetometer into an AHRS for absolute heading estimation.

### 11.3 Basic Learning

#### QMC6309 Register Map

| Register | Address | Description |
|----------|---------|-------------|
| CHIP_ID | 0x00 | Fixed 0x90, used to verify communication |
| X_LSB | 0x01 | X-axis data LSB (6 bytes total for XYZ) |
| STATUS | 0x09 | bit0 = DRDY (data ready) |
| CTRL1 | 0x0A | OSR, operating mode |
| CTRL2 | 0x0B | Soft reset, ODR, range |

Init sequence: soft reset → verify CHIP_ID → configure CTRL1 (OSR) → configure CTRL2 (ODR, range) → set continuous measurement mode.

#### Hard-Iron Calibration

Rotate the device 360° in the horizontal plane and record min/max for X and Y:

```c
hard_iron[0] = (max_x + min_x) / 2.0f;
hard_iron[1] = (max_y + min_y) / 2.0f;
```

After calibration: `x_cal = x_raw - hard_iron[0]`

#### Soft-Iron Calibration

When X and Y ranges differ, the field trace is an ellipse. A diagonal scaling matrix stretches it back to a circle:

```c
float avg_range = (range_x + range_y) / 2.0f;
soft_iron[0] = avg_range / range_x;   // X scale
soft_iron[4] = avg_range / range_y;   // Y scale
soft_iron[8] = 1.0f;                  // Z unchanged (ground vehicle)
```

#### Tilt Compensation

`atan2(my, mx)` is only accurate when the board is level. When tilted, project the field onto the horizontal plane first:

```c
float mx_h = mx * cosf(pitch) + mz * sinf(pitch);
float my_h = mx * sinf(roll) * sinf(pitch)
           + my * cosf(roll)
           - mz * sinf(roll) * cosf(pitch);
float heading = atan2f(-my_h, mx_h) * 180.0f / M_PI;
if (heading < 0) heading += 360.0f;
```

Roll and pitch come from the IMU (Chapter 10 Madgwick AHRS).

#### Complementary Filter

The magnetometer has noise but no drift; the gyroscope is precise short-term but drifts. A complementary filter combines both:

```c
// alpha = 0.98 static, 0.995 moving
yaw_cf += gyro_z * dt;
float err = mag_heading - yaw_cf;
// normalize err to [-π, π]
yaw_cf += (1.0f - alpha) * err;
```

### 11.4 Program Study

#### Initialization

```c
void qmc6309_init(i2c_master_dev_handle_t dev) {
    s_dev = dev;
    // soft reset
    write_reg(QMC_REG_CTRL2, CTRL2_SOFT_RST);
    vTaskDelay(pdMS_TO_TICKS(20));
    // verify CHIP_ID
    uint8_t id = 0;
    read_reg(QMC_REG_CHIP_ID, &id, 100);
    if (id != 0x90) printf("WARN: QMC6309 CHIP_ID=0x%02X\n", id);
    // configure ODR=200Hz, range=8G, continuous mode
    uint8_t ctrl1 = CTRL1_OSR2(4) | CTRL1_OSR1(3) | CTRL1_MODE(0);
    write_reg(QMC_REG_CTRL1, ctrl1);
    write_reg(QMC_REG_CTRL2, CTRL2_ODR(4) | CTRL2_RNG(2));
    ctrl1 = (ctrl1 & ~0x03) | CTRL1_MODE(3);
    write_reg(QMC_REG_CTRL1, ctrl1);
    vTaskDelay(pdMS_TO_TICKS(100));
}
```

#### Read and Apply Calibration

```c
bool qmc6309_read(qmc6309_data_t *out) {
    uint8_t status = 0;
    read_reg(QMC_REG_STATUS, &status, 2);
    if (!(status & STATUS_DRDY)) return false;

    uint8_t data[6];
    read_burst(QMC_REG_X_LSB, data, 6);
    float rx = (int16_t)((data[1]<<8)|data[0]) / SENSITIVITY_8G;
    float ry = (int16_t)((data[3]<<8)|data[2]) / SENSITIVITY_8G;
    float rz = (int16_t)((data[5]<<8)|data[4]) / SENSITIVITY_8G;

    // apply hard-iron + soft-iron
    float dx = rx - s_hard_iron[0];
    float dy = ry - s_hard_iron[1];
    float dz = rz - s_hard_iron[2];
    out->x = s_soft_iron[0]*dx + s_soft_iron[1]*dy + s_soft_iron[2]*dz;
    out->y = s_soft_iron[3]*dx + s_soft_iron[4]*dy + s_soft_iron[5]*dz;
    out->z = s_soft_iron[6]*dx + s_soft_iron[7]*dy + s_soft_iron[8]*dz;

    out->heading = atan2f(out->y, out->x) * 180.0f / M_PI;
    if (out->heading < 0) out->heading += 360.0f;
    return true;
}
```

#### Main Loop

```c
while (1) {
    qmc6309_data_t d;
    if (qmc6309_read(&d))
        printf("hdg=%.1f x=%.4f y=%.4f z=%.4f\n", d.heading, d.x, d.y, d.z);

    // non-blocking USB CDC command parsing
    // cal [sec] / heading / help
    vTaskDelay(pdMS_TO_TICKS(100));
}
```

### 11.5 Chapter Summary

This chapter covered QMC6309 I2C driver implementation, the two-step hard/soft iron calibration workflow, and tilt-compensated heading calculation. The magnetometer provides an absolute heading reference but is sensitive to electromagnetic interference. In the Chapter 12 full example, a complementary filter fuses the magnetometer with the gyroscope to produce a stable compass heading under dynamic conditions.

## Chapter 13: Factory checks

### Goal

Check the onboard functions individually. Browser fault switches are teaching inputs, not hardware measurements.

### Preparation

Use the board and a USB data cable. External receivers, motors and servos require the corresponding devices to be connected.

### Procedure

Open the device panel in lesson 13, build and download the diagnostic example, then reconnect the serial log. Check the LED, buzzer and sensors before external interfaces.

### Source and output

The implementation is in `13_factory_demo/main/main.c`. Use real device responses for diagnosis; browser outputs are separate. Motor tests require explicit commands; lost frames or a receiver timeout stop remote output.

### Verification

Record each item as passed, failed or not connected. Missing peripherals do not imply a faulty board. Check power, wiring and the running firmware version first.
