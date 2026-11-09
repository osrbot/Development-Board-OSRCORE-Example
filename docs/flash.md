# Download to the controller

[Open the interactive lessons](/#lesson-01) in Chrome or Edge on a computer with USB access.

1. Select a lesson and open **Connect controller**.
2. Plug in a USB data cable, click **Connect and download lesson**, and select the controller's USB serial port.
3. The website fetches and verifies the published lesson firmware, then writes and verifies it over USB. An existing connection is reused when downloading another lesson.
4. The controller restarts and the page attempts to reconnect its console. Follow **Try it now** for this lesson’s physical actions and expected results.

The device card identifies the running lesson only after the controller replies. A completed download or an open USB port alone does not confirm program startup. The console provides **Read device info**, **Restart controller** and command input; identity and restart require the updated course firmware. If a step fails, the panel shows the relevant troubleshooting instructions.

No installation, ESP-IDF setup or local server is needed for these downloads. The published firmware uses the lesson's default parameters; simulation sliders do not change its contents.

## First installation and recovery

The page first attempts automatic download mode. If it cannot connect, hold **B (BOOT)**, press and release **R (RESET)**, then release B. Select the USB port again and retry. If the program does not start after writing, press R once.

Downloading replaces the current program. Separate image writes preserve NVS for this project's partition layout; compatibility with unrelated partition layouts is not guaranteed.

## Custom programs

Changing C source or compiled parameters requires a new firmware build. The optional local companion service enables custom compilation and Wi-Fi operations. It is not required for the published USB lesson downloads.
