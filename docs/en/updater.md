---
title: Firmware Flashing
description: Flash OSRBOT firmware in the browser with no-BOOT app update or BOOT factory restore.
---

# Firmware Flashing

> Chrome or Edge is required because this page uses the Web Serial API.

This page provides two flashing paths:

- **No-BOOT app update**: the device boots normally and only the app partition is updated. BOOT mode is not required, but the currently running firmware must support the `fw` update protocol. Remote firmware can be selected from the current repository examples; custom URLs must be full HTTPS addresses.
- **Factory restore**: first flash, factory restore, or broken firmware recovery. BOOT download mode is required and the built-in full flash image is written.

Local `.bin` files are still supported for internal testing or temporary firmware validation.

Recommended test after flashing an example firmware:

1. Open the serial monitor, send `diag`, and check SN, flash, heap, I2C, IMU, magnetometer, and SBUS status.
2. Send `imu` and `mag`, then gently rotate the board and confirm raw data changes.
3. Send `ledtest` and `beeptest` to verify the LED and buzzer.
4. Send `rc`, move the transmitter sticks, and confirm channel values change.
5. Lift the vehicle, then send `servo left`, `servo center`, and `servo right`.
6. Lift the vehicle, then send `motor fwd`, `motor rev`, and `motor stop`.
7. Lift the vehicle, send `drive on`, use CH0/CH2 for limited low-speed RC testing, then send `drive off`.

<FirmwareUpdater locale="en" />
