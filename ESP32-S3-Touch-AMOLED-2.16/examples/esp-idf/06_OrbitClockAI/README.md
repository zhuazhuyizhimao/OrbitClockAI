# Orbit Clock AI

[简体中文使用说明](README_ZH.md)

Custom XiaoZhi v2.5.2 board for the Waveshare ESP32-S3-Touch-AMOLED-2.16. It retains the existing pixel clock, both Pikachu forms, single-flight click sound, focus timer, calibrated rotation, dice (D2–D8), sand timer and player wheel. Say “你好，小智” after Wi-Fi provisioning and activation, or open XiaoZhi from the clock settings. Key3 keeps its game navigation and returns from voice mode to the previous page.

Voice mode pauses sand, cancels a dice roll or wheel spin, and retains the current page. Focus time continues. One AudioService owns the codec; the existing 2.2-second effect is supplied as Ogg Opus without installing another I2S driver. Wake detection is paused during the effect and restored afterwards.

Wi-Fi connects automatically at boot, preferring the most recently successful saved SSID, then falling back to other saved networks. **Key2 / BOOT (GPIO0)** retries a saved connection while offline, or requests another NTP sync while online without interrupting voice. Repeated clicks do not restart an active connection attempt. An empty saved list or a 60-second startup connection timeout opens provisioning. Key2 can leave provisioning and retry saved credentials. Wake and the clock settings menu remain the voice entry points; Key3 navigation is unchanged.

Every successful IP connection requests nonblocking NTP sync. A real time-sync callback confirms success; timeout is reported after 30 seconds and retried later. Clock time is UTC with one CST display conversion. XiaoZhi receives the same connection/disconnection events for activation and its protocol lifecycle; Internet access and device activation are still required for cloud conversation. A Wi-Fi or NTP success message does not claim cloud availability.

The panel uses the working Arduino CO5300 MADCTL value with no additional hardware mirror, and raw touch coordinates are normalized before software page rotation. The local Wi-Fi component fork is documented in [ORBIT_CHANGES.md](network/esp-wifi-connect/ORBIT_CHANGES.md).

Upright means viewing the screen with the three physical buttons at the top. Following hardware feedback that the previous image was 90 degrees clockwise in that pose, the final panel transfer adds one counterclockwise quarter turn and raw touch applies the matching inverse mounting transform. This covers every screen, including voice and provisioning, after saved or automatic logical page rotation. Existing settings remain intact; use the same buttons-up pose for the first orientation-calibration step.

The board wraps the unchanged CST9217 1.0.4 report reader to send the missing `D0 00 AB` acknowledgement, clears stale contacts on read/ACK failure, and polls touch every 20ms. Game buttons have pressed feedback and a small extended hit area. Regression checks exercise the actual vendor decoder with ACK-gated reports, release/recovery, and every dice/sand/player control through all four rotations.

The sand page has no separate start button. Tap the hourglass, countdown or background to start, pause or resume; tap after completion to restart. A single wide reset button restores the configured duration without starting. Hold the screen for two seconds to open duration settings. Small finger drift is tolerated; a drag cancels the gesture. A settings hold consumes its release. Key3 navigation is unchanged.

The combined application removes both legacy main-button callbacks and their two-button layout. An independent page handler owns tap-to-toggle. The new wide reset control has exactly its visible bounds, with no extended reset hit area, and consumes its own click. Tapping to the right of the hourglass only toggles the timer without resetting remaining time.

One complete press/release owns one action. A new page press must begin at least 500ms after the previous accepted toggle. A two-second hold takes priority, opens settings once, and consumes its release. Invalid/missing packets are not releases: explicit releases debounce for 80ms, while stale contacts expire after 1.2 seconds, covering sparse reports without latching a disconnected controller indefinitely.

The first sand entry offers three-target touch calibration when no saved mapping exists. Hold the board with its three keys at the top and tap/release each displayed target in order. The overlay consumes raw coordinates independently of existing hit testing, solves an affine mapping, saves it to the new `orbit-touch` NVS namespace, and restores the previous page rotation. Hold Key3 for three seconds to recalibrate; a Key3 click cancels without changing the saved mapping. Existing Key3 single/double navigation remains unchanged. Bad samples or save failure retain the previous mapping.

Key2 now acknowledges press immediately and acts on release, avoiding dropped requests when the button library classifies repeated taps as a double click. Connection attempts remain single flight. With no saved SSID, it also reopens provisioning UI if the user previously dismissed it.

From the repository root:

```sh
python3 scripts/orbit_ai.py prepare
python3 scripts/orbit_ai.py build
python3 scripts/orbit_ai.py flash --skip-backup
```

Use ESP-IDF >=6.0.1 (6.1 recommended). Source is pinned to `49b4380b3faa0ceadd28a3e78aaf0f311c46c57e`. Edit this directory's board sources or the maintained Arduino UI / port generator, rather than generated files in the task's `work/xiaozhi-upstream` checkout.

Prebuilt images and `OrbitClockAI.code-workspace` are in the task's `outputs` directory. The ROM flashing task verifies five separate image hashes, lengths, addresses and the actual partition table before serial access. NVS remains at 0x9000/0x5000. The AI app starts at **0x20000**, and assets at 0x800000; do not use the Arduino app offset. The explicit `--skip-backup` task does not create a full board backup. Without that option, an unsuccessful backup stops flashing.

The board has a distinct `orbit-clock-ai` identity and disables network firmware upgrades so the combined UI cannot be replaced by a stock image. Server discovery/activation remains enabled. Server timestamps stay in UTC; the clock applies its CST timezone once. The original Arduino firmware and rollback task remain available.

Validation commands:

```sh
python3 -m unittest discover -s tests -v
python3 scripts/orbit_ai.py test
python3 scripts/check_orbit_ai_ui.py
```

The complete ESP32-S3 firmware compiles. Host checks exercise real LVGL 9 rendering, partial/full redraw equivalence, all game configurations, rotated input, navigation, voice UI handoff and the actual audio producer with the official Ogg parser. Physical microphone capture, wake accuracy, playback and online conversation still require testing on the board. See the Chinese guide for provisioning, activation, controls, partition details and the first hardware test sequence.

A host simulation does not confirm recovery from an entirely unresponsive physical touchscreen. The serial `STATUS` command also emits `ORBIT TOUCH` with the `sand_stable_v4` UI revision, read/contact/error counters, error code and raw coordinates to distinguish firmware and input-path failures during hardware diagnosis.

Official setup reference: [Waveshare XiaoZhi tutorial](https://docs.waveshare.net/ESP32-S3-Touch-AMOLED-2.16/XiaoZhi_AI/).
