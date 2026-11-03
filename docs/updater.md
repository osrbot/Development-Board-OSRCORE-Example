# Build and download to the board

[Open the interactive lessons](/#lesson-01), select a lesson, then open **Connect board**.

1. Run the browser model and change firmware parameters.
2. Connect USB, build current parameters, and download the resulting firmware.
3. Reconnect device logs to observe real output. Parameter changes require a new build.
4. Provision Wi-Fi over USB with the new lesson firmware first, then update through the same device panel.

Resources, source and build artifacts follow the current refactored curriculum. Legacy prebuilt firmware, remote URL downloads and full-flash recovery are no longer offered here.

Local compilation requires `tools/lab-server.mjs`. For USB, open localhost on the computer connected to the board. Hardware validation is still pending.
