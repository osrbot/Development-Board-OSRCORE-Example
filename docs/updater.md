---
title: 固件升级工具
description: 使用浏览器完成 OSRBOT 设备的普通 app OTA 升级或 BOOT 恢复刷机。
---

# 固件升级工具

> 需要 Chrome 或 Edge 浏览器（支持 Web Serial API）。

这个页面提供两种升级方式：

- **普通升级**：设备正常开机，不需要进入 BOOT，只更新 app 固件。
- **恢复刷机**：第一次刷机、恢复出厂或固件无法启动时使用，需要进入 BOOT 下载模式并写入 full flash 固件。

远端固件 URL 支持两种格式：

- 站内相对路径，例如 `/firmware/osrbot_ESP32S3_IDF_App.bin`
- 完整 HTTPS 地址，例如 `https://example.com/firmware.bin`

出厂诊断固件刷入后，建议测试流程：

1. 打开串口监视器，发送 `diag`，确认芯片 SN、Flash、Heap、I2C、IMU、地磁、SBUS 状态。
2. 发送 `imu` 和 `mag`，轻微转动开发板，确认原始数据会变化。
3. 发送 `ledtest` 和 `beeptest`，确认 LED 与蜂鸣器工作。
4. 发送 `rc`，推动遥控器摇杆，确认通道数值变化。
5. 架空车辆后发送 `servo left`、`servo center`、`servo right`，确认舵机方向。
6. 架空车辆后发送 `motor fwd`、`motor rev`、`motor stop`，确认电机正反转。
7. 架空车辆后发送 `drive on`，用遥控器 CH0/CH2 做受限低速测试；结束发送 `drive off`。

<FirmwareUpdater />
