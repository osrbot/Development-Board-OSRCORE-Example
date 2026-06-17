---
title: 固件烧录
description: 使用浏览器完成 OSRBOT 设备的在线烧录或恢复出厂。
---

# 固件烧录

> 需要 Chrome 或 Edge 浏览器（支持 Web Serial API）。

这个页面提供两种烧录方式：

- **在线烧录**：设备正常开机时优先通过 `fw` 协议只更新 app 固件；如果设备已经在 ESP-ROM 下载模式，会自动刷写当前选择例程的 full flash 固件。远端固件可以直接从当前仓库例程中选择；自定义 URL 必须填写完整 HTTPS 地址。
- **恢复出厂**：第一次刷机、恢复出厂或固件无法启动时使用，需要进入 BOOT 下载模式并写入站内 full flash 固件。

所有当前仓库例程都会内置 `fw` 更新协议。设备完成一次恢复出厂后，后续在这些例程之间切换可以直接使用在线烧录。

本地 `.bin` 文件仍可用于内部测试或临时固件验证。

例程固件刷入后，建议测试流程：

1. 打开串口监视器，发送 `diag`，确认芯片 SN、Flash、Heap、I2C、IMU、地磁、SBUS 状态。
2. 发送 `imu` 和 `mag`，轻微转动开发板，确认原始数据会变化。
3. 发送 `ledtest` 和 `beeptest`，确认 LED 与蜂鸣器工作。
4. 发送 `rc`，推动遥控器摇杆，确认通道数值变化。
5. 架空车辆后发送 `servo left`、`servo center`、`servo right`，确认舵机方向。
6. 架空车辆后发送 `motor fwd`、`motor rev`、`motor stop`，确认电机正反转。
7. 架空车辆后发送 `drive on`，用遥控器 CH0/CH2 做受限低速测试；结束发送 `drive off`。

<FirmwareUpdater />
