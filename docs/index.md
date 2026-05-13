---
layout: home

hero:
  name: OSRCORE
  text: ESP-IDF 机器人控制板教程
  tagline: 面向 OSRCORE 开发板的 ESP-IDF 上手指南，覆盖外设驱动、传感器读取、运动控制与完整机器人示例。
  image:
    src: /logo.svg
    alt: OSRBOT logo
  actions:
    - theme: brand
      text: 开始阅读
      link: /chapters
    - theme: alt
      text: 查看示例代码
      link: https://github.com/osrbot/Development-Board-OSRCORE-Example

features:
  - icon: 🧭
    title: 章节化导航
    details: 0–11 章覆盖 ESP-IDF 环境、LED、蜂鸣器、SBUS、IMU、PID、AHRS 与完整示例。
  - icon: 🌐
    title: 中英文文档
    details: 提供中文与英文版本，并在构建时检查章节索引、标题与文件映射。
  - icon: ⚙️
    title: ESP-IDF 优先
    details: 代码和说明贴近 ESP-IDF v5.x API，方便边看边编译、烧录、调试。
---

<div class="osrcore-chip-row">
  <span class="osrcore-chip">ESP32-S3</span>
  <span class="osrcore-chip">USB CDC</span>
  <span class="osrcore-chip">RMT</span>
  <span class="osrcore-chip">LEDC</span>
  <span class="osrcore-chip">UART / SBUS</span>
  <span class="osrcore-chip">I2C / IMU</span>
  <span class="osrcore-chip">PCNT / Encoder</span>
  <span class="osrcore-chip">FreeRTOS</span>
</div>

<div class="osrcore-panel">
  <strong>推荐阅读路径：</strong>
  先从第 0 章完成 ESP-IDF 环境与开发板资源确认，再按外设逐章运行示例。最后阅读第 11 章，把遥控、姿态、编码器和闭环控制拼成完整机器人程序。
</div>

## 文档入口

- [中文章节索引](/chapters)
- [English Chapter Index](/en/chapters)
- [GitHub 示例仓库](https://github.com/osrbot/Development-Board-OSRCORE-Example)
