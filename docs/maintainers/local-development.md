# Local development and controller downloads

GitHub Pages serves the website and prebuilt firmware. The optional local service previews the site and supports maintainer build tools and a Wi-Fi bridge.

```sh
cd docs
npm ci
npm run build
cd ..
node tools/lab-server.mjs
```

Open `http://localhost:5173/` on the computer connected to the controller. LAN clients can browse using the host IPv4 address on port 5173. Web Serial requires a supported browser and a secure context; ordinary LAN HTTP cannot access USB.

The customer workflow uses published, prebuilt lesson firmware. It needs neither ESP-IDF nor the local service. Install the selected lesson over USB, wait for automatic reconnection, then apply or save supported parameters through the shared device session. Changing simulation-only inputs does not send commands to hardware.

For firmware development, install ESP-IDF v6.0.1 at `~/esp/esp-idf`, or set `OSRCORE_IDF_PATH`. The local build tools can generate custom firmware; they are not part of the website's parameter workflow. Build caches live in `.cache/lesson-builds` and include source fingerprints and image hashes.

USB installation writes the bootloader, partition table, OTA initialization data and application while preserving NVS for this project's partition layout. This does not guarantee compatibility with unrelated firmware layouts. The normal lesson installer uses USB. Wi-Fi connectivity remains an optional local-service feature: configure the controller over USB, read its pairing information, then use the local Wi-Fi tab for supported commands and logs.

Wi-Fi requests are forwarded by the local service to the explicitly entered LAN IPv4 address and authenticated with the pairing code obtained over USB. Only save/reboot commands are exposed over Wi-Fi; continuous motor commands use USB. Moving a simulation slider does not drive the physical controller automatically.

```sh
node --test tools/*.test.mjs docs/scripts/*.test.mjs
node tools/check-firmware.mjs
node tools/verify-firmware-artifacts.mjs
```

Test physical peripherals separately; browser and unit tests do not establish hardware behavior.
