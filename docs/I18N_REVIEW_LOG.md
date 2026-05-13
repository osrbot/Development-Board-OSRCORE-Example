# OSRCORE Docs i18n Review Log

This log records paragraph-level bilingual review progress for the split documentation.

## Review Criteria

A chapter can be marked as reviewed when:

1. The Chinese and English chapters have the same main section structure.
2. Every Chinese paragraph, table, code block, formula block, and summary has a corresponding English item.
3. Code blocks preserve API names, constants, GPIO values, and command lines.
4. Technical wording is consistent with ESP-IDF and embedded systems terminology.
5. The chapter passes `npm run i18n:check` after regeneration.

## Progress

| Chapter | Status | Notes |
|---|---|---|
| 00 | Reviewed | English Chapter 0 mirrors the Chinese 0.1–0.5 structure: key points, course content, ESP-IDF installation, project structure, idf.py commands, OSRCORE resource table, USB CDC configuration, minimal app, and summary. |
| 01 | Reviewed | English Chapter 1 mirrors the Chinese 1.1–1.5 structure: WS2812B timing requirements, ESP-IDF RMT TX v5.x API, custom encoder architecture, GRB byte order, timing table, RMT tick conversion, TX channel setup, color transmission function, and chapter summary. |
| 02 | Reviewed | English Chapter 2 mirrors the Chinese 2.1–2.5 structure: LEDC timer/channel configuration, dynamic PWM frequency changes, 50% duty-cycle buzzer drive, note frequency table, timer/channel initialization, single-tone playback, and summary. |
| 03 | Reviewed | English Chapter 3 mirrors the Chinese 3.1–3.5 structure: standard RC PWM timing, 14-bit LEDC duty calculation, 1000–2000 µs pulse mapping, ESC arming flow, dual-channel throttle/steering configuration, set-pulse helper, and summary. |
| 04 | Reviewed | English Chapter 4 mirrors the Chinese 4.1–4.5 structure: SBUS frame format, 100000 baud 8E2 inverted serial configuration, 16-channel 11-bit unpacking, failsafe/lost-frame flags, UART initialization, frame synchronization, SBUS-to-PWM conversion, and summary. |
| 05 | Reviewed | English Chapter 5 mirrors the Chinese 5.1–5.5 structure: ESP-IDF I2C master bus-device API, QMI8658 register map, accelerometer/gyroscope ranges, physical-unit conversion, I2C bus/device initialization, register read helper, IMU data printout, and summary. |
| 06 | Reviewed | English Chapter 6 mirrors the Chinese 6.1–6.5 structure: quadrature encoder A/B phase principle, PCNT new API setup, speed conversion formula, overflow handling, PCNT unit/channel setup, edge/level action configuration, speed calculation, and summary. |
| 07 | Reviewed | English Chapter 7 mirrors the Chinese 7.1–7.5 structure: NVS namespace/key/value concepts, PID parameter persistence, default values, initialization, namespace open/read flow, blob write/commit flow, and summary. |
| 08 | Reviewed | English Chapter 8 mirrors the Chinese 8.1–8.5 structure: FreeRTOS task creation, ESP32-S3 core pinning, queue communication, mutex/shared-state concepts, task responsibility table, queue send/receive examples, and summary. |
| 09 | Reviewed | English Chapter 9 mirrors the Chinese 9.1–9.5 structure: closed-loop speed-control structure, encoder feedback, PID formula, parameter effects, PID update function, ESC PWM output mapping, failsafe neutral-output behavior, and summary. |
| 10 | Reviewed | English Chapter 10 mirrors the Chinese 10.1–10.5 structure: quaternion attitude representation, accelerometer/gyroscope fusion, Madgwick 6-DOF update flow, quaternion normalization, Euler conversion formulas, IMU update loop, and summary. |
| 11 | Reviewed | English Chapter 11 mirrors the Chinese 11.1–11.5 structure: full robot integration, multi-task architecture, SBUS/serial command input, encoder feedback, PID control, ESC/servo output, IMU telemetry, safety rules, initialization flow, control loop, and summary. |

## Result

All chapters 00–11 have been reviewed at the structure, paragraph, table, formula, code-block, and terminology level. Future documentation updates should keep `tutorial_zh.md`, `en/tutorial.md`, and `i18n-map.json` synchronized.
