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

<FirmwareUpdater />
