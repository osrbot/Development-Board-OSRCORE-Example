# Local development and controller downloads

The companion service compiles lesson parameters and provides USB flashing and Wi-Fi OTA. GitHub Pages serves lessons and resources; it does not run the compiler.

```sh
cd docs
npm ci
npm run build
cd ..
node tools/lab-server.mjs
```

Open `http://localhost:5173/` on the computer connected to the controller. LAN clients can browse using the host IPv4 address on port 5173. Web Serial requires a supported browser and a secure context; ordinary LAN HTTP cannot access USB.

Install ESP-IDF v6.0.1 at `~/esp/esp-idf`, or set `OSRCORE_IDF_PATH`.

1. Run the browser simulation and choose firmware parameters.
2. Connect the controller over USB.
3. Compile and download the current lesson. Changing the lesson or parameters invalidates a previous build.
4. Reconnect USB to inspect boot logs. Configure Wi-Fi through USB and read the pairing information.
5. Connect through the Wi-Fi tab to install the application into its OTA slot. Reconnect after reboot and verify the program and build identifier.

Builds are stored in `.cache/lesson-builds`, keyed by source fingerprint, lesson and parameters, with SHA-256 image hashes. USB flashing writes the bootloader, partition table, OTA initialization data and application, preserving NVS for this project's partition layout. This does not guarantee compatibility with unrelated firmware layouts. Wi-Fi updates only the application OTA slot.

Wi-Fi requests are forwarded by the local service to the explicitly entered LAN IPv4 address and authenticated with the pairing code obtained over USB. Only save/reboot commands are exposed over Wi-Fi; continuous motor commands use USB. Moving a simulation slider does not drive the physical controller automatically.

```sh
node --test tools/*.test.mjs docs/scripts/*.test.mjs
node tools/check-firmware.mjs
node tools/verify-firmware-artifacts.mjs
```

Physical-board USB, Wi-Fi and peripheral acceptance testing remains pending.
