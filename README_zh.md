# OSRCORE Learn

[English](README.md) · [简体中文](README_zh.md)

**基于 ESP32-S3 的 OSRCORE 机器人控制器交互教程与例程。**

从认识接口、读取传感器到控制电机，把单个功能组合成机器人控制程序。

[**打开在线教程 →**](https://osrbot.github.io/osrcore-learn/zh/)

## 在线学习

- **认识控制器**：旋转、缩放三维模型，选择端子、排针和板载器件，查看引脚与信号定义。
- **运行课程模拟**：调整课程参数，观察代码、指示灯和输出的变化。
- **编写机器人程序**：结合配套 ESP-IDF 例程，学习传感、通信、运动控制与多任务协作。

## 课程与源码

14 节课程，包含 13 个 ESP-IDF 固件例程。点击名称查看对应源码或硬件资料。

| 课程 | 内容 |
|---|---|
| 00 | [认识开发板](materials/) |
| 01 | [点亮 RGB 灯](01_blink_led/) |
| 02 | [让蜂鸣器发声](02_buzzer/) |
| 03 | [控制舵机与电调](03_pwm_servo/) |
| 04 | [接收遥控信号](04_sbus_rc/) |
| 05 | [感知运动](05_imu_i2c/) |
| 06 | [测量转速](06_encoder/) |
| 07 | [保存参数](07_nvs/) |
| 08 | [运行多个任务](08_freertos/) |
| 09 | [闭环控制电机](09_pid_motor/) |
| 10 | [解算姿态](10_ahrs/) |
| 11 | [找到方向](11_mag_compass/) |
| 12 | [组合成机器人](12_full_example/) |
| 13 | [控制器功能检查](13_factory_demo/) |

## 硬件资料

[产品手册](https://osrbot.github.io/osrcore-learn/hardware/manual-zh.pdf) · [原理图](https://osrbot.github.io/osrcore-learn/hardware/schematic-zh.pdf) · [PCB 与接口资料](materials/)
