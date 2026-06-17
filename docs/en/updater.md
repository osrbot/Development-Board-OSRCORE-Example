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

<FirmwareUpdater locale="en" />
