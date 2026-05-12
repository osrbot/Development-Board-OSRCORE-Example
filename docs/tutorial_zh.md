# OSRCORE 开发板嵌入式软件教程

> 适用硬件：OSRCORE（ESP32-S3-WROOM-1U N8R16）
> 工具链：ESP-IDF v5.3+
> 对标参考：RoboMaster C 型开发板教程（共 12 章）

---

## 第 0 章  入门：ESP-IDF 工具链与 OSRCORE 硬件

### 0.1 知识要点

- ESP-IDF 的安装方式（官方脚本 / VS Code 插件）
- 项目目录结构与 CMakeLists.txt 的作用
- `idf.py` 常用命令：`build`、`flash`、`monitor`、`menuconfig`
- OSRCORE 开发板各外设的 GPIO 分配总览
- USB CDC 作为调试控制台的配置方法

### 0.2 课程内容

本章介绍如何在 Linux/macOS/Windows 上搭建 ESP-IDF 开发环境，并对 OSRCORE 开发板的硬件资源进行全面介绍。完成本章后，你将能够编译、烧录并监视一个最小示例程序，为后续各章的实验打下基础。

OSRCORE 是一块面向移动机器人的紧凑型控制板，搭载 ESP32-S3 双核 240 MHz 处理器，板载 WS2812B 彩灯、无源蜂鸣器、QMI8658 六轴 IMU、正交编码器接口、SBUS 遥控接收、ESC/舵机 PWM 输出以及 USB CDC 控制台，覆盖了小型轮式机器人所需的全部基础外设。

### 0.3 基础学习

#### ESP-IDF 安装

推荐使用官方一键安装脚本：

```bash
# Linux / macOS
git clone --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf
cd ~/esp/esp-idf
./install.sh esp32s3
. ./export.sh          # 每次新终端都需要执行
```

Windows 用户可下载 ESP-IDF Windows Installer，安装后在 ESP-IDF CMD 终端中操作。

#### 项目结构

```
my_project/
├── CMakeLists.txt          # 顶层：cmake_minimum_required + project()
├── sdkconfig.defaults      # 预设 menuconfig 选项
└── main/
    ├── CMakeLists.txt      # idf_component_register(SRCS "main.c")
    └── main.c
```

顶层 `CMakeLists.txt` 最简形式：

```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(my_project)
```

#### idf.py 常用命令

| 命令 | 说明 |
|------|------|
| `idf.py set-target esp32s3` | 设置目标芯片（首次必须执行） |
| `idf.py menuconfig` | 图形化配置（波特率、分区表等） |
| `idf.py build` | 编译项目 |
| `idf.py flash` | 烧录到开发板（默认 `/dev/ttyUSB0`） |
| `idf.py monitor` | 打开串口监视器（115200 baud） |
| `idf.py flash monitor` | 烧录后立即监视 |
| `idf.py fullclean` | 清除全部编译产物 |

指定端口：`idf.py -p /dev/ttyACM0 flash monitor`

#### OSRCORE 硬件资源总览

| 外设 | GPIO | 驱动/协议 | 备注 |
|------|------|-----------|------|
| WS2812B LED | GPIO45 | RMT TX | GRB 字节序 |
| 无源蜂鸣器 | GPIO42 | LEDC TIMER1 CH2 | 10-bit，50% 占空比 |
| ESC 油门 | GPIO1 | LEDC TIMER0 CH0 | 50 Hz，14-bit |
| 舵机转向 | GPIO2 | LEDC TIMER0 CH1 | 50 Hz，14-bit |
| 编码器 EA | GPIO3 | PCNT edge | 512 PPR |
| 编码器 EB | GPIO9 | PCNT level | 减速比 10.55 |
| I2C SDA | GPIO10 | I2C_NUM_0 | 400 kHz |
| I2C SCL | GPIO11 | I2C_NUM_0 | |
| QMI8658 IMU | I2C 0x6B | I2C | ±8g / ±2048 dps |
| SBUS RX | GPIO44 | UART0 | 100k 8E2 反相 |
| 电池 ADC | GPIO4 | ADC1 CH3 | 分压检测 |
| 电池使能 | GPIO16 | GPIO OUTPUT | 高电平使能 |
| USB CDC | 内置 | USB Serial JTAG | 控制台 |

#### USB CDC 控制台配置

在 `sdkconfig.defaults` 中加入：

```
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y
CONFIG_ESP_CONSOLE_SECONDARY_NONE=y
```

或在代码中手动初始化：

```c
usb_serial_jtag_driver_config_t cfg = {
    .rx_buffer_size = 256, .tx_buffer_size = 256
};
usb_serial_jtag_driver_install(&cfg);
esp_vfs_usb_serial_jtag_use_driver();
```

### 0.4 程序学习

ESP-IDF 应用程序的入口是 `app_main()`，运行在 FreeRTOS 任务中（默认栈 4 KB，优先级 1）。最小程序框架：

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

`ESP_LOGI` 宏输出带时间戳和标签的日志，级别从低到高：`LOGV < LOGD < LOGI < LOGW < LOGE`。

### 0.5 课程总结

本章完成了 ESP-IDF 工具链的安装，掌握了 `idf.py` 的核心工作流，并对 OSRCORE 开发板的全部外设资源有了整体认识。从第 1 章起，每章聚焦一个外设，逐步构建完整的机器人控制系统。

---

## 第 1 章  WS2812B 彩色 LED（RMT 驱动）

### 1.1 知识要点

- WS2812B 单线串行协议的时序要求（T0H/T0L/T1H/T1L/Reset）
- ESP-IDF RMT TX 新 API（v5.x）的使用方式
- 自定义 RMT 编码器（`rmt_encoder_t`）的实现方法
- GRB 字节序与颜色映射

### 1.2 课程内容

WS2812B 是一种集成驱动 IC 的 RGB LED，通过单根数据线传输 GRB 颜色数据。ESP32-S3 的 RMT（Remote Control Transceiver）外设能以硬件精度生成符合 WS2812B 时序要求的脉冲序列，无需占用 CPU 资源。

本章实现一个颜色循环程序：红 → 绿 → 蓝 → 熄灭，每 500 ms 切换一次。

### 1.3 基础学习

#### WS2812B 时序

WS2812B 使用归零码（NRZ）编码，每个 bit 由高低电平的持续时间决定：

| 符号 | 高电平 | 低电平 | 含义 |
|------|--------|--------|------|
| 0 码 | 400 ns (T0H) | 850 ns (T0L) | 逻辑 0 |
| 1 码 | 800 ns (T1H) | 450 ns (T1L) | 逻辑 1 |
| Reset | — | ≥ 50 µs | 帧结束 |

每颗 LED 接收 24 bit（GRB 顺序，MSB 先），多颗级联时数据自动透传。

RMT 时钟配置为 80 MHz（12.5 ns/tick），对应 tick 数：

```
T0H = 32 ticks  T0L = 68 ticks
T1H = 64 ticks  T1L = 36 ticks
Reset = 4000 ticks (50 µs)
```

#### RMT 编码器架构

ESP-IDF v5.x RMT 采用编码器链式结构：
- `rmt_bytes_encoder`：将字节流按位展开为 RMT 符号
- `rmt_copy_encoder`：直接复制预定义符号（用于 Reset 脉冲）
- 自定义编码器组合两者，实现完整的 WS2812B 帧

### 1.4 程序学习

**创建 TX 通道与字节编码器：**

```c
rmt_tx_channel_config_t ch_cfg = {
    .gpio_num          = 45,           // LED_PIN
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

**发送颜色（GRB 字节序）：**

```c
static void led_set(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t grb[3] = { g, r, b };
    rmt_transmit_config_t tx_cfg = { .loop_count = 0 };
    rmt_transmit(s_channel, s_encoder, grb, sizeof(grb), &tx_cfg);
    rmt_tx_wait_all_done(s_channel, portMAX_DELAY);
}
```

### 1.5 课程总结

本章掌握了 WS2812B 的单线时序原理，学会了使用 ESP-IDF RMT TX 新 API 和自定义编码器驱动 LED。RMT 外设的硬件精度保证了时序的准确性，这一模式也可扩展到驱动 LED 灯带。

---

## 第 2 章  无源蜂鸣器（LEDC PWM 音调控制）

### 2.1 知识要点

- LEDC（LED Control）外设的定时器与通道配置
- 通过动态修改 PWM 频率实现音调变化
- 占空比与音量的关系（50% 占空比驱动无源蜂鸣器）
- 音符频率表与旋律播放逻辑

### 2.2 课程内容

无源蜂鸣器需要外部提供特定频率的方波才能发声，频率决定音调。ESP32-S3 的 LEDC 外设可以灵活配置 PWM 频率，非常适合驱动蜂鸣器播放旋律。本章实现 C 大调音阶的上行与下行演奏。

### 2.3 基础学习

#### LEDC 外设结构

LEDC 包含 4 个定时器和 8 个通道（低速模式）：
- **定时器**：配置时钟源、分辨率（bit 数）、频率
- **通道**：绑定到定时器，配置 GPIO 和占空比

蜂鸣器使用 LEDC_TIMER_1，10-bit 分辨率（1024 步），占空比 512（50%）时输出对称方波。

改变音调只需调用 `ledc_set_freq()`，无需重新初始化通道。静音时将占空比置 0。

#### 音符频率（Hz）

```
C4=262  D4=294  E4=330  F4=349
G4=392  A4=440  B4=494  C5=523
```

### 2.4 程序学习

**初始化定时器与通道：**

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

**播放单个音符：**

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

### 2.5 课程总结

本章学会了使用 LEDC 外设驱动无源蜂鸣器，掌握了通过动态修改频率实现音调控制的方法。LEDC 是 ESP32-S3 上最通用的 PWM 外设，后续章节的 ESC 和舵机控制也将使用它。

---

## 第 3 章  PWM 舵机与 ESC 控制

### 3.1 知识要点

- 标准 RC PWM 协议：50 Hz 周期，1000–2000 µs 脉宽
- LEDC 14-bit 分辨率下的占空比计算
- ESC 上电解锁流程（中立位保持 2 秒）
- 双通道同步配置（油门 + 转向）

### 3.2 课程内容

电调（ESC）和舵机均使用标准 RC PWM 协议控制：50 Hz 重复频率，脉宽 1000 µs 对应最小值，1500 µs 对应中立，2000 µs 对应最大值。OSRCORE 使用 LEDC TIMER0 的两个通道分别驱动 GPIO1（油门）和 GPIO2（转向）。

### 3.3 基础学习

#### RC PWM 时序

```
周期 = 20 ms (50 Hz)
脉宽范围：1000 µs（最小）~ 2000 µs（最大）
中立位：1500 µs
```

#### 14-bit 分辨率下的占空比计算

APB 时钟 80 MHz，14-bit 分辨率（16384 步）：

```
period_ticks = 80_000_000 / 50 = 1_600_000
1 µs = 80 ticks
duty = pulse_us × 80
```

例：1500 µs → duty = 120000；2000 µs → duty = 160000。

宏定义：`#define US_TO_DUTY(us) ((us) * 80)`

#### ESC 解锁流程

大多数 ESC 上电后需要检测到中立位信号（1500 µs）才会解锁，通常需要保持 1–3 秒。未解锁的 ESC 不响应油门指令。

### 3.4 程序学习

**定时器与双通道初始化：**

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
    .gpio_num = 1, .channel = LEDC_CHANNEL_0,
    .timer_sel = LEDC_TIMER_0, .duty = US_TO_DUTY(1500),
};
ledc_channel_config(&ch_throttle);

ledc_channel_config_t ch_steering = {
    .gpio_num = 2, .channel = LEDC_CHANNEL_1,
    .timer_sel = LEDC_TIMER_0, .duty = US_TO_DUTY(1500),
};
ledc_channel_config(&ch_steering);
```

**设置脉宽与 ESC 解锁：**

```c
static void set_pulse(ledc_channel_t ch, uint32_t pulse_us)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, US_TO_DUTY(pulse_us));
    ledc_update_duty(LEDC_LOW_SPEED_MODE, ch);
}

// ESC 解锁：中立位保持 2 秒
set_pulse(CH_THROTTLE, 1500);
set_pulse(CH_STEERING, 1500);
vTaskDelay(pdMS_TO_TICKS(2000));
```

### 3.5 课程总结

本章掌握了 RC PWM 协议的原理和 LEDC 14-bit 精度配置，实现了 ESC 解锁和舵机扫描。`US_TO_DUTY()` 宏将物理脉宽直接映射为寄存器值，是后续 PID 控制输出的基础接口。


---

## 第 4 章  SBUS 遥控接收

### 4.1 知识要点

- SBUS 协议帧格式：25 字节，100000 baud，8E2，信号反相
- 16 通道 11-bit 数据的位域解包算法
- ESP-IDF UART 驱动配置与信号反相
- Failsafe 与 Lost Frame 标志位的处理

### 4.2 课程内容

SBUS 是 Futaba 开发的数字遥控协议，广泛用于航模和机器人领域。它通过单根串行线传输 16 个模拟通道和 2 个数字通道的数据，每帧 25 字节，以 14 ms（高速模式）或 7 ms 间隔发送。OSRCORE 使用 UART0 的 GPIO44 接收 SBUS 信号，硬件反相器将 SBUS 的反相逻辑还原为标准 UART 电平。

### 4.3 基础学习

#### SBUS 帧格式

```
字节 0：0x0F（帧头）
字节 1-22：22 字节数据（16 通道 × 11 bit = 176 bit）
字节 23：标志字节
  bit2 = failsafe（接收机失联保护激活）
  bit3 = lost_frame（帧丢失）
字节 24：0x00（帧尾）
```

通道值范围：240（最小）~ 1810（最大），中立约 1024。

#### 位域解包

16 个通道的 11-bit 数据紧密打包在 22 字节中，需要按位提取：

```c
ch[0] = ((buf[1]      | buf[2]  << 8) & 0x07FF);
ch[1] = ((buf[2] >> 3 | buf[3]  << 5) & 0x07FF);
ch[2] = ((buf[3] >> 6 | buf[4]  << 2 | buf[5] << 10) & 0x07FF);
// ... 以此类推
```

#### UART 配置要点

SBUS 使用非标准参数：100000 baud（非 115200）、偶校验（8E2）、信号反相。ESP-IDF 通过 `uart_set_line_inverse()` 在硬件层面处理反相，无需外部电路。

### 4.4 程序学习

**UART 初始化（含反相）：**

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

**帧同步与解析：**

```c
// 同步到帧头 0x0F
uint8_t byte;
do {
    uart_read_bytes(UART_NUM_0, &byte, 1, portMAX_DELAY);
} while (byte != 0x0F);

buf[0] = byte;
int got = uart_read_bytes(UART_NUM_0, buf + 1, 24, pdMS_TO_TICKS(10));
if (got == 24 && sbus_parse(buf, &data)) {
    if (!data.failsafe) {
        // 处理通道数据
    }
}
```

**SBUS 通道值转换为 PWM 脉宽（µs）：**

```c
// SBUS 范围 240-1810 → PWM 1000-2000 µs
#define SBUS_TO_US(v) ((uint32_t)(((v) - 240) * 1000 / 1570 + 1000))
```

### 4.5 课程总结

本章掌握了 SBUS 协议的帧格式和位域解包算法，学会了配置 ESP-IDF UART 驱动处理非标准波特率和信号反相。SBUS 接收是机器人遥控操作的基础，第 11 章将把它集成到完整控制系统中。

---

## 第 5 章  QMI8658 IMU（I2C 读取）

### 5.1 知识要点

- ESP-IDF I2C Master 新 API（v5.x）的总线与设备配置
- QMI8658 寄存器映射与初始化序列
- 加速度计（±8g）和陀螺仪（±2048 dps）的量程与灵敏度
- 原始数据到物理量的换算公式

### 5.2 课程内容

QMI8658 是一款六轴惯性测量单元（IMU），集成三轴加速度计和三轴陀螺仪，通过 I2C 接口与 ESP32-S3 通信。OSRCORE 将其配置为 1000 Hz 输出速率，为姿态解算提供高频传感器数据。

### 5.3 基础学习

#### I2C Master 新 API 架构

ESP-IDF v5.x 引入了新的 I2C Master API，采用总线-设备两级结构：
- `i2c_new_master_bus()`：创建总线句柄，配置 SDA/SCL 引脚和时钟
- `i2c_master_bus_add_device()`：在总线上添加设备，配置地址和速率
- `i2c_master_transmit()` / `i2c_master_transmit_receive()`：执行传输

#### QMI8658 关键寄存器

| 寄存器 | 地址 | 说明 |
|--------|------|------|
| WHO_AM_I | 0x00 | 芯片 ID（应为 0x05） |
| CTRL1 | 0x02 | SPI/I2C 配置 |
| CTRL2 | 0x03 | 加速度计量程/ODR |
| CTRL3 | 0x04 | 陀螺仪量程/ODR |
| CTRL7 | 0x08 | 使能加速度计+陀螺仪 |
| AX_L | 0x35 | 加速度 X 低字节（共 12 字节） |

#### 数据换算

```
加速度（g）= raw_int16 / 4096.0   （±8g 量程，灵敏度 4096 LSB/g）
角速度（dps）= raw_int16 / 16.0   （±2048 dps 量程，灵敏度 16 LSB/dps）
温度（°C）= raw_int16 / 256.0 + 25.0
```

### 5.4 程序学习

**I2C 总线与设备初始化：**

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

**读取寄存器（写地址后读数据）：**

```c
static esp_err_t qmi_read(i2c_master_dev_handle_t dev,
                           uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_transmit_receive(dev, &reg, 1, buf, len, 100);
}
```

**读取并换算 IMU 数据：**

```c
qmi8658_data_t d;
if (qmi8658_read(&d)) {
    printf("ax=%.3f ay=%.3f az=%.3f  gx=%.2f gy=%.2f gz=%.2f  T=%.1f\n",
           d.ax, d.ay, d.az, d.gx, d.gy, d.gz, d.temp);
}
```

### 5.5 课程总结

本章掌握了 ESP-IDF I2C Master 新 API 的使用方式，学会了 QMI8658 的初始化和数据读取。IMU 数据是姿态解算的原始输入，第 10 章将在此基础上实现 Madgwick AHRS 算法。

---

## 第 6 章  正交编码器速度测量（PCNT）

### 6.1 知识要点

- 正交编码器的工作原理（A/B 相，方向判断）
- ESP-IDF PCNT（脉冲计数）新 API 的配置
- 溢出回调与累积计数的实现
- 脉冲数到线速度的换算公式

### 6.2 课程内容

OSRCORE 配备了一个带减速箱的直流电机，电机轴上安装了 512 PPR 的正交编码器。通过 PCNT 外设对编码器脉冲计数，结合减速比（10.55）和轮径（42.5 mm），可以精确计算车轮线速度，为 PID 闭环控制提供反馈。

### 6.3 基础学习

#### 正交编码器原理

正交编码器输出两路相位差 90° 的方波（A 相和 B 相）：
- 正转时：A 相超前 B 相 90°
- 反转时：B 相超前 A 相 90°

通过检测 A 相边沿时 B 相的电平，可以判断旋转方向。

PCNT 配置为：A 相（EA）上升沿计数，B 相（EB）电平控制方向（高电平正计数，低电平反计数）。

#### 速度换算公式

```
每转脉冲数 = PPR × 减速比 = 512 × 10.55 = 5401.6
轮周长 = 2π × 0.0425 = 0.2670 m
每脉冲距离 = 0.2670 / 5401.6 = 4.943×10⁻⁵ m/pulse
速度 (m/s) = Δpulses × 每脉冲距离 / Δt
```

#### PCNT 溢出处理

PCNT 硬件计数器为 16-bit（-32768 ~ 32767），高速旋转时会溢出。通过注册 `on_reach` 回调，在达到上下限时将溢出量累加到软件计数器，实现无限范围计数。

### 6.4 程序学习

**PCNT 初始化（单边沿模式）：**

```c
pcnt_unit_config_t unit_cfg = {
    .low_limit  = -32768,
    .high_limit =  32767,
};
pcnt_new_unit(&unit_cfg, &s_unit);

pcnt_chan_config_t chan_a = {
    .edge_gpio_num  = 3,   // EA
    .level_gpio_num = 9,   // EB
};
pcnt_channel_handle_t ch_a;
pcnt_new_channel(s_unit, &chan_a, &ch_a);
pcnt_channel_set_edge_action(ch_a,
    PCNT_CHANNEL_EDGE_ACTION_INCREASE,  // 上升沿 +1
    PCNT_CHANNEL_EDGE_ACTION_HOLD);     // 下降沿不计
pcnt_channel_set_level_action(ch_a,
    PCNT_CHANNEL_LEVEL_ACTION_KEEP,     // EB 高：正方向
    PCNT_CHANNEL_LEVEL_ACTION_INVERSE); // EB 低：反方向
```

**溢出回调与累积读取：**

```c
static bool IRAM_ATTR pcnt_overflow_cb(pcnt_unit_handle_t unit,
                                        const pcnt_watch_event_data_t *edata,
                                        void *user_ctx)
{
    int32_t *accum = (int32_t *)user_ctx;
    *accum += edata->watch_point_val;
    return false;
}

static int32_t encoder_get_count(void)
{
    int raw = 0;
    pcnt_unit_get_count(s_unit, &raw);
    return s_accum + raw;
}
```

**速度计算（20 ms 周期）：**

```c
int32_t delta = encoder_get_count() - last_count;
float speed = delta * DIST_PER_PULSE / 0.020f;  // m/s
```

### 6.5 课程总结

本章掌握了正交编码器的工作原理和 PCNT 外设的配置，实现了带溢出处理的精确脉冲计数和线速度计算。这是 PID 速度闭环控制的核心反馈环节。

---

## 第 7 章  NVS 参数持久化存储

### 7.1 知识要点

- NVS（Non-Volatile Storage）的分区结构与命名空间
- 基本类型（u8/i32/float）和 blob 的读写 API
- 参数初始化标志位的设计模式
- USB CDC 串口命令解析与参数热更新

### 7.2 课程内容

机器人控制系统中，PID 参数等调试数据需要在断电后保留。ESP32-S3 的 NVS 提供了一个键值存储系统，基于 Flash 的磨损均衡分区，支持字符串、整数、浮点数和任意二进制数据（blob）的持久化存储。本章演示将 PID 参数以 blob 形式存入 NVS，并通过 USB CDC 串口命令实时修改。

### 7.3 基础学习

#### NVS 结构

NVS 使用命名空间（namespace）隔离不同模块的数据，每个命名空间下可以存储多个键值对：

```
NVS Flash
└── namespace: "pid_params"
    ├── key: "init"   → u8（是否已初始化）
    └── key: "params" → blob（pid_params_t 结构体）
```

#### 初始化标志模式

首次上电时 NVS 为空，需要写入默认值。通过一个 `init` 标志位判断是否已初始化，避免每次重启都覆盖用户修改的参数：

```c
uint8_t inited = 0;
nvs_get_u8(h, "init", &inited);
if (!inited) {
    // 使用默认值，不从 NVS 读取
}
```

#### Blob 读写

Blob 可以存储任意结构体，读取时需要传入缓冲区大小：

```c
// 写入
nvs_set_blob(h, "params", &g_params, sizeof(pid_params_t));
nvs_commit(h);  // 必须 commit 才会真正写入 Flash

// 读取
size_t sz = sizeof(pid_params_t);
nvs_get_blob(h, "params", &g_params, &sz);
```

### 7.4 程序学习

**加载参数（带默认值回退）：**

```c
static void params_load(void)
{
    nvs_handle_t h;
    if (nvs_open("pid_params", NVS_READONLY, &h) != ESP_OK) goto defaults;

    uint8_t inited = 0;
    nvs_get_u8(h, "init", &inited);
    if (!inited) { nvs_close(h); goto defaults; }

    size_t sz = sizeof(pid_params_t);
    if (nvs_get_blob(h, "params", &g_params, &sz) == ESP_OK) {
        nvs_close(h);
        return;
    }
defaults:
    g_params.kp = 447.0f;
    g_params.ki = 4.7f;
    g_params.kd = 47.0f;
}
```

**保存参数：**

```c
static void params_save(void)
{
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open("pid_params", NVS_READWRITE, &h));
    nvs_set_blob(h, "params", &g_params, sizeof(pid_params_t));
    nvs_set_u8(h, "init", 1);
    nvs_commit(h);
    nvs_close(h);
}
```

**串口命令解析：**

```c
char line[64];
if (fgets(line, sizeof(line), stdin)) {
    float val;
    if (sscanf(line, "kp %f", &val) == 1) {
        g_params.kp = val;
        params_save();
    } else if (strcmp(line, "show") == 0) {
        printf("kp=%.2f ki=%.2f kd=%.2f\n",
               g_params.kp, g_params.ki, g_params.kd);
    }
}
```

### 7.5 课程总结

本章掌握了 NVS 的命名空间、blob 读写和 commit 机制，实现了 PID 参数的断电保持和串口热更新。这一模式适用于所有需要持久化的配置参数，是嵌入式系统调试的重要工具。


---

## 第 8 章  FreeRTOS 多任务架构

### 8.1 知识要点

- FreeRTOS 任务创建、优先级与栈大小配置
- ESP32-S3 双核绑定（`xTaskCreatePinnedToCore`）
- 任务间通信：Queue（队列）和 Mutex（互斥锁）
- 实时任务与通信任务的核心分离架构

### 8.2 课程内容

ESP32-S3 搭载双核 Xtensa LX7 处理器（Core 0 和 Core 1），FreeRTOS 支持对称多处理（SMP）。合理的任务架构是机器人控制系统实时性的基础：将传感器采样和控制计算绑定到 Core 1（实时核），将通信和日志绑定到 Core 0（通信核），避免相互干扰。

### 8.3 基础学习

#### 任务优先级规划

FreeRTOS 优先级数值越大越高，`configMAX_PRIORITIES` 默认为 25。OSRCORE 的三任务架构：

| 任务 | 核心 | 优先级 | 周期 | 职责 |
|------|------|--------|------|------|
| task_sensor | Core 1 | 5 | 100 ms | 传感器采样 |
| task_control | Core 1 | 4 | 20 ms | 控制计算 |
| task_comm | Core 0 | 3 | 1000 ms | 状态上报 |

#### Queue 与 Mutex

- **Queue**：用于任务间传递数据（生产者-消费者模式）。`xQueueOverwrite()` 适合只需要最新值的场景（如传感器数据）。
- **Mutex**：用于保护共享数据结构，防止并发访问导致数据撕裂。

#### 双核绑定

```c
// 绑定到 Core 1（实时任务）
xTaskCreatePinnedToCore(task_sensor, "sensor", 4096, NULL, 5, NULL, 1);
// 绑定到 Core 0（通信任务）
xTaskCreatePinnedToCore(task_comm, "comm", 4096, NULL, 3, NULL, 0);
```

不绑定核心时使用 `xTaskCreate()`，FreeRTOS 自动调度。

### 8.4 程序学习

**共享状态与同步原语：**

```c
typedef struct {
    float sensor_val;
    float control_out;
    uint32_t tick;
} shared_state_t;

static shared_state_t g_state;
static SemaphoreHandle_t g_mutex;
static QueueHandle_t g_sensor_q;  // 深度为 1 的队列

// 初始化
g_mutex    = xSemaphoreCreateMutex();
g_sensor_q = xQueueCreate(1, sizeof(sensor_msg_t));
```

**传感器任务（写队列 + 更新共享状态）：**

```c
static void task_sensor(void *arg)
{
    sensor_msg_t msg;
    while (1) {
        msg.value        = read_sensor();
        msg.timestamp_us = esp_timer_get_time();
        xQueueOverwrite(g_sensor_q, &msg);  // 覆盖旧值

        xSemaphoreTake(g_mutex, portMAX_DELAY);
        g_state.sensor_val = msg.value;
        g_state.tick++;
        xSemaphoreGive(g_mutex);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

**控制任务（读队列，执行 PI 控制）：**

```c
static void task_control(void *arg)
{
    sensor_msg_t msg;
    float integral = 0.0f;
    const float kp = 2.0f, ki = 0.1f, target = 1.25f;

    while (1) {
        if (xQueuePeek(g_sensor_q, &msg, 0) == pdTRUE) {
            float err = target - msg.value;
            integral += err * 0.02f;
            float out = kp * err + ki * integral;

            xSemaphoreTake(g_mutex, portMAX_DELAY);
            g_state.control_out = out;
            xSemaphoreGive(g_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
```

### 8.5 课程总结

本章建立了 OSRCORE 的三任务双核架构，掌握了 Queue 和 Mutex 的使用模式。这一架构是后续 PID 闭环控制和完整示例的基础框架，实时任务与通信任务的核心分离保证了控制周期的确定性。

---

## 第 9 章  PID 电机速度闭环控制

### 9.1 知识要点

- 离散 PID 算法的实现（增量式积分，积分限幅，死区）
- 一阶低通滤波器消除编码器速度噪声
- PID 输出到 ESC 脉宽的映射与钳位
- 串口命令设置目标速度的实时调试方法

### 9.2 课程内容

本章将编码器测速（第 6 章）和 ESC PWM 控制（第 3 章）结合，实现电机速度的 PID 闭环控制。控制周期为 20 ms，目标速度通过 USB CDC 串口命令设置，实际速度经一阶低通滤波后作为 PID 反馈。

### 9.3 基础学习

#### 离散 PID 算法

位置式 PID 公式：

```
e(k) = target - measured
u(k) = Kp × e(k) + Ki × Σe(k)×dt + Kd × (e(k) - e(k-1)) / dt
```

实现要点：
- **积分限幅**：防止积分饱和（windup），限制 `integral` 的绝对值
- **死区**：误差绝对值小于 deadband 时不累积积分，消除静态抖动
- **输出钳位**：将 PID 输出限制在 ESC 可接受范围内

#### 一阶低通滤波

编码器速度在低速时噪声较大，使用一阶低通滤波平滑：

```
filtered = α × raw + (1 - α) × filtered_prev
```

α = 0.15（OSRCORE 默认值），α 越小滤波越强但响应越慢。

#### PID 参数（OSRCORE 默认值）

```
Kp = 447.0   Ki = 4.7   Kd = 47.0
积分限幅 = 1000.0   死区 = 0.05 m/s
```

### 9.4 程序学习

**PID 结构体与计算函数：**

```c
typedef struct {
    float kp, ki, kd;
    float max_integral, deadband;
    float integral, last_error;
} pid_t;

float pid_calc(pid_t *p, float target, float measured, float dt)
{
    float err = target - measured;
    if (fabsf(err) > p->deadband) {
        p->integral += err * dt;
        if (p->integral >  p->max_integral) p->integral =  p->max_integral;
        if (p->integral < -p->max_integral) p->integral = -p->max_integral;
    }
    float deriv = (err - p->last_error) / dt;
    p->last_error = err;
    return p->kp * err + p->ki * p->integral + p->kd * deriv;
}
```

**控制任务（20 ms 周期）：**

```c
static void task_control(void *arg)
{
    pid_t pid;
    pid_init(&pid, 447.0f, 4.7f, 47.0f, 1000.0f, 0.05f);
    int32_t last_count = encoder_count();
    float filtered_speed = 0.0f;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(20));

        int32_t delta = encoder_count() - last_count;
        last_count = encoder_count();

        float raw_speed = delta * DIST_PER_PULSE / 0.020f;
        filtered_speed  = 0.15f * raw_speed + 0.85f * filtered_speed;

        float out = pid_calc(&pid, s_target_speed, filtered_speed, 0.020f);
        int32_t pulse = 1500 + (int32_t)out;
        // 钳位到 [1000, 2000]
        if (pulse < 1000) pulse = 1000;
        if (pulse > 2000) pulse = 2000;
        esc_set((uint32_t)pulse);
    }
}
```

### 9.5 课程总结

本章实现了完整的速度 PID 闭环控制，掌握了积分限幅、死区和低通滤波等工程实践技巧。PID 参数调整方法：先增大 Kp 直到出现轻微振荡，再加 Ki 消除稳态误差，最后加 Kd 抑制超调。

---

## 第 10 章  Madgwick AHRS 姿态解算

### 10.1 知识要点

- 四元数表示姿态的优势（无万向锁，计算高效）
- Madgwick 6-DOF 算法原理（梯度下降优化）
- β 参数对收敛速度与噪声抑制的影响
- 四元数到欧拉角（Roll/Pitch/Yaw）的转换公式

### 10.2 课程内容

姿态航向参考系统（AHRS）将 IMU 的加速度计和陀螺仪数据融合，输出稳定的姿态估计。Madgwick 算法是一种计算效率高、适合嵌入式系统的互补滤波算法。OSRCORE 以 200 Hz（5 ms 周期）运行 AHRS，为平衡控制等应用提供实时姿态数据。

### 10.3 基础学习

#### 四元数表示

四元数 q = [q0, q1, q2, q3]（标量 + 向量部分）可以无奇异点地表示三维旋转。初始值为单位四元数 [1, 0, 0, 0]（无旋转）。

#### Madgwick 算法原理

6-DOF 模式（无磁力计）：
1. 陀螺仪积分：用角速度更新四元数（预测步）
2. 加速度计修正：通过梯度下降最小化重力向量与测量值的误差（修正步）
3. β 参数控制修正步长：β 越大收敛越快但噪声越大，β=0.1 是典型平衡点

#### 快速反平方根优化

Madgwick 算法需要频繁归一化四元数，使用 Quake III 的快速反平方根（`invSqrt`）可以显著减少计算量：

```c
static float invSqrt(float x) {
    float halfx = 0.5f * x;
    float y = x;
    long i = *(long*)&y;
    i = 0x5f3759df - (i >> 1);
    y = *(float*)&i;
    y = y * (1.5f - (halfx * y * y));
    return y;
}
```

#### 四元数到欧拉角

```
Roll  = atan2(2(q0q1 + q2q3), 1 - 2(q1² + q2²))
Pitch = asin(2(q0q2 - q3q1))
Yaw   = atan2(2(q0q3 + q1q2), 1 - 2(q2² + q3²))
```

### 10.4 程序学习

**初始化与更新：**

```c
madgwick_t g_ahrs;
madgwick_init(&g_ahrs, 0.1f);  // beta = 0.1

// 每 5ms 调用一次（200 Hz）
qmi8658_data_t d;
if (qmi8658_read(&d)) {
    madgwick_update_imu(&g_ahrs,
        d.gx, d.gy, d.gz,   // 陀螺仪 deg/s
        d.ax, d.ay, d.az);  // 加速度计 g
}
```

**读取欧拉角：**

```c
float roll, pitch, yaw;
madgwick_get_euler(&g_ahrs, &roll, &pitch, &yaw);
printf("R=%.1f P=%.1f Y=%.1f\n", roll, pitch, yaw);
```

**IMU 任务（Core 1，P5，5ms）：**

```c
static void task_imu(void *arg)
{
    qmi8658_data_t d;
    int print_cnt = 0;
    while (1) {
        if (qmi8658_read(&d)) {
            madgwick_update_imu(&g_ahrs, d.gx, d.gy, d.gz, d.ax, d.ay, d.az);
            if (++print_cnt >= 20) {  // 每 100ms 打印
                print_cnt = 0;
                float roll, pitch, yaw;
                madgwick_get_euler(&g_ahrs, &roll, &pitch, &yaw);
                printf("q: %.4f %.4f %.4f %.4f  R=%.1f P=%.1f Y=%.1f\n",
                       g_ahrs.q0, g_ahrs.q1, g_ahrs.q2, g_ahrs.q3,
                       roll, pitch, yaw);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
```

### 10.5 课程总结

本章掌握了 Madgwick AHRS 算法的原理和实现，学会了四元数姿态表示和欧拉角转换。200 Hz 的更新频率为动态运动提供了足够的时间分辨率，β=0.1 在静态精度和动态响应之间取得了良好平衡。

---

## 第 11 章  完整机器人示例

### 11.1 知识要点

- 多外设协同初始化的顺序与依赖关系
- 三任务双核架构的完整实现
- SBUS 遥控与串口命令的优先级仲裁
- 串口控制超时安全机制
- 电池使能 GPIO 的初始化

### 11.2 课程内容

本章将前 10 章的所有外设整合到一个完整的机器人控制程序中，实现与 OSRCORE 固件相同的三任务架构。系统支持两种控制模式：SBUS 遥控模式（CH7 < 1500）和串口命令模式（CH7 ≥ 1500），遥控模式优先级更高。串口控制设有 500 ms 超时保护，防止通信中断时车辆失控。

### 11.3 基础学习

#### 三任务架构

```
Core 1  task_imu     P5  5ms   QMI8658 → Madgwick AHRS → 更新 g_state.quat
Core 1  task_control P4  20ms  编码器 → LPF → PID → ESC/舵机输出
Core 0  task_comm    P3  1ms   SBUS 解码 / USB CDC 命令 / 状态打印
```

#### 控制模式仲裁

```
SBUS CH7（index 6）< 1500  → 遥控模式：SBUS CH2 → 速度，CH0 → 转向
SBUS CH7（index 6）≥ 1500  → 串口模式：USB CDC 命令控制
Failsafe 激活              → 强制停止
```

#### 串口超时安全

串口控制模式下，若 500 ms 内未收到新命令，自动将速度和转向复位到中立位，防止通信中断导致失控。

#### 初始化顺序

```
1. USB CDC 控制台
2. 电池使能（GPIO16 高电平）
3. NVS Flash 初始化
4. I2C + QMI8658
5. PCNT 编码器
6. LEDC PWM（ESC + 舵机）
7. UART SBUS
8. Madgwick + PID 初始化
9. ESC 解锁（中立位 2 秒）
10. 创建三个任务
```

### 11.4 程序学习

**全局状态结构体（portMUX 保护）：**

```c
typedef struct {
    float    target_speed;
    uint32_t steering_pulse;
    float    filtered_speed;
    float    quat[4];
    bool     remote_active;
    bool     failsafe;
} app_state_t;

static app_state_t g_state;
static portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
```

**task_comm 中的模式仲裁（核心逻辑）：**

```c
// SBUS 解码后的模式判断
bool remote = (ch[CONTROL_MODE_CH] < 1500);
portENTER_CRITICAL(&g_mux);
g_state.remote_active = remote && !fs;
if (remote && !fs) {
    float vx = ((float)ch[2] - 1024.0f) / 784.0f * 6.0f;
    g_state.target_speed   = vx;
    g_state.steering_pulse = SBUS_TO_US(ch[0]);
}
portEXIT_CRITICAL(&g_mux);

// 串口命令（仅在非遥控模式下生效）
if (!g_state.remote_active) {
    if (sscanf(line, "v %f %f", &v, &s) == 2) {
        g_state.target_speed   = v;
        g_state.steering_pulse = (uint32_t)s;
        last_serial_cmd = esp_timer_get_time();
    }
}

// 串口超时保护（500ms）
if (!g_state.remote_active && last_serial_cmd > 0) {
    if ((esp_timer_get_time() - last_serial_cmd) > 500000) {
        g_state.target_speed   = 0.0f;
        g_state.steering_pulse = 1500;
        last_serial_cmd = 0;
    }
}
```

**app_main 初始化序列：**

```c
void app_main(void)
{
    // USB CDC
    usb_serial_jtag_driver_install(&usb_cfg);
    esp_vfs_usb_serial_jtag_use_driver();

    // 电池使能
    gpio_set_direction(16, GPIO_MODE_OUTPUT);
    gpio_set_level(16, 1);

    ESP_ERROR_CHECK(nvs_flash_init());

    init_i2c_imu();
    init_encoder();
    init_pwm();
    init_sbus();

    madgwick_init(&g_ahrs, 0.1f);
    pid_init(&g_pid, 447.0f, 4.7f, 47.0f, 1000.0f, 0.05f);

    // ESC 解锁
    set_throttle(1500);
    set_steering(1500);
    vTaskDelay(pdMS_TO_TICKS(2000));

    xTaskCreatePinnedToCore(task_imu,     "imu",     4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(task_control, "control", 4096, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(task_comm,    "comm",    8192, NULL, 3, NULL, 0);
}
```

### 11.5 课程总结

本章完成了 OSRCORE 完整机器人控制程序的实现，将 WS2812B、蜂鸣器、ESC、舵机、编码器、IMU、SBUS、NVS 和 FreeRTOS 多任务架构全部整合在一起。三任务双核架构保证了 IMU 采样和 PID 控制的实时性，SBUS/串口双模式控制提供了灵活的操控方式，超时安全机制保障了系统的可靠性。

至此，OSRCORE 开发板教程全部完成。建议按以下路径深入学习：
- 调整 PID 参数，观察速度响应曲线
- 在 task_imu 中加入姿态反馈，实现倾斜补偿
- 扩展 SBUS 通道映射，支持更多遥控功能
- 使用 ESP-IDF 的 WiFi/BLE 组件，添加无线调试界面

