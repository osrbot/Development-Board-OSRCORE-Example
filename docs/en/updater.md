---
title: Firmware Updater
description: Update OSRBOT firmware in the browser with app OTA or BOOT recovery flashing.
---

# Firmware Updater

> Chrome or Edge is required because this page uses the Web Serial API.

This page provides two update paths:

- **App update**: the device boots normally and only the app partition is updated. BOOT mode is not required.
- **Recovery flash**: first flash, factory restore, or broken firmware recovery. BOOT download mode is required and a full flash image is written.

Remote firmware URL accepts two formats:

- Site-relative path, for example `/firmware/osrbot_ESP32S3_IDF_App.bin`
- Full HTTPS URL, for example `https://example.com/firmware.bin`

Recommended factory test after flashing the diagnostic firmware:

1. Open the serial monitor, send `diag`, and check SN, flash, heap, I2C, IMU, magnetometer, and SBUS status.
2. Send `imu` and `mag`, then gently rotate the board and confirm raw data changes.
3. Send `ledtest` and `beeptest` to verify the LED and buzzer.
4. Send `rc`, move the transmitter sticks, and confirm channel values change.
5. Lift the vehicle, then send `servo left`, `servo center`, and `servo right`.
6. Lift the vehicle, then send `motor fwd`, `motor rev`, and `motor stop`.
7. Lift the vehicle, send `drive on`, use CH0/CH2 for limited low-speed RC testing, then send `drive off`.

<FirmwareUpdater locale="en" />
