---
layout: home

hero:
  name: OSRCORE
  text: ESP-IDF 机器人控制板教程
  tagline: 从点亮 WS2812B 到完整机器人控制系统，把开发板文档整理成按章节维护、可双语对齐的网站。
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
    details: 0–11 章拆分维护，侧边栏可直接跳转到 LED、蜂鸣器、SBUS、IMU、PID、AHRS 与完整示例。
  - icon: 🌐
    title: 中英文对齐
    details: 使用 i18n 映射表维护章节配对，构建前自动检查中文和英文版本的索引、标题与章节文件。
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
- [兼容单页中文教程](/tutorial_zh)
- [GitHub 示例仓库](https://github.com/osrbot/Development-Board-OSRCORE-Example)
