# From simulation to your controller

[Open the interactive lessons](/#lesson-01) in desktop Chrome or Edge with a USB data cable. No compiler, software installation or local server is needed.

## Install a lesson

Choose a lesson, open its **Download to controller** button and click **Connect and download lesson**. Select the controller's USB port. The website writes and verifies the published firmware, restarts the controller and attempts to reconnect. On completion, it returns to the course and opens **Device serial** at the bottom.

Install the updated lesson firmware once to enable runtime parameters. Switching to another lesson requires installing that lesson's firmware. Changing supported parameters within the same lesson does not require another download.

## Apply and save parameters

Parameters marked **Device parameter** support USB updates: LED brightness and interval, buzzer frequency and duration, servo pulse width, encoder counts per revolution, Kp, task periods and attitude-filter weight.

1. Adjust the controls to preview the simulation.
2. Click **Apply to controller**. The page displays the values acknowledged by the controller; sending a command alone does not count as success. The lesson uses the new values on its next update cycle.
3. Click **Save to controller** to retain those values after restart. Applying without saving only changes the current session; a restart restores saved values or defaults.

Applying a servo pulse moves the servo. Saving it also restores that pulse after restart. Simulation inputs such as tilt, receiver channels and simulated load are not commands to the controller.

The page checks the controller's reported lesson before sending parameters. Older firmware needs an update. If a reply times out, use **Read device info** to check the current values before retrying.

## One console

The bottom console separates **Simulation output** from **Device serial**. Device serial contains real logs, command input, device information, restart and disconnect. It remains connected when the download panel closes or the selected lesson changes. Collapse the console when it is not needed.

## Recovery

The installer first attempts automatic download mode. If it cannot connect, hold **B (BOOT)**, press and release **R (RESET)**, then release B. Select the USB port and retry. If the application does not start after writing, press R once.

Downloading replaces the program while retaining NVS under this project's partition layout, including saved lesson parameters. The installer provides troubleshooting for USB access, occupied ports and firmware verification errors.

## Source code

Code excerpts explain the lesson and reflect the page parameters. **Apply to controller** sends supported parameters; it does not compile or execute arbitrary C edits. Changing program logic still requires building firmware with ESP-IDF.
