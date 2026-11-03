# OSRCORE 开发板嵌入式软件教程

> 适用硬件：OSRCORE（ESP32-S3-WROOM-1U N16R8）
> 工具链：ESP-IDF v5.3+

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
| 编码器 EA | GPIO3 | PCNT edge | 1024 PPR |
| 编码器 EB | GPIO9 | PCNT level | 减速比 10.55 |
| I2C SDA | GPIO10 | I2C_NUM_0 | 400 kHz |
| I2C SCL | GPIO11 | I2C_NUM_0 | |
| QMI8658 IMU | I2C 0x6B | I2C | ±4g / ±1024 dps |
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

> 📂 **示例代码**：[01_blink_led](https://github.com/osrbot/Development-Board-OSRCORE-Example/tree/main/01_blink_led)

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

RMT 时钟配置为 80 MHz（12.5 ns/tick），tick 数由时序规格直接推导：

```
T0H = 400/12.5 = 32 ticks   T0L = 850/12.5 = 68 ticks
T1H = 800/12.5 = 64 ticks   T1L = 450/12.5 = 36 ticks
Reset = 50000/12.5 = 4000 ticks (≥ 50 µs)
```

用整数除法写成宏：`#define T0H (400 * 80 / 1000)`，编译器在编译期计算，无运行时开销。

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

/* WS2812B bit timing: formula-based macros, computed at compile time */
#define T0H  (400 * 80 / 1000)   // 32 ticks
#define T0L  (850 * 80 / 1000)   // 68 ticks
#define T1H  (800 * 80 / 1000)   // 64 ticks
#define T1L  (450 * 80 / 1000)   // 36 ticks
#define RESET_TICKS (50000 * 80 / 1000)  // 4000 ticks

rmt_bytes_encoder_config_t bcfg = {
    .bit0 = { .level0=1, .duration0=T0H, .level1=0, .duration1=T0L },
    .bit1 = { .level0=1, .duration0=T1H, .level1=0, .duration1=T1L },
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
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(s_channel, -1));
}
```

### 1.5 课程总结

本章掌握了 WS2812B 的单线时序原理，学会了使用 ESP-IDF RMT TX 新 API 和自定义编码器驱动 LED。RMT 外设的硬件精度保证了时序的准确性，这一模式也可扩展到驱动 LED 灯带。

---

## 第 2 章  无源蜂鸣器（LEDC PWM 音调控制）

### 本课要完成什么

让板载 GPIO42 蜂鸣器以指定频率重复发声，理解频率与音高、时长与节奏的关系。只需要开发板和 USB 数据线。

### 网页与实板

网页调整频率（220–1760 Hz）和时长（100–1000 ms），开启网页声音后运行。每次发声后静音 100 ms。实板使用 LEDC 产生 50% 占空比的方波，行为与本课参数一致。

打开顶部“连接开发板”，编译当前参数后下载。默认 440 Hz、300 ms；修改参数需要重新编译。完整实现见 `02_buzzer/main/main.c`，默认参数见 `components/osrcore_fw_update/include/osrcore_lesson_config.h`。

### 观察与练习

将频率加倍，比较音高；保持频率不变，只增加时长。听不到声音时先检查是否下载了本课，网页声音开关不控制真板。

### 下载步骤

在顶部连接 USB，编译当前参数，下载完成后重连日志；Wi-Fi 需先由 USB 配网。

### 故障定位

真板不响时核对运行程序名称和参数；网页不响时检查声音开关和浏览器音量。

---

## 第 3 章  PWM 舵机与 ESC 控制

### 本课要完成什么

用固定脉宽控制舵机位置，不在启动后自动扫动。GPIO2 输出 50 Hz PWM，脉宽为 1000–2000 µs，默认 1500 µs；GPIO1 电调信号固定保持 1500 µs 中位。

### 接线

EX2 第 8 行接 GPIO2、VCC_ESC、GND。舵机按其额定电压独立供电并与开发板共地。VCC_ESC 与普通 5 V 引脚不是同一个供电域。先核对网页接口示意中的方向和引脚，不凭线色接线。

### 网页与实板

网页角度只是常见舵机的近似映射，实际转角由舵机型号决定。先尝试 1500 µs，再少量调整。编译并下载后，板卡持续输出选定脉宽；仅拖动网页滑条不会立即驱动真板。

完整实现见 `03_pwm_servo/main/main.c`。连接设备后查看脉宽日志，确认运行版本。

### 代码入口

PWM 初始化与占空比换算均在 main.c；编译参数 OSR_SERVO_PULSE 控制 GPIO2。

### 验收

先核对信号脉宽，再连接机械负载；1500 µs 是否代表居中由实际舵机决定。

---

## 第 4 章  SBUS 遥控接收

> 📂 **示例代码**：[04_sbus_rc](https://github.com/osrbot/Development-Board-OSRCORE-Example/tree/main/04_sbus_rc)

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
  bit2 = lost_frame（帧丢失）
  bit3 = failsafe（接收机失联保护激活）
字节 24：0x00（帧尾）
```

注意：早期文档中 bit2/bit3 的含义有误（两者互换），代码中已按 FrSky/Futaba 规范更正：bit2 = lost_frame，bit3 = failsafe。

通道值范围：240（最小）~ 1810（最大），中立约 1024。

#### 位域解包

16 个通道的 11-bit 数据紧密打包在 22 字节中，需要按位提取。完整 16 通道解包：

```c
ch[0]  = ((buf[1]       | buf[2]  << 8) & 0x07FF);
ch[1]  = ((buf[2]  >> 3 | buf[3]  << 5) & 0x07FF);
ch[2]  = ((buf[3]  >> 6 | buf[4]  << 2 | buf[5] << 10) & 0x07FF);
ch[3]  = ((buf[5]  >> 1 | buf[6]  << 7) & 0x07FF);
ch[4]  = ((buf[6]  >> 4 | buf[7]  << 4) & 0x07FF);
ch[5]  = ((buf[7]  >> 7 | buf[8]  << 1 | buf[9] << 9) & 0x07FF);
ch[6]  = ((buf[9]  >> 2 | buf[10] << 6) & 0x07FF);
ch[7]  = ((buf[10] >> 5 | buf[11] << 3) & 0x07FF);
ch[8]  = ((buf[12]      | buf[13] << 8) & 0x07FF);
ch[9]  = ((buf[13] >> 3 | buf[14] << 5) & 0x07FF);
ch[10] = ((buf[14] >> 6 | buf[15] << 2 | buf[16] << 10) & 0x07FF);
ch[11] = ((buf[16] >> 1 | buf[17] << 7) & 0x07FF);
ch[12] = ((buf[17] >> 4 | buf[18] << 4) & 0x07FF);
ch[13] = ((buf[18] >> 7 | buf[19] << 1 | buf[20] << 9) & 0x07FF);
ch[14] = ((buf[20] >> 2 | buf[21] << 6) & 0x07FF);
ch[15] = ((buf[21] >> 5 | buf[22] << 3) & 0x07FF);

/* 标志字节：bit2=lost_frame，bit3=failsafe */
bool lost_frame = (buf[23] & (1 << 2)) != 0;
bool failsafe   = (buf[23] & (1 << 3)) != 0;
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
    // data.lost_frame = (buf[23] & (1<<2)) != 0
    // data.failsafe   = (buf[23] & (1<<3)) != 0
    if (!data.failsafe && !data.lost_frame) {
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

本章掌握了 SBUS 协议的帧格式和位域解包算法，学会了配置 ESP-IDF UART 驱动处理非标准波特率和信号反相。SBUS 接收是机器人遥控操作的基础，第 12 章将把它集成到完整控制系统中。

---

## 第 5 章  QMI8658 IMU（I2C 读取）

> 📂 **示例代码**：[05_imu_i2c](https://github.com/osrbot/Development-Board-OSRCORE-Example/tree/main/05_imu_i2c)

### 5.1 知识要点

- ESP-IDF I2C Master 新 API（v5.x）的总线与设备配置
- QMI8658 寄存器映射与初始化序列
- 加速度计（±4g）和陀螺仪（±1024 dps）的量程与灵敏度
- 5 样本硬件平均、静止检测与陀螺仪偏置校准
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
| CTRL2 | 0x03 | 加速度计量程/ODR（0x13 = ±4g，1000 Hz） |
| CTRL3 | 0x04 | 陀螺仪量程/ODR（0x63 = ±1024 dps，1000 Hz） |
| CTRL7 | 0x08 | 使能加速度计+陀螺仪 |
| AX_L | 0x35 | 加速度 X 低字节（共 12 字节） |

#### 数据换算

```
加速度（g）= raw_int16 / 8192.0   （±4g 量程，ACCEL_SCALE = 32768/4 = 8192 LSB/g）
角速度（dps）= raw_int16 / 32.0   （±1024 dps 量程，GYRO_SCALE = 32768/1024 = 32 LSB/dps）
温度（°C）= raw_int16 / 256.0 + 25.0
```

#### 5 样本平均与偏置校准

驱动内部对每 5 次原始读取做算术平均后才输出一次 `qmi8658_data_t`，有效降低高频噪声。

陀螺仪偏置分两阶段校准：
1. **启动校准**：上电后采集 100 个静止样本，计算均值作为初始偏置。
2. **在线批量校准**：运行中持续检测静止状态（加速度 Z 轴方差 + 陀螺仪 Z 轴均值），静止超过阈值后累积 1000 个样本更新偏置，防止温漂。

水平偏移（level offset）通过 `qmi8658_set_level_offset(ox, oy, oz)` 设置，从加速度计输出中减去安装误差。

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
/* 初始化时写入量程寄存器 */
write_reg(QMI_REG_CTRL2, 0x13);  // ±4g，1000 Hz ODR
write_reg(QMI_REG_CTRL3, 0x63);  // ±1024 dps，1000 Hz ODR
write_reg(QMI_REG_CTRL7, 0x03);  // 使能加速度计 + 陀螺仪

/* 启动后执行一次静止校准（需保持静止约 100ms） */
qmi8658_calibrate_bias();

/* 读取数据（内部 5 样本平均，每 5 次调用才返回 true） */
qmi8658_data_t d;
if (qmi8658_read(&d)) {
    // d.accelX/Y/Z 单位 g，d.gyroX/Y/Z 单位 dps（已减去偏置）
    printf("ax=%.3f ay=%.3f az=%.3f  gx=%.2f gy=%.2f gz=%.2f  T=%.1f\n",
           d.accelX, d.accelY, d.accelZ,
           d.gyroX,  d.gyroY,  d.gyroZ, d.temp);
}
```

### 5.5 课程总结

本章掌握了 ESP-IDF I2C Master 新 API 的使用方式，学会了 QMI8658 的初始化（±4g / ±1024 dps 量程，CTRL2=0x13，CTRL3=0x63）和数据读取。5 样本平均降低了噪声，两阶段陀螺仪偏置校准（启动 100 样本 + 在线批量 1000 样本）保证了长时间运行的精度。IMU 数据是姿态解算的原始输入，第 10 章将在此基础上实现 Madgwick AHRS 算法。

---

## 第 6 章  正交编码器速度测量（PCNT）

> 📂 **示例代码**：[06_encoder](https://github.com/osrbot/Development-Board-OSRCORE-Example/tree/main/06_encoder)

### 6.1 知识要点

- 正交编码器的工作原理（A/B 相，方向判断）
- ESP-IDF PCNT（脉冲计数）新 API 的配置
- 溢出回调与累积计数的实现
- 脉冲数到线速度的换算公式

### 6.2 课程内容

OSRCORE 配备了一个带减速箱的直流电机，电机轴上安装了 1024 PPR 的正交编码器。通过 PCNT 外设对编码器脉冲计数，结合减速比（10.55）和轮径（42.5 mm），可以精确计算车轮线速度，为 PID 闭环控制提供反馈。

### 6.3 基础学习

#### 正交编码器原理

正交编码器输出两路相位差 90° 的方波（A 相和 B 相）：
- 正转时：A 相超前 B 相 90°
- 反转时：B 相超前 A 相 90°

通过检测 A 相边沿时 B 相的电平，可以判断旋转方向。

PCNT 配置为：A 相（EA）上升沿计数，B 相（EB）电平控制方向（高电平正计数，低电平反计数）。

#### 速度换算公式

```
每转脉冲数 = PPR × 减速比 = 1024 × 10.55 = 10803.2
轮周长 = 2π × 0.0425 = 0.2670 m
每脉冲距离 = 0.2670 / 10803.2 = 2.471×10⁻⁵ m/pulse
速度 (m/s) = Δpulses × 每脉冲距离 / Δt
```

#### PCNT 硬件累加（accum_count）

将 `pcnt_unit_config_t.flags.accum_count = 1` 置位后，PCNT 硬件在计数器到达 watch point 时自动将溢出量累加到内部 64-bit 累加器。`pcnt_unit_get_count()` 直接返回真实累计值，无需软件溢出回调，代码更简洁，也不存在中断延迟导致的计数丢失风险。

### 6.4 程序学习

**PCNT 初始化（accum_count 硬件累加模式）：**

```c
pcnt_unit_config_t unit_cfg = {
    .low_limit  = -30000,
    .high_limit =  30000,
    .flags.accum_count = 1,  // 启用硬件级累加，无需软件溢出回调
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

/* watch point 供硬件累加器使用 */
pcnt_unit_add_watch_point(s_unit,  30000);
pcnt_unit_add_watch_point(s_unit, -30000);

pcnt_unit_enable(s_unit);
pcnt_unit_clear_count(s_unit);
pcnt_unit_start(s_unit);
```

**直接读取累计计数（无需软件 accum 变量）：**

```c
static int32_t encoder_get_count(void)
{
    int raw = 0;
    pcnt_unit_get_count(s_unit, &raw);
    return raw;  // 硬件已累加，直接返回真实计数
}
```

**速度计算（20 ms 周期）：**

```c
int32_t delta = encoder_get_count() - last_count;
float speed = delta * DIST_PER_PULSE / 0.020f;  // m/s
```

### 6.5 课程总结

本章掌握了正交编码器的工作原理和 PCNT 外设的配置，通过 `accum_count=1` 启用硬件级累加，消除了软件溢出回调的复杂性。PPR 从 512 更新为 1024，每转脉冲数 10803.2，线速度计算更精确。这是 PID 速度闭环控制的核心反馈环节。

---

## 第 7 章  NVS 参数持久化存储

### 本课要完成什么

只保存一个 Kp 参数，并在重启后读回，理解 RAM 与 NVS 的区别。本课不运行机器人控制器。

### 操作顺序

1. 设置 Kp（默认 2.5），编译并下载本课。
2. 连接设备日志，先查看 `Loaded Kp=...`；已有保存值会优先读出。
3. 点击“保存 Kp”或发送 `save`，保存本次固件编译的 Kp，观察 `Saved Kp=...`。
4. 点击“重启并读回”或发送 `reboot`，重新连接，确认读出的数值一致。

网页模拟使用浏览器存储；真实开发板使用 NVS 的 `lesson7` 命名空间，两者独立。USB 下载本课不擦除 NVS；修改网页参数必须重新编译下载后再保存。

完整实现见 `07_nvs/main/main.c`。`nvs_set_blob()` 写值，`nvs_commit()` 提交，`nvs_get_blob()` 读回。

### 存储边界

只读写 lesson7/kp，不清除其他课程或 Wi-Fi 的命名空间。

### 故障定位

保存前确认当前运行的是07_nvs；仅重新编译不会改变板上的程序或保存值。

### 验收

使用两个不同 Kp 分别保存和重启读回，观察第二次确实覆盖本课旧值。

---

## 第 8 章  FreeRTOS 多任务架构

### 本课要完成什么

让 A、B 两个任务按各自周期打印计数，理解并发任务和周期。无需外设，不再混入机器人驱动与传感器初始化。

### 网页与实板

默认 A 每 500 ms 输出一次、B 每 1000 ms 输出一次。分别调整周期、运行网页时间轴，再编译下载到板卡。真实设备日志会出现任务名和递增计数。

任务使用 `vTaskDelayUntil()` 相对上次唤醒时刻安排下一周期。网页展示周期行为，不模拟 ESP32 两核调度、抢占或实际执行耗时。

### 观察与练习

将两个周期设为相同，再改为 300 ms 与 700 ms，观察输出交错。多任务不保证同时打印时的固定先后顺序。

完整实现见 `08_freertos/main/main.c`。

### 代码入口

两个任务的入口分别负责计数和打印；主函数只创建任务，参数通过 OSR_TASK_A_MS 和 OSR_TASK_B_MS 注入。

### 验收

观察一段较长时间内的计数比例，不用相邻两行日志的排列判断任务是否丢失。

---

## 第 9 章  PID 电机速度闭环控制

> 📂 **示例代码**：[09_pid_motor](https://github.com/osrbot/Development-Board-OSRCORE-Example/tree/main/09_pid_motor)

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
} pid_ctrl_t;

/* 返回 int（pulse_us），内部已加 neutral 并钳位到 [1000, 2000] */
int pid_update(pid_ctrl_t *p, float target, float current,
               int neutral, float dt_sec)
{
    float err = target - current;
    if (fabsf(err) < p->deadband) err = 0.0f;

    p->integral += err * dt_sec;
    if (p->integral >  p->max_integral) p->integral =  p->max_integral;
    if (p->integral < -p->max_integral) p->integral = -p->max_integral;

    float derivative = (err - p->last_error) / dt_sec;
    p->last_error = err;

    float output = p->kp * err + p->ki * p->integral + p->kd * derivative;
    int pulse = neutral + (int)output;
    if (pulse < 1000) pulse = 1000;
    if (pulse > 2000) pulse = 2000;
    return pulse;  // 直接可用的 ESC 脉宽（µs）
}

void pid_reset(pid_ctrl_t *p)
{
    p->integral   = 0.0f;
    p->last_error = 0.0f;
}
```

`pid_update` 返回值是可以直接传给 `esc_set()` 的脉宽（µs），neutral 加法和钳位都在函数内完成，调用方不需要额外处理。目标速度接近零时调用 `pid_reset` 清除积分，防止重新启动时出现积分饱和冲击。

**控制任务（20 ms 周期）：**

```c
static void task_control(void *arg)
{
    pid_ctrl_t pid;
    pid_init(&pid, 447.0f, 4.7f, 47.0f, 1000.0f, 0.05f);
    int32_t last_count = encoder_count();
    float filtered_speed = 0.0f;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(20));

        int32_t delta = encoder_count() - last_count;
        last_count = encoder_count();

        float raw_speed = delta * DIST_PER_PULSE / 0.020f;
        filtered_speed  = 0.15f * raw_speed + 0.85f * filtered_speed;

        float target = s_target_speed;

        /* 目标为零时重置积分，防止重启冲击 */
        if (fabsf(target) < 0.05f)
            pid_reset(&pid);

        /* pid_update 直接返回 pulse_us，内含 neutral+clamp */
        int pulse = pid_update(&pid, target, filtered_speed,
                               THROTTLE_NEUTRAL, 0.020f);
        esc_set((uint32_t)pulse);
    }
}
```

### 9.5 课程总结

本章实现了完整的速度 PID 闭环控制，掌握了积分限幅、死区和低通滤波等工程实践技巧。PID 参数调整方法：先增大 Kp 直到出现轻微振荡，再加 Ki 消除稳态误差，最后加 Kd 抑制超调。

---

## 第 10 章  互补滤波姿态解算

### 本课要完成什么

用互补滤波融合陀螺仪与重力方向，输出 Roll 和 Pitch。基础课程不启动加热器，也不等待温度达到阈值。

### 原理

陀螺仪角速度乘以实际采样间隔得到角度增量；加速度计提供重力方向的角度参考。每次更新按 `alpha × 陀螺积分角度 + (1-alpha) × 重力角度` 融合。默认 alpha=0.95。

采样间隔由 `esp_timer_get_time()` 测量，包含 I²C 读取和采样平均所需时间，不能简单假设每次都是 5 ms。强线性加速度会干扰重力估计；仅靠重力不能得到绝对航向。

### 操作顺序

启动时保持板卡静止，完成陀螺仪偏置采样，然后缓慢倾斜。比较不同 alpha 下的响应。网页角度是模拟输入；真板直接读取板载 QMI8658（GPIO10/11）。

完整实现见 `10_ahrs/main/main.c`。历史 Madgwick 和加热器文件保留为进阶参考，但不参与本课构建。

### 参数边界

alpha 越接近1，单次更新越依赖陀螺积分；本课权重按采样更新使用，改变采样频率时需重新评估。

### 验收

平放、缓慢左右倾斜、缓慢前后倾斜各测试一次；快速平移时的角度误差是模型限制。

---

## 第 12 章  完整机器人示例

> 📂 **示例代码**：[12_full_example](https://github.com/osrbot/Development-Board-OSRCORE-Example/tree/main/12_full_example)

### 12.1 知识要点

- 多外设协同初始化的顺序与依赖关系
- 三任务双核架构的完整实现
- ADC 电压监测与查找表线性插值
- serial_tx 队列与专用 TX 任务（解耦发送与控制逻辑）
- 里程计积分（速度 × dt × 航向角）
- SBUS 遥控与串口命令的优先级仲裁
- 速度模式通道（ch[7]）：限速 15% vs 全速
- RC 超时（200 ms）与串口控制超时（500 ms）安全机制
- 电池使能 GPIO 初始化（GPIO16 置 LOW）
- 默认直接启动采样与控制，不等待加热器升温

### 12.2 课程内容

本章将前 10 章的所有外设整合到一个完整的机器人控制程序中，实现与 OSRCORE 固件相同的三任务架构。系统支持两种控制模式：SBUS 遥控模式（CH7 < 1500）和串口命令模式（CH7 ≥ 1500），遥控模式优先级更高。速度模式通道（CH8，index 7）控制限速：< 1500 时限速 15%，≥ 1500 时全速。RC 超时 200 ms、串口控制超时 500 ms，两者均触发自动停车。

### 12.3 基础学习

#### 三任务架构

```
Core 1  task_imu     P5  1ms   QMI8658 → Madgwick AHRS → 里程计积分 → 更新 g_state
Core 1  task_control P4  20ms  编码器 → LPF → PID → ESC/舵机输出 → ADC 电压监测
Core 0  task_comm    P3  1ms   SBUS 解码 / USB CDC 命令 / serial_tx 队列发送
```

另有一个低优先级 serial_tx 任务（P1，Core 0）专门负责从队列取出字符串并写入 USB CDC，将发送操作与控制逻辑完全解耦。

#### 控制模式仲裁

```
SBUS CH7（index 6）< 1500  → 遥控模式：SBUS CH3 → 速度，CH1 → 转向
SBUS CH7（index 6）≥ 1500  → 串口模式：USB CDC 命令控制
SBUS CH8（index 7）< 1500  → 速度限制 15%（低速安全模式）
SBUS CH8（index 7）≥ 1500  → 全速模式
Failsafe / lost_frame 激活  → 强制停止
```

#### RC 超时与串口超时安全

- RC 超时（200 ms）：遥控模式下，若 200 ms 内未收到有效 SBUS 帧，自动触发 failsafe，速度和转向复位到中立位。
- 串口超时（500 ms）：串口控制模式下，若 500 ms 内未收到新命令，自动将速度和转向复位到中立位。

#### ADC 电压监测

电池电压通过 GPIO4（ADC1 CH3）分压检测，使用 9 点查找表做线性插值，将 ADC 原始值映射为实际电压（V）。低于 11.3 V 时通过 serial_tx 队列发出警告。

```c
static const int   ADC_PTS[]  = {1017,1135,1197,1257,1379,1450,1495,1555,1615};
static const float VOLT_PTS[] = {9.0f,10.0f,10.5f,11.0f,12.0f,12.6f,13.0f,13.5f,14.0f};
```

#### 里程计积分

task_imu 在每次 AHRS 更新后，用当前航向角（从四元数提取 yaw）和 task_control 写入的 `filtered_speed` 积分里程计位置：

```c
g_state.odom_pos[0] += spd * dt * cosf(g_state.odom_yaw);
g_state.odom_pos[1] += spd * dt * sinf(g_state.odom_yaw);
```

#### 初始化顺序

```
1. USB CDC 控制台
2. serial_tx 队列 + TX 任务
3. 电池使能（GPIO16 置 LOW，osrcore 硬件约定）
4. NVS Flash 初始化
5. I2C + QMI8658（含启动偏置校准）
6. PCNT 编码器
7. LEDC PWM（ESC + 舵机 + 蜂鸣器）
8. UART SBUS
9. ADC 电压检测
10. Madgwick + PID 初始化
11. ESC 解锁（中立位 2 秒）
12. 创建三个任务（+ serial_tx 任务已在步骤 2 创建）
```

### 12.4 程序学习

**全局状态结构体（portMUX 保护）：**

```c
typedef struct {
    float  target_speed;          // m/s
    float  filtered_speed;        // m/s
    float  kp, ki, kd;
    float  odom_pos[2];           // m, x/y
    float  odom_yaw;              // rad
    float  quat[4];               // w x y z
    float  accel[3];              // m/s²
    float  gyro[3];               // rad/s
    float  voltage;               // V
    float  temp;                  // °C
    int    steering_pulse;        // µs
    int    rc_ch[10];
    bool   remote_active;
    bool   failsafe;
    bool   speed_full_mode;       // CH8 ≥ 1500 → 全速
    bool   serial_control_active;
    unsigned long last_serial_cmd_ms;
} app_state_t;

static app_state_t g_state;
static portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
```

**serial_tx 队列（专用 TX 任务）：**

```c
#define SERIAL_TX_LINE_MAX  192
#define SERIAL_TX_QUEUE_LEN 32

typedef struct { char line[SERIAL_TX_LINE_MAX]; } serial_tx_msg_t;
static QueueHandle_t s_serial_tx_queue;

static void serial_tx_task_fn(void *arg)
{
    serial_tx_msg_t msg;
    while (1) {
        if (xQueueReceive(s_serial_tx_queue, &msg, portMAX_DELAY) == pdTRUE) {
            fputs(msg.line, stdout);
            fflush(stdout);
        }
    }
}

/* 任何任务调用此函数发送，不阻塞调用方 */
static void serial_tx_printf(const char *fmt, ...)
{
    serial_tx_msg_t msg;
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg.line, SERIAL_TX_LINE_MAX, fmt, args);
    va_end(args);
    xQueueSend(s_serial_tx_queue, &msg, 0);
}
```

**task_comm 中的模式仲裁（核心逻辑）：**

```c
// SBUS 解码后的模式判断
if (rc_ch[CONTROL_MODE_CH] < 1500) {
    /* 遥控模式 */
    bool full = (rc_ch[SPEED_MODE_CH] >= 1500);  // CH8 控制限速
    float spd = /* 从 CH3 映射 */;
    if (!full) spd *= 0.15f;                      // 限速 15%

    portENTER_CRITICAL(&g_mux);
    g_state.remote_active    = true;
    g_state.speed_full_mode  = full;
    g_state.target_speed     = spd;
    g_state.steering_pulse   = /* 从 CH1 映射 */;
    portEXIT_CRITICAL(&g_mux);
}

// RC 超时（200ms）
if (rc_initialized && g_state.remote_active) {
    if (now_ms - last_rc_ok_ms >= RC_TIMEOUT_MS) {  // RC_TIMEOUT_MS = 200
        portENTER_CRITICAL(&g_mux);
        g_state.failsafe       = true;
        g_state.remote_active  = false;
        g_state.target_speed   = 0.0f;
        g_state.steering_pulse = STEERING_CENTER;
        portEXIT_CRITICAL(&g_mux);
        serial_tx_printf("WARN: RC signal lost\n");
    }
}

// 串口超时（500ms）
if (g_state.serial_control_active && !g_state.remote_active) {
    if (g_state.last_serial_cmd_ms > 0 &&
        now_ms - g_state.last_serial_cmd_ms > SERIAL_TIMEOUT_MS) {  // 500ms
        portENTER_CRITICAL(&g_mux);
        g_state.target_speed   = 0.0f;
        g_state.steering_pulse = STEERING_CENTER;
        portEXIT_CRITICAL(&g_mux);
    }
}
```

**app_main 初始化序列：**

```c
void app_main(void)
{
    // USB CDC
    usb_serial_jtag_driver_install(&usb_cfg);

    // serial_tx 队列 + TX 任务
    serial_tx_init();

    // 电池使能：GPIO16 置 LOW（osrcore 硬件约定）
    gpio_set_direction(IO_CTRL_BAT, GPIO_MODE_OUTPUT);
    gpio_set_level(IO_CTRL_BAT, 0);

    ESP_ERROR_CHECK(nvs_flash_init());

    init_i2c_imu();   // 含 qmi8658_calibrate_bias()
    init_encoder();
    init_pwm();
    init_sbus();
    init_adc();       // ADC1 CH3，查找表电压监测

    madgwick_init(&g_ahrs, 0.1f);
    pid_init(&g_pid, PID_KP_DEFAULT, PID_KI_DEFAULT, PID_KD_DEFAULT,
             PID_MAX_INTEGRAL, PID_DEADBAND);

    // ESC 解锁
    set_throttle(THROTTLE_NEUTRAL);
    set_steering(STEERING_CENTER);
    vTaskDelay(pdMS_TO_TICKS(2000));

    xTaskCreatePinnedToCore(task_imu,     "imu",  4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(task_control, "ctrl", 4096, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(task_comm,    "comm", 8192, NULL, 3, NULL, 0);
    // serial_tx 任务（P1）已在 serial_tx_init() 中创建
}
```

#### 默认启动行为

当前教学版本不启动加热器，也不等待 warm/stable/ready 温度阈值。IMU 采样、控制与通信直接启动；历史加热器模块保留为进阶参考。

### 12.5 课程总结

本章完成了 OSRCORE 完整机器人控制程序的实现，将 WS2812B、蜂鸣器、ESC、舵机、编码器、IMU、SBUS、NVS 和 FreeRTOS 多任务架构全部整合在一起。新增特性包括：ADC 查找表电压监测、serial_tx 队列解耦发送、里程计积分、速度模式通道（CH8）、RC 超时 200 ms 安全机制，以及电池使能 GPIO16 置 LOW 的硬件约定。三任务双核架构保证了 IMU 采样和 PID 控制的实时性，SBUS/串口双模式控制提供了灵活的操控方式。

至此，OSRCORE 开发板教程全部完成。建议按以下路径深入学习：
- 调整 PID 参数，观察速度响应曲线
- 在 task_imu 中加入姿态反馈，实现倾斜补偿
- 扩展 SBUS 通道映射，支持更多遥控功能
- 使用 ESP-IDF 的 WiFi/BLE 组件，添加无线调试界面

---

## 第 11 章  QMC6309 磁力计与罗盘航向

> 📂 **示例代码**：[11_mag_compass](https://github.com/osrbot/Development-Board-OSRCORE-Example/tree/main/11_mag_compass)

### 11.1 知识要点

- QMC6309 三轴磁力计工作原理（霍尔效应，I2C 接口，地址 0x7C）
- 硬铁（hard-iron）干扰：电路板上的永磁体造成固定偏移，用 min/max 法校准
- 软铁（soft-iron）干扰：铁磁材料使磁场椭圆化，用缩放矩阵校正为圆
- 倾斜补偿航向（tilt-compensated heading）：利用 roll/pitch 将水平磁场分量投影到水平面
- 互补滤波融合陀螺仪积分与磁力计绝对航向，抑制各自的短板

### 11.2 课程内容

本章演示 QMC6309 磁力计的完整使用流程：初始化、原始数据读取、硬铁/软铁校准、倾斜补偿航向计算，以及通过 USB CDC 控制台交互。掌握本章后，可将磁力计融合进 AHRS，实现绝对航向估计。

### 11.3 基础学习

#### QMC6309 寄存器

| 寄存器 | 地址 | 说明 |
|--------|------|------|
| CHIP_ID | 0x00 | 固定值 0x90，用于验证通信 |
| X_LSB | 0x01 | X 轴数据低字节（共 6 字节 XYZ） |
| STATUS | 0x09 | bit0=DRDY（数据就绪） |
| CTRL1 | 0x0A | OSR、工作模式 |
| CTRL2 | 0x0B | 软复位、ODR、量程 |

初始化流程：软复位 → 验证 CHIP_ID → 配置 CTRL1（OSR）→ 配置 CTRL2（ODR、量程）→ 设置连续测量模式。

#### 硬铁校准

让设备在水平面旋转 360°，记录 X/Y 轴的最大值和最小值：

```c
hard_iron[0] = (max_x + min_x) / 2.0f;
hard_iron[1] = (max_y + min_y) / 2.0f;
```

校准后：`x_cal = x_raw - hard_iron[0]`

#### 软铁校准

XY 轴量程不等时，磁场轨迹为椭圆。用对角缩放矩阵将其拉伸为圆：

```c
float avg_range = (range_x + range_y) / 2.0f;
soft_iron[0] = avg_range / range_x;   // X 缩放
soft_iron[4] = avg_range / range_y;   // Y 缩放
soft_iron[8] = 1.0f;                  // Z 不变（地面车辆）
```

#### 倾斜补偿

直接用 atan2(my, mx) 只在水平放置时准确。倾斜时需先将磁场投影到水平面：

```c
float mx_h = mx * cosf(pitch) + mz * sinf(pitch);
float my_h = mx * sinf(roll) * sinf(pitch)
           + my * cosf(roll)
           - mz * sinf(roll) * cosf(pitch);
float heading = atan2f(-my_h, mx_h) * 180.0f / M_PI;
if (heading < 0) heading += 360.0f;
```

其中 roll/pitch 来自 IMU（第 10 章 Madgwick AHRS）。

#### 互补滤波

磁力计噪声大但无累积误差；陀螺仪短期精准但有漂移。互补滤波取长补短：

```c
// 静止时 alpha=0.98，运动时 alpha=0.995
yaw_cf += gyro_z * dt;
float err = mag_heading - yaw_cf;
// 角度差归一化到 [-π, π]
yaw_cf += (1.0f - alpha) * err;
```

### 11.4 程序学习

#### 初始化

```c
void qmc6309_init(i2c_master_dev_handle_t dev) {
    s_dev = dev;
    memset(s_hard_iron, 0, sizeof(s_hard_iron));
    memcpy(s_soft_iron, k_identity, sizeof(s_soft_iron));

    // 软复位
    write_reg(QMC_REG_CTRL2, CTRL2_SOFT_RST);
    vTaskDelay(pdMS_TO_TICKS(20));
    write_reg(QMC_REG_CTRL2, 0x00);
    vTaskDelay(pdMS_TO_TICKS(20));

    // 验证 CHIP_ID
    uint8_t id = 0;
    read_reg(QMC_REG_CHIP_ID, &id, 100);
    if (id != 0x90) printf("WARN: QMC6309 CHIP_ID=0x%02X\n", id);

    // OSR2=4, OSR1=3, 单次模式（后续切换为连续）
    uint8_t ctrl1 = CTRL1_OSR2(4) | CTRL1_OSR1(3) | CTRL1_MODE(0);
    write_reg(QMC_REG_CTRL1, ctrl1);
    // ODR=4 (200Hz), RNG=2 (8G)
    write_reg(QMC_REG_CTRL2, CTRL2_ODR(4) | CTRL2_RNG(2));
    // 切换为连续测量模式
    ctrl1 = (ctrl1 & ~0x03) | CTRL1_MODE(3);
    write_reg(QMC_REG_CTRL1, ctrl1);
    vTaskDelay(pdMS_TO_TICKS(100));
}
```

#### 读取与校准应用

```c
bool qmc6309_read(qmc6309_data_t *out) {
    uint8_t status = 0;
    read_reg(QMC_REG_STATUS, &status, QMC_READ_I2C_TIMEOUT_MS);
    if (!(status & STATUS_DRDY)) return false;

    uint8_t data[6];
    read_burst(QMC_REG_X_LSB, data, 6);
    float rx = (int16_t)((data[1] << 8) | data[0]) / s_sensitivity;
    float ry = (int16_t)((data[3] << 8) | data[2]) / s_sensitivity;
    float rz = (int16_t)((data[5] << 8) | data[4]) / s_sensitivity;

    // 应用硬铁 + 软铁校准
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

#### 校准流程

```c
void qmc6309_calibrate(uint32_t duration_ms) {
    // 初始化 min/max
    for (int i = 0; i < 3; i++) { s_min_val[i] = 1e6f; s_max_val[i] = -1e6f; }

    int64_t start = esp_timer_get_time();
    while ((esp_timer_get_time() - start) < (int64_t)duration_ms * 1000) {
        // 读取原始高斯值并更新 min/max
        // ...
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    // 计算硬铁偏移和软铁缩放
    for (int i = 0; i < 3; i++)
        s_hard_iron[i] = (s_max_val[i] + s_min_val[i]) / 2.0f;
    float avg_xy = ((s_max_val[0]-s_min_val[0]) + (s_max_val[1]-s_min_val[1])) / 2.0f;
    s_soft_iron[0] = avg_xy / (s_max_val[0] - s_min_val[0]);
    s_soft_iron[4] = avg_xy / (s_max_val[1] - s_min_val[1]);
}
```

#### 主循环

```c
while (1) {
    qmc6309_data_t d;
    if (qmc6309_read(&d)) {
        printf("hdg=%.1f x=%.4f y=%.4f z=%.4f\n",
               d.heading, d.x, d.y, d.z);
    }

    // 非阻塞 USB CDC 命令解析
    uint8_t byte;
    if (usb_serial_jtag_read_bytes(&byte, 1, 0) == 1) {
        // 积累到换行符后处理命令
        // cal [sec] / heading / help
    }
    vTaskDelay(pdMS_TO_TICKS(100));
}
```

### 11.5 课程总结

本章掌握了 QMC6309 磁力计的 I2C 驱动编写、硬铁/软铁两步校准流程，以及倾斜补偿航向计算。磁力计提供绝对航向参考，但对电磁干扰敏感；第 12 章完整示例中，通过互补滤波将磁力计与陀螺仪融合，在动态场景下获得稳定的罗盘航向。

## 第 13 章 出厂功能检查

### 目标

逐项确认板载功能能否响应；网页故障开关是教学模拟，不能替代实板检测。

### 准备

准备开发板与 USB 数据线。外部接收机、电机、舵机等接口，只有接上对应外设才能测试。

### 操作

在第 13 课打开设备面板，编译并下载诊断例程。重新连接串口，查看启动检测和日志。先检查灯、蜂鸣器与传感器，再测试外部接口。

### 源码与输出

完整实现位于 `13_factory_demo/main/main.c`。以真实日志中的响应为准；网页模拟输出独立显示。电机测试必须使用对应命令，失联或丢帧会停止遥控输出。

### 验收

逐项记录通过、失败或未连接；未连接外设不等于开发板损坏。先检查供电、接线和运行的固件版本，再判断硬件故障。
