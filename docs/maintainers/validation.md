# Lesson validation

Browser simulations teach behavior; they are not a general ESP32 instruction emulator. Firmware parameters use a shared allowlist to generate C macros for real ESP-IDF builds. Simulated sensor inputs do not represent physical measurements.

| Lesson | Implementation and hardware acceptance focus |
|---|---|
| RGB | GPIO45; compiled brightness and interval; verify red/green/blue/off and zero brightness. |
| Buzzer | GPIO42; frequency, duration and 100 ms silence; verify pitch and timing. |
| PWM | GPIO2 pulse width; GPIO1 neutral; first verify 1500 µs with separate servo power and common ground. |
| SBUS | GPIO44; lost frames and failsafe invalidate input; verify receiver frames and disconnect behavior. |
| IMU | GPIO10/11; reject incorrect WHO_AM_I; verify gravity, rotation and temperature. |
| Encoder | GPIO3/9; compiled counts per revolution; verify single-edge counts per output-shaft revolution. |
| NVS | One Kp value in the lesson7 namespace; verify save, reboot and loaded value. |
| FreeRTOS | Two periodic logging tasks; verify timing and counters. |
| PID | Reject nonnumeric commands; stop after 1 s without commands; verify encoder direction, polarity and neutral. |
| Attitude | Complementary filter with measured dt; verify roll/pitch direction; no absolute yaw claim. |
| Compass | Calibration window 5–120 s; verify horizontal rotation and magnetic environment. |
| Robot | RC timeout 200 ms, serial timeout 500 ms; reject nonfinite/out-of-range parameters; synchronize text logs to Wi-Fi. |
| Factory | Invalid/lost RC frames disable control; return to neutral after 200 ms; validate each physical interface. |

## Recorded checks

- 18 automated checks cover parameters, request boundaries, simulation behavior and hardware selection mappings.
- All 13 default firmware examples compiled. Image hashes, embedded build identifiers, OTA capacity and NVS write ranges were checked.
- Physical-board flashing, Wi-Fi provisioning/OTA and peripheral responses remain unverified.
- Wi-Fi captures ESP logs and the robot example's text queue. Some legacy printf diagnostics remain USB-only. Logs use bounded buffers and are not lossless recording.

Real motor, encoder and servo parameters require hardware tuning; simulated trajectories are not a guarantee of physical behavior.
