# OSRCORE Learn

[English](README.md) · [简体中文](README_zh.md)

Interactive learning for the OSRCORE ESP32-S3 board: a selectable 3D board, pin references, lesson simulations, and matching ESP-IDF examples.

## Status

- 14 lessons (00–13), including 13 buildable firmware examples.
- Browser simulations model lesson behavior; they do not execute arbitrary ESP32 firmware.
- Parameter compilation, USB flashing and Wi-Fi OTA use the local companion service, which is not included in static hosting.
- Local builds and automated checks completed; physical-board USB, Wi-Fi and peripheral validation is pending.

## Run locally

```sh
npm --prefix docs ci
npm --prefix docs run build
node tools/lab-server.mjs
```

Open `http://localhost:5173/`. Firmware compilation requires ESP-IDF v6.0.1; USB access requires a Web Serial capable browser. See [local service instructions](LOCAL_LAB.md).

## Lessons and source

| # | Lesson | Source |
|---|---|---|
| 00 | Meet OSRCORE | [Hardware reference](materials/) |
| 01 | Light up an LED | [01_blink_led](01_blink_led/) |
| 02 | Make some sound | [02_buzzer](02_buzzer/) |
| 03 | Servo & ESC | [03_pwm_servo](03_pwm_servo/) |
| 04 | Read RC signals | [04_sbus_rc](04_sbus_rc/) |
| 05 | Sense motion | [05_imu_i2c](05_imu_i2c/) |
| 06 | Measure speed | [06_encoder](06_encoder/) |
| 07 | Save settings | [07_nvs](07_nvs/) |
| 08 | Run multiple tasks | [08_freertos](08_freertos/) |
| 09 | Control motor speed | [09_pid_motor](09_pid_motor/) |
| 10 | Estimate attitude | [10_ahrs](10_ahrs/) |
| 11 | Find your heading | [11_mag_compass](11_mag_compass/) |
| 12 | Build a robot | [12_full_example](12_full_example/) |
| 13 | Factory check | [13_factory_demo](13_factory_demo/) |

## Validation

```sh
node --test tools/*.test.mjs docs/scripts/*.test.mjs
npm --prefix docs run build
```

See [EXAMPLE_REVIEW.md](EXAMPLE_REVIEW.md) for validation scope. Tutorial sources live in `docs/tutorial_zh.md` and `docs/en/tutorial.md`; navigation is generated from the same lesson definitions as the interactive app.

## Release and history

The previous version is retained as `legacy-before-interactive`. The rewrite is reviewed on `refactor/interactive-learning` before merging into the default branch.

## License

MIT
