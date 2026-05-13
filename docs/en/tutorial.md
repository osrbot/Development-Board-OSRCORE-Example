# OSRCORE Embedded Software Tutorial

> Target hardware: OSRCORE development board, ESP32-S3-WROOM-1U N8R16  
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
    rmt_tx_wait_all_done(s_channel, portMAX_DELAY);
}
```

### 1.5 Summary

This chapter explained WS2812B timing and the ESP-IDF RMT TX API. The same RMT encoder approach can be extended from one onboard LED to a full LED strip.

---

## Chapter 2: Passive Buzzer (LEDC PWM Tone Control)

### 2.1 Key Points

- LEDC timer and channel configuration
- Dynamic PWM frequency changes for tone generation
- Duty cycle and sound output relationship
- Note frequency table and melody playback logic

### 2.2 Course Content

A passive buzzer does not contain an internal oscillator. It must be driven by an external square wave, and the frequency determines the pitch. ESP32-S3's LEDC peripheral can generate PWM at flexible frequencies, making it suitable for melody playback.

This chapter implements an ascending and descending C major scale.

### 2.3 Basic Learning

#### LEDC Structure

LEDC contains timers and channels:

- The timer defines clock source, resolution, and frequency.
- The channel binds a GPIO to a timer and sets duty cycle.

For the buzzer, OSRCORE uses `LEDC_TIMER_1` with 10-bit resolution. A duty value of 512 produces a 50% duty square wave. Muting the buzzer is done by setting the duty to zero.

#### Note Frequencies

```text
C4=262  D4=294  E4=330  F4=349
G4=392  A4=440  B4=494  C5=523
```

### 2.4 Program Study

Initialize the timer and channel:

```c
ledc_timer_config_t timer = {
    .speed_mode      = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_10_BIT,
    .timer_num       = LEDC_TIMER_1,
    .freq_hz         = 1000,
    .clk_cfg         = LEDC_AUTO_CLK,
};
ledc_timer_config(&timer);

ledc_channel_config_t ch = {
    .gpio_num   = 42,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel    = LEDC_CHANNEL_2,
    .timer_sel  = LEDC_TIMER_1,
    .duty       = 0,
};
ledc_channel_config(&ch);
```

Play a single note:

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

### 2.5 Summary

This chapter showed how to drive a passive buzzer with LEDC and how to change tone by changing PWM frequency. LEDC is also used later for ESC and servo PWM output.

---

## Chapter 3: Servo and ESC Control

### 3.1 Key Points

- Standard RC PWM: 50 Hz period and 1000–2000 µs pulse width
- Duty calculation with LEDC 14-bit resolution
- ESC arming sequence: hold neutral for about two seconds
- Dual-channel configuration for throttle and steering

### 3.2 Course Content

Both the ESC and steering servo use standard RC PWM. A 1000 µs pulse represents the minimum command, 1500 µs is neutral, and 2000 µs is the maximum command. OSRCORE uses LEDC TIMER0 with two channels: GPIO1 for throttle and GPIO2 for steering.

### 3.3 Basic Learning

#### RC PWM Timing

```text
Period: 20 ms (50 Hz)
Pulse range: 1000 µs to 2000 µs
Neutral: 1500 µs
```

#### Duty Calculation

With an 80 MHz clock and 14-bit LEDC resolution:

```text
period_ticks = 80_000_000 / 50 = 1_600_000
1 µs = 80 ticks
duty = pulse_us × 80
```

Examples:

```text
1500 µs -> 120000
2000 µs -> 160000
```

A convenient macro is:

```c
#define US_TO_DUTY(us) ((us) * 80)
```

#### ESC Arming

Many ESCs require a neutral input signal after power-up before accepting throttle commands. Holding 1500 µs for one to three seconds is a common arming sequence.

### 3.4 Program Study

Configure the timer and two PWM channels:

```c
ledc_timer_config_t timer = {
    .speed_mode      = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_14_BIT,
    .timer_num       = LEDC_TIMER_0,
    .freq_hz         = 50,
    .clk_cfg         = LEDC_AUTO_CLK,
};
ledc_timer_config(&timer);

ledc_channel_config_t ch_throttle = {
    .gpio_num = 1,
    .channel = LEDC_CHANNEL_0,
    .timer_sel = LEDC_TIMER_0,
    .duty = US_TO_DUTY(1500),
};
ledc_channel_config(&ch_throttle);

ledc_channel_config_t ch_steering = {
    .gpio_num = 2,
    .channel = LEDC_CHANNEL_1,
    .timer_sel = LEDC_TIMER_0,
    .duty = US_TO_DUTY(1500),
};
ledc_channel_config(&ch_steering);
```

Set pulse width and arm the ESC:

```c
static void set_pulse(ledc_channel_t ch, uint32_t pulse_us)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, US_TO_DUTY(pulse_us));
    ledc_update_duty(LEDC_LOW_SPEED_MODE, ch);
}

set_pulse(CH_THROTTLE, 1500);
set_pulse(CH_STEERING, 1500);
vTaskDelay(pdMS_TO_TICKS(2000));
```

### 3.5 Summary

This chapter covered RC PWM timing, LEDC 14-bit duty calculation, and ESC arming. The `US_TO_DUTY()` macro maps physical pulse width directly to the LEDC duty value used by later control loops.

---

## Chapter 4: SBUS RC Receiver

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

### 7.1 Key Points

- ESP-IDF NVS Flash concepts: namespace, key, and value
- Storing and loading PID parameters
- Default values when NVS data is missing
- Updating parameters without recompiling firmware

### 7.2 Course Content

NVS stores small key-value data in flash. In the OSRCORE examples, it is used for PID parameters and other runtime settings. After tuning parameters through a serial interface or debug command, the values can be saved and restored after reboot.

### 7.3 Basic Learning

#### NVS Concepts

NVS organizes data by namespace. Each namespace contains keys. Values can be integers, strings, binary blobs, or floating-point values stored as raw bytes.

Common steps:

1. Initialize NVS flash.
2. Open a namespace.
3. Read or write values.
4. Commit writes.
5. Close the handle.

#### Typical PID Parameter Set

| Parameter | Meaning |
|---|---|
| `kp` | Proportional gain |
| `ki` | Integral gain |
| `kd` | Derivative gain |
| `target_speed` | Desired speed |

### 7.4 Program Study

Initialize NVS:

```c
esp_err_t ret = nvs_flash_init();
if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());
}
```

Open a namespace and read parameters:

```c
nvs_handle_t handle;
ESP_ERROR_CHECK(nvs_open("pid", NVS_READWRITE, &handle));

size_t len = sizeof(pid_params_t);
esp_err_t err = nvs_get_blob(handle, "params", &params, &len);
if (err == ESP_ERR_NVS_NOT_FOUND) {
    params.kp = 1.0f;
    params.ki = 0.0f;
    params.kd = 0.0f;
}
```

Write and commit parameters:

```c
ESP_ERROR_CHECK(nvs_set_blob(handle, "params", &params, sizeof(params)));
ESP_ERROR_CHECK(nvs_commit(handle));
nvs_close(handle);
```

### 7.5 Summary

NVS makes runtime tuning practical. Instead of hard-coding PID parameters, the firmware can store tuned values in flash and load them automatically after each reboot.

---

## Chapter 8: FreeRTOS Multitasking

### 8.1 Key Points

- FreeRTOS task creation and core pinning on ESP32-S3
- Queue-based communication between tasks
- Mutex protection for shared data
- Separating sensor, control, and telemetry tasks

### 8.2 Course Content

A robot program should not place all logic inside a single loop. OSRCORE uses FreeRTOS tasks to separate RC input, sensor reading, control computation, and logging. This improves timing stability and makes the software easier to extend.

### 8.3 Basic Learning

#### Task Model

ESP32-S3 has two cores. ESP-IDF allows tasks to be pinned to a specific core:

```c
xTaskCreatePinnedToCore(task_func, "task", 4096, NULL, 5, NULL, 0);
```

A typical split is:

| Task | Responsibility |
|---|---|
| RC task | Receive and parse SBUS frames |
| Control task | Encoder feedback, PID calculation, ESC output |
| IMU task | Read and process IMU data |
| Telemetry task | Print logs and debug values |

#### Queue and Mutex

- A queue transfers data between tasks safely.
- A mutex protects shared state from concurrent access.

### 8.4 Program Study

Create a queue:

```c
typedef struct {
    uint16_t ch[16];
    bool failsafe;
} rc_frame_t;

QueueHandle_t rc_queue = xQueueCreate(4, sizeof(rc_frame_t));
```

Send from the SBUS task:

```c
rc_frame_t frame;
if (sbus_read(&frame)) {
    xQueueSend(rc_queue, &frame, 0);
}
```

Receive in the control task:

```c
rc_frame_t frame;
if (xQueueReceive(rc_queue, &frame, pdMS_TO_TICKS(20))) {
    if (!frame.failsafe) {
        // Convert channels to throttle and steering commands.
    }
}
```

### 8.5 Summary

FreeRTOS allows the robot firmware to remain responsive and modular. Queues and mutexes provide safe communication between tasks and avoid timing interference between unrelated work.

---

## Chapter 9: PID Motor Speed Control

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

## Chapter 10: Madgwick AHRS Attitude Estimation

### 10.1 Key Points

- Quaternion attitude representation
- Accelerometer and gyroscope fusion
- Madgwick 6-DOF AHRS update flow
- Converting quaternion to Euler angles

### 10.2 Course Content

The gyroscope provides angular velocity but drifts over time. The accelerometer gives a gravity reference but is noisy during motion. Madgwick AHRS fuses both sensors to estimate roll, pitch, and yaw-related attitude information.

### 10.3 Basic Learning

#### Why Use Quaternions

Euler angles are easy to read but suffer from singularities. Quaternions avoid gimbal lock and are efficient for continuous attitude updates.

A quaternion contains four components:

```text
q = [q0, q1, q2, q3]
```

#### AHRS Pipeline

1. Read accelerometer and gyroscope data.
2. Normalize the accelerometer vector.
3. Convert gyroscope units to radians per second.
4. Run the Madgwick update step.
5. Normalize the quaternion.
6. Convert the quaternion to roll, pitch, and yaw for display or control.

### 10.4 Program Study

Typical update loop:

```c
qmi8658_data_t imu;
if (qmi8658_read(&imu)) {
    madgwick_update_imu(&ahrs,
                        imu.gx * DEG_TO_RAD,
                        imu.gy * DEG_TO_RAD,
                        imu.gz * DEG_TO_RAD,
                        imu.ax, imu.ay, imu.az,
                        dt);
}
```

Quaternion to Euler conversion:

```c
roll  = atan2f(2.0f * (q0*q1 + q2*q3),
               1.0f - 2.0f * (q1*q1 + q2*q2));
pitch = asinf(2.0f * (q0*q2 - q3*q1));
yaw   = atan2f(2.0f * (q0*q3 + q1*q2),
               1.0f - 2.0f * (q2*q2 + q3*q3));
```

### 10.5 Summary

This chapter introduced IMU fusion with the Madgwick algorithm. The result is a stable attitude estimate that can be used for logging, control, or future robot behavior extensions.

---

## Chapter 11: Complete Robot Example

### 11.1 Key Points

- Integrating all previous peripherals into one project
- Multi-task architecture on ESP32-S3
- SBUS remote control and serial debug control
- Encoder feedback, PID control, ESC output, servo output, and IMU telemetry
- Safety behavior when failsafe or signal loss occurs

### 11.2 Course Content

The full example combines the previous chapters into a complete robot application. Instead of testing one peripheral at a time, the firmware runs multiple tasks and coordinates input, control, and output.

### 11.3 Basic Learning

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

### 11.4 Program Study

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

### 11.5 Summary

This final chapter integrates LED, buzzer, SBUS, I2C IMU, encoder, NVS, FreeRTOS, PID, AHRS, and PWM output into a complete OSRCORE robot firmware architecture. The project can be used as a base for further robot applications such as autonomous driving, telemetry streaming, and higher-level navigation.
