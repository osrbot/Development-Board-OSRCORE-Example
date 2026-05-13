---
layout: home

hero:
  name: OSRCORE
  text: ESP-IDF Robot Control Board Guide
  tagline: An ESP-IDF guide for the OSRCORE development board, covering peripheral drivers, sensor readout, motion control, and a complete robot example.
  image:
    src: /logo.svg
    alt: OSRBOT logo
  actions:
    - theme: brand
      text: Start Reading
      link: /en/chapters
    - theme: alt
      text: View Examples
      link: https://github.com/osrbot/Development-Board-OSRCORE-Example

features:
  - icon: 🧭
    title: Chapter-Based Navigation
    details: Chapters 0–11 cover ESP-IDF setup, LED, buzzer, SBUS, IMU, encoder, NVS, FreeRTOS, PID, AHRS, and the complete example.
  - icon: 🌐
    title: Chinese and English Docs
    details: Both language versions are generated from paired chapters and checked during the documentation build.
  - icon: ⚙️
    title: ESP-IDF First
    details: Code and explanations are aligned with ESP-IDF v5.x APIs so they can be built, flashed, and debugged directly.
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
  <strong>Recommended path:</strong>
  Start with Chapter 0 to set up ESP-IDF and understand the OSRCORE hardware resources, then run the examples chapter by chapter. Chapter 11 combines remote control, attitude sensing, encoder feedback, and closed-loop control into a complete robot program.
</div>

## Documentation

- [English Chapter Index](/en/chapters)
- [Chapter 0: ESP-IDF Toolchain and OSRCORE Hardware](/en/chapter-00)
- [中文章节索引](/chapters)
- [GitHub Repository](https://github.com/osrbot/Development-Board-OSRCORE-Example)
