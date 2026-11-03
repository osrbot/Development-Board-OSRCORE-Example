# OSRCORE Learn

[English](README.md) · [简体中文](README_zh.md) · [在线教程 / Website](https://osrbot.github.io/osrcore-learn/)

OSRCORE（ESP32-S3）交互式入门教程：三维开发板、接口引脚查询、课程行为模拟，以及配套 ESP-IDF 例程。

## 当前状态

- 14 课（00–13），其中 13 个可编译固件例程。
- 网页模拟按课程行为运行，不编译或执行任意 ESP32 程序。
- USB 下载、参数编译和 Wi-Fi OTA 通过本地配套服务使用；静态预览站不包含编译服务。
- 已完成本地编译和自动化检查；实板 USB、Wi-Fi 与外设验收仍待完成。

## 本地启动

```sh
npm --prefix docs ci
npm --prefix docs run build
node tools/lab-server.mjs
```

打开 `http://localhost:5173/`。编译固件需要 ESP-IDF v6.0.1；USB 需要支持 Web Serial 的浏览器。详见 [本地使用说明](LOCAL_LAB.md)。

## 课程与源码

| # | 课程 | 源码 |
|---|---|---|
| 00 | 认识开发板 | [硬件资料](materials/) |
| 01 | 点亮 RGB 灯 | [01_blink_led](01_blink_led/) |
| 02 | 让蜂鸣器发声 | [02_buzzer](02_buzzer/) |
| 03 | 控制舵机与电调 | [03_pwm_servo](03_pwm_servo/) |
| 04 | 接收遥控信号 | [04_sbus_rc](04_sbus_rc/) |
| 05 | 感知运动 | [05_imu_i2c](05_imu_i2c/) |
| 06 | 测量转速 | [06_encoder](06_encoder/) |
| 07 | 保存参数 | [07_nvs](07_nvs/) |
| 08 | 运行多个任务 | [08_freertos](08_freertos/) |
| 09 | 闭环控制电机 | [09_pid_motor](09_pid_motor/) |
| 10 | 解算姿态 | [10_ahrs](10_ahrs/) |
| 11 | 找到方向 | [11_mag_compass](11_mag_compass/) |
| 12 | 组合成机器人 | [12_full_example](12_full_example/) |
| 13 | 出厂功能检查 | [13_factory_demo](13_factory_demo/) |

## 验证

```sh
node --test tools/*.test.mjs docs/scripts/*.test.mjs
npm --prefix docs run build
```

例程复核记录：[EXAMPLE_REVIEW.md](EXAMPLE_REVIEW.md)。教材源文件位于 `docs/tutorial_zh.md` 和 `docs/en/tutorial.md`，目录与网页课程共用课程定义。

## 发布与历史

旧版通过 `legacy-before-interactive` 标签保留。重构先在 `refactor/interactive-learning` 分支验收，通过后再合并主分支。

## 许可证

MIT
