# Orbit Clock AI

Pixel desktop clock, interactive character audio, focus timer, dice, sand countdown, player wheel and XiaoZhi voice assistant for the Waveshare ESP32-S3-Touch-AMOLED-2.16.

**[中文说明 / README_CN.md](README_CN.md)**

![Clock preview](docs/assets/clock.png)

![Custom eyes animation](docs/assets/eyes.gif)

The 2026-10-27 package includes prebuilt firmware, maintained sources, required source dependencies, portable VS Code tasks, SHA-256 checksums and a privacy audit. XiaoZhi uses the extracted looping eyes GIF. Key3 returns from voice mode; there is no on-screen return button.

Open `OrbitClockAI.code-workspace` to flash. Firmware is in `outputs/OrbitClockAI-firmware/`; the application address is **0x20000**, and assets start at **0x800000**. No NVS image or full-device dump is distributed.

Use ESP-IDF >=6.0.1 to rebuild the combined board. XiaoZhi is pinned to `49b4380b3faa0ceadd28a3e78aaf0f311c46c57e`. See the Chinese guide for setup, activation, controls, build commands and testing limits.

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for software licenses and the separate provenance of character artwork, audio, font subsets and extracted video imagery. Those media assets are not relicensed by the code license.
