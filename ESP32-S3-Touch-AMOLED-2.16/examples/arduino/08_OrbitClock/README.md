# Retro Pixel Clock (Orbit project)

Custom Arduino application for the Waveshare ESP32-S3-Touch-AMOLED-2.16, using
its supplied display, touch and sensor setup. This is a local addition, not factory firmware.

The new 480 × 480 screen uses a gray-blue square matrix, a hooded Pikachu based on
the user-supplied reference, and large HH:MM digits. `retro_character.pixels`
contains the editable 57 × 61 reference-grid drawing. Every design tile is retained
on a 64 × 64 canvas and rendered at a 5-pixel pitch without downsampling. Source
lettering is excluded; the small covered face area has been filled with gold tiles.
A second, classic Pikachu is stored in `retro_character_classic.pixels` as a
101 × 104 grid. Its cell colors were read without the printed identifiers or helper
lines; exterior white becomes background while enclosed eye highlights stay white.
The denser design is fitted to 61 × 63 tiles once during asset generation and placed
on the same 64 × 64 canvas. Fine face details are then optically redrawn at the
final resolution: the triangular nose, complete pout and square right-eye highlight.
This prevents thin features from disappearing between sampled rows.
Both forms and the backdrop are indexed 4-bit LVGL
assets stored in flash, so no resizing or color conversion runs on the board.

## Controls

- Tap the character or unused clock background to toggle between the hooded and
  classic forms, with a short four-step pixel hop. The backdrop stays still.
  Time controls, focus controls and settings do not change the character.
  Reboot restores the hooded form.
- Tap the time to switch 12/24-hour display. Dragging does not toggle it.
- A short tap on the character, unused clock background or time also plays the
  selected 2.2-second Pikachu effect. Playback is nonblocking. Taps while starting,
  playing or draining the output are discarded for audio; they never restart,
  overlap or queue another sound. After completion, a new tap can play again.
  Character/time interactions remain responsive. Focus/settings controls and long
  presses do not start sounds; an already playing sound finishes normally.
- Long-press the time without dragging to open the Chinese settings menu.
- Open `专注计时` to choose 1–180 minutes. Short presses change one minute;
  repeated long presses change five. Pause before changing the duration.
- `开始 / 暂停 / 继续` controls the timer. `重置` restores the selected duration.
  Returning to the clock keeps a running timer active. Completion is visual.
- `校准方向` starts the two-pose calibration; `锁定当前方向` disables automatic rotation.

The earlier orbital themes, particle transitions and perspective-drag effects have
been replaced by this reference-based composition. Animation requests are 60 ms
apart only while the character hops; this is not a measured board frame rate.

## Key3 dice game

Single-click **Key3 (GPIO18)** from the clock to enter the dice game. In the dice
page, a single click returns to the clock and a double click opens the sand timer.
The first release waits up to 300 ms to distinguish these actions; a second press
started in that window is accepted even if released just after the deadline.
A single click in sand returns to dice, and the next single click returns to the
clock. Click history is cleared on every page transition. In dice settings the
key returns to the clock; in sand settings it returns to dice and discards drafts. GPIO18 uses its pull-up and active-low
press. The nonblocking 35 ms debouncer emits one event on release, ignores holds
of 800 ms or more, and ignores a key already held during boot.

Default play uses one six-sided die. Long-press the game without dragging to set
**1–3 dice** and a shared **2–8 face count**, then choose `保存`. `取消` discards
changes. Count and faces are stored together in the separate `retro-dice` NVS
namespace; calibration storage is unchanged. Invalid saved values use 1d6.
Selecting **2 faces** forces and stores a count of **1**, disables both count
buttons, and shows an amber-gold flipping coin with `正` / `反`. Its 32 px Chinese result
reads `正面` or `反面`, rather than a numeric total. A full 2-second roll uses the
same uniform random result and busy-motion protection as dice. Returning to
3–8 faces unlocks count adjustment. The model also normalizes saved D2 counts.

The reference-inspired amber resin style uses warm directional shading, folded
foil fragments, fine bright rims and faint translucent highlight bands. The
patterns stay attached to each face while it rolls. A single die has more foil
and reflection detail; two or three use reduced detail within the same fixed
polygon budget. This is a stylized material, without physical refraction or a
texture framebuffer. All scored faces, including D6, carry pale gold numerals.
The D6 chamfer is narrow, and the front numeral has a small dark engraved offset.
The D2 coin has a quiet inset behind its Chinese glyph to keep every stroke clear.

D4 is a tetrahedron, D5 a triangular prism, D7 a pentagonal prism and D8 a regular
octahedron with eight triangular faces. D3 uses only the three rectangular
scoring faces of an elongated triangular prism; its two end caps are blank. Its
horizontal axis, stronger tilt and quiet outlined triangular ends make the cross
section visible beside the numbered sides. A bounded
three-face polyhedron is not implied. The settings caption names the chosen shape.
The front numeral and the separate badge below each die show its result. D6
opposite sides sum to seven. Dice bodies use the available column width and a
bounded y=113–286 region. Each 2-second roll fits its projected bounds before
drawing. The per-die result is about 35 px high; the 48 px total and its y=376
position are retained. The canvas ends at y=333 and the hint is y=334, above the
total caption at y=356. Results remain uniform software draws, rather than a
physical simulation of differently sized faces on the D5/D7 models.

Shake the board after entry; an initial quiet interval is no longer compulsory.
A 300 ms gravity estimate preserves dynamic acceleration. Two opposed peaks above
0.28 g, at least 80 ms apart and within 700 ms, trigger a roll. Slow tilts and an
isolated tap are filtered. The QMI8658 accelerometer uses an 8 g range and
LPF_MODE_3 (13.37% of ODR per the bundled driver), replacing the very narrow
LPF_MODE_0. Polling requests are 20 ms apart in the game and 50 ms outside it.
Accelerometer operation no longer depends on successful gyro initialization.

Dice tumble, bounce and decelerate for 2000 ms; the final values are drawn once
using ESP32 hardware random values with rejection sampling. Each die's result
badge and the total remain visible. Motion during the roll is discarded. After
a result, a quiet 300 ms interval followed by a new opposed shake is required,
so continuous shaking cannot queue or repeat rolls. Touching the screen, stale
samples and free fall reset detection. A missing acceleration stream for 750 ms
shows `未收到运动数据`; failed initialization shows `运动传感器未就绪`.

Opening settings or leaving the game cancels an unfinished roll. Key3 closes clock
settings/calibration and returns from focus mode before entering; a running focus
timer continues in the background. The game holds its entry screen orientation,
so shaking cannot spin the entire page. Long-press/setting interactions do not
play the Pikachu tap sound. The motion sensor must initialize successfully to roll;
otherwise the game shows a sensor message and Key3 still returns to the clock.

Animation updates are requested every 40 ms while rolling. Geometry is cached once
per update and clipped across DMA strips. Back faces are culled, numeral runs are
merged in both axes and projected numerals follow their face planes. Rounded projected outlines are normalized to convex hulls and split into at most
eight vertices per polygon, bounding LVGL software-mask usage. A fixed 224-polygon
cache replaces full-screen rotation/text buffers; lighting and shadows use solid
colors and clipped highlight alpha without blur buffers. Physical sensor sensitivity and real frame rate still need
board checking. The PNG/GIF previews in `outputs/DiceGame-*` are native LVGL renders,
not board recordings. `STATUS` includes `dice`, `dice_count`, `dice_faces` and
`dice_rolling` for on-device checks. It also reports `imu_samples`, `imu_age_ms`,
`imu_read_errors`, `accel`, `dice_armed`, `shake_g` and `finger` to distinguish a
missing stream from motion filtering or a held touch. Dice results do not persist
across reboot. `dice_geometry.h` defines the convex meshes; native topology checks
verify face counts, closed edges, convexity and equal octahedron edges.


## Sand countdown

Double-click Key3 on the dice page to open the sand timer. Curved warm glass,
wooden caps and 460 colorful grains show the remaining sand above, a falling
stream, and the accumulated mound below. The gold **MM:SS** display (up to
`60:00`) sits above `开始 / 暂停 / 继续` and
`重置` controls. At zero, the stream stops and the page shows `时间到`; completion
is visual and does not play a sound. `再来一次` starts a fresh round.

Long-press the glass or page background without dragging to open `流沙设置`.
Duration is **1–3600 seconds**, initially **60**. Use +/-1, +/-10, or 30/60/180 second
presets. Repeated holds change 10 or 60 seconds. `返回游戏` discards the draft;
`保存并返回` saves the chosen duration and resets the hourglass. Failed saving
keeps the settings open. The committed duration is stored as `seconds` in the
separate `retro-sand` NVS namespace. Power-up restores that duration with a full,
stopped hourglass.

Opening settings or returning to dice pauses a running round. Returning without
saving keeps it paused; entering sand again preserves the paused remainder so
`继续` resumes it. Only duration persists across power loss. Timing uses unsigned
monotonic millisecond differences, with pause/resume, exact zero saturation and
wraparound tests; NTP/USB time changes do not affect it.

Sand and dice reuse one polygon cache. The glass is built once per entry; sand
adds a fixed 460-grain cache and two precomputed seat tables (about 7 KiB total).
Settled grains retain their positions and colors, and paused stream grains freeze.
Each draw clips against the active LVGL refresh strip. There are no per-grain LVGL
objects or full-screen pixel buffers. Refresh requests are 40 ms apart only while
sand moves, with a final repaint on completion. The game holds the entry orientation.
Single-click Key3 in sand to return to dice after the 300 ms double-click window;
a further single click returns to the clock. Double-click in sand to open the
starting-player wheel. Sand settings accept an immediate single click back to dice.
`STATUS` adds `sand`, `sand_seconds`, `sand_left_ms`, and `sand_state` (0 idle,
1 running, 2 paused, 3 finished). Computer PNG/GIFs in `outputs/SandGame-*` render
the real UI; they do not measure the board's frame rate.

## Starting-player wheel

Double-click Key3 in the sand game to open `开始玩家`. Default **4 players**;
long-press the wheel/background or start button to set **2–12**. `返回游戏`
discards the draft; `保存并返回` stores it as `players` in the `retro-player`
NVS namespace and resets the wheel. Saving failure stays in settings for retry.
Only the player count persists. A plain click on the wheel or `开始` starts a round.

The pixel circle has equally sized, numbered colored sectors. The pointer spins
at a constant rate for **3000 ms**, then slows continuously for **1200 ms** and
stops at the selected sector center. An unbiased rejection-sampled ESP32 random
value selects the player once at round start; the result stays hidden until the
pointer stops. Busy taps are discarded. The final text reads `玩家 N 先开始`;
`再转一次` starts another round. Opening settings or exiting cancels a running
round without a later result. Single-click Key3 in the wheel or its settings to
return immediately to sand. Entering the wheel pauses sand; return leaves sand
paused, retaining its remaining time.

Two fixed 57 × 57 byte grids cache the circle and current pointer. Sector angles
are calculated only when player count changes. Drawing merges horizontal pixel
runs and clips to the LVGL refresh area, with no per-pixel objects or full-screen
buffer. Animation requests are 40 ms apart only while spinning. The game keeps
its entry orientation and does not trigger character audio. `STATUS` includes
`player`, `player_count`, `player_spinning`, and `player_winner` (zero until done).

## Tap sound

`clock_audio.cpp` uses the official `07_ES8311` codec driver (copied here with its
SPDX notices) and the same shared I2C bus and audio pins: MCLK 42, BCLK 9, WS 45,
DAC data 8, amplifier enable 46. The [official pin map](https://docs.waveshare.net/ESP32-S3-Touch-AMOLED-2.16/)
and [reference sketch](../07_ES8311/07_ES8311.ino) describe the playback path.
No microphone input or SD card is needed. Volume is initially set to 65/100.

The selected `pikachu_effect.wav` is 16000 Hz, 16-bit mono, 35200 samples (2.200 s).
`pikachu_pcm.c` embeds its 70400 PCM bytes in flash. A FreeRTOS worker on core 0
feeds small stereo I2S chunks by copying each mono sample to both slots, while
LVGL remains on its existing loop. An atomic state reserves playback before the
worker is notified. Busy requests are not queued. After the clip, more than a full
DMA ring of silence is written before the amplifier is disabled and the state
returns to idle. Automatic DMA clearing prevents old data looping during idle or
an underrun. Output failure disables playback without stopping the clock.

`STATUS` adds `audio=READY/PLAYING/FAILED`, `audio_error` and `audio_plays` (accepted
play requests since boot). Read these after upload when checking sound and rapid
taps. Actual sound, volume, amplifier behavior and simultaneous UI responsiveness
must still be verified on the physical board.

To replace the clip, keep the selected format/duration and run:

```sh
python3 scripts/generate_pikachu_audio.py
```

This uses Python's standard library; normal Arduino compilation needs only the
already generated C resource. Rebuild/upload after changing it. The source audio
was extracted from the [user-supplied note](https://xhslink.cn/o/Qg1gSYr03n).

## Build and upload

Use Arduino-ESP32 **3.3.11**, the supplied **LVGL 8.4.0**, GFX, SensorLib,
Mylibrary and `lv_conf.h` under `examples/arduino/libraries`. The build settings are
ESP32S3 Dev Module, 16 MB flash, OPI PSRAM, hardware CDC, and
`app3M_fat9M_16MB`. Do not substitute LVGL 9.

From the repository root:

```sh
python3 scripts/orbit_clock.py build
python3 scripts/orbit_clock.py flash --skip-backup --no-stub
```

The direct command replaces firmware without creating an original-flash backup.
The default `flash` command instead requires a complete 16 MB backup before any
write: 115200 baud, 512 KiB segments, up to three attempts per segment. A new run
starts a fresh backup. Backup failure prevents flashing. The user previously
reported successful ROM-mode flashing through VS Code. Run upload tasks from
VS Code or an ordinary terminal with USB serial access.

The task workspace in `outputs/OrbitClock.code-workspace` provides separate
build, direct upload, build-and-upload, backup-and-upload and USB time tasks.
The current revised firmware is compiled; this chat has not uploaded it.

Optional Wi-Fi: copy `clock_secrets.example.h` to `clock_secrets.h` and enter a
2.4 GHz Wi-Fi name/password locally. Credentials are ignored by Git and excluded
from the source package. NTP uses UTC+8 by default. Without Wi-Fi, the upload task
sets time from the computer over USB. Before synchronization, the clock shows
`--:--` and `等待校时`. USB commands remain `TIME <UTC epoch seconds>` and `STATUS`.

## Direction calibration

Direction starts locked in this redesigned application. From the settings menu:

1. Choose `校准方向`. Hold the screen upright with text readable, stop for one
   second and tap `下一步`.
2. Keep the screen facing you, turn the board clockwise by 90°, hold it upright
   and still for one second, then tap `完成校准`.

The two measured gravity vectors establish the screen basis. The gyro vetoes
motion rather than integrating yaw. Automatic rotation requires a calibrated
cardinal direction within 18°, one second of stillness and a three-second interval
between changes. Flat poses, motion, stale samples and impacts hold the page.
Rotation is deferred while touching the screen. A stable sample up to 300 ms before
the tap can be used when the tap nudges the board.

Calibration and lock state are saved in the `retro-clock` NVS namespace. This new
UI does not reuse the older Orbit namespace. Normal segmented firmware uploads
preserve NVS; whole-flash merged uploads or full erases can remove it. Real sensor
orientation and persistence still require board verification.

Software page rotation uses a second 24-line RGB565 output buffer and matching
LVGL input transformation; it does not require a full-screen rotation buffer.
`STATUS` reports `style=retro_dot`, `imu`, `rotation`, `orientation`, `stable`,
`particles=0`, `frame_ms` and `pixels`. The last two are refresh statistics, not FPS.

## Editing assets

Edit hexadecimal palette indices in `retro_character.pixels` (hooded) or
`retro_character_classic.pixels` (classic); `.` means backdrop. The generator defines
a separate 16-color palette for each drawing. `restore_classic_face()` contains
the final-resolution facial corrections; regeneration preserves those corrections.
With Pillow installed, run:

```sh
python3 scripts/generate_retro_assets.py
```

This rebuilds `retro_assets.c/.h`, both character PNGs and the enlarged drawings in
`outputs`. No source photograph is required to rebuild. The time geometry is in
`retro_clock_face.h`. Chinese 16/20 px UI subsets, the 32 px coin-result subset and `coin_glyphs.h` can be regenerated
with `python3 scripts/generate_orbit_fonts.py`. Building the sketch itself does not
require Pillow. Native tests cover every minute of the day and placeholder digits.

## Limits

The onboard RTC is not used; power loss requires NTP or USB time synchronization.
Hour format, chosen duration and timer state reset after restart. Calibration,
orientation lock, dice count/face configuration and sand duration are the saved UI settings. Countdown uses monotonic time and is
unaffected by NTP corrections. There is no sound alarm, weather, battery estimate,
voice control, Bluetooth bridge or automatic brightness. Board frame rate,
brightness, touch behavior and actual physical rotation need on-device checking.
Native LVGL PNG/GIF previews render the real UI code on the computer; they are not
photographs or recordings of the board.
