# reflbo on the LilyGo T5-ePaper-S3: design spec

- **Date:** 2026-10-06
- **Status:** Draft for the owner's review (r1)
- **Covers:** the fork `Vybo/reflbo-t5`, milestones T0–T4
- **Related:** the upstream design spec [`2026-09-25-firmware-design.md`](2026-09-25-firmware-design.md) (r44 at the fork's base), `AGENTS.md`

## 1. Purpose and scope

This fork ports reflbo to a second board, the LilyGo T5-ePaper-S3 (4.7″ e-paper, 960×540, 16 grays), and keeps the Waveshare ESP32-S3-RLCD-4.2 working unchanged. Both boards build from one codebase, chosen at build time, so the fork can merge back into `Vybo/reflbo` later.

The fork starts from upstream `0d88d3f` (spec r44). Upstream's later milestones (M7 MQTT and Home Assistant, M8 audio, M9 microSD) are built upstream. The fork takes them when it merges `upstream/main`, before it goes back as one pull request.

The upstream spec stays authoritative for everything this spec doesn't change. This spec covers what the T5 needs and the seams both boards share.

### 1.1 Requirements

| ID | Requirement |
|---|---|
| TR1 | The T5 runs the same features as upstream at the fork's base, minus the hardware the board lacks (§2.4): dashboards, presets and cycling, the menu, the schedule and night sleep, Wi-Fi and the web configurator, OTA, sync, weather, air quality, the radars, solar and energy |
| TR2 | The RLCD build keeps its behaviour, and its host goldens stay byte-identical |
| TR3 | One codebase; the board is a build-time choice; the fork stays mergeable into upstream |
| TR4 | The T5 draws natively at 960×540, denser than the RLCD (more fields a screen, slightly smaller text), in 16 grays |
| TR5 | E-paper refresh: fast updates, then a clean refresh after a configurable number of them; the web page states the result in words |
| TR6 | Agent-verifiable as upstream: screenshots over USB (in gray), host renders and goldens for both boards |
| TR7 | Power: deep sleep between updates on the T5; its costs measured and recorded in `docs/power.md` |

### 1.2 Decisions

Fork decisions are numbered DT1, DT2, … so they don't collide with upstream's D-numbers.

| # | Decision |
|---|---|
| DT1 | Owner, 2026-10-06: a fork, `Vybo/reflbo-t5`, based on upstream's current `main`, to be merged into the main repository later; later milestones are handled upstream. The owner's board is revision V2.3 without touch. LGPL is acceptable. Hardware the T5 lacks is dropped on that board |
| DT2 | Owner, 2026-10-06: the T5 UI is native and dense at 960×540, not a scaled RLCD canvas: about 1.7× the RLCD's pixel sizes, so a screen holds about 1.5× the content and text is about 15 % smaller physically. The owner approves the T5 layouts from host renders before goldens are committed |
| DT3 | Owner, 2026-10-06: the fork uses the panel's grayscale: a 4 bpp framebuffer, anti-aliased fonts and icons, gray shading. The RLCD stays 1 bpp |
| DT4 | Owner, 2026-10-06: epdiy (LGPL-3.0) is vendored into the repository with a patch that adds an S3 output path for boards whose panel latch is on a shift register, as on V2.3. This is an exception to "dependencies come from the ESP Component Registry" (AGENTS.md §8) |
| DT5 | Owner, 2026-10-06: routine updates are fast (only the changed area, no flashing); a clean refresh follows a configurable number of them and every preset switch; the web page says what the setting means in words |
| DT6 | 2026-10-06: the T5 idles in deep sleep (upstream D3 keeps light sleep on the RLCD). Its panel keeps the image without power, and its 3V3 rail is an LDO, so deep sleep saves what it can't on the RLCD (upstream gotcha 23) |
| DT7 | 2026-10-06: the T5 uses KEY (IO21) and BOOT (IO0) as the two buttons. IO0 also strobes the panel's shift register, so it's read only while the panel is idle. Until the owner can reach them, button behaviour is checked with the console's `btn` |
| DT8 | 2026-10-06: the fork never flashes the RLCD board, which upstream's sessions use. The fork's RLCD build is checked by its host goldens and a clean build |

### 1.3 Out of scope

- Touch (the owner's board has none), board revision V2.4 (§12), audio, the SHTC3's fields, the RTC trim, the microSD (upstream's M9).
- A new feature for either board. Anything beyond porting is a proposal for the owner, as upstream (upstream D10).

## 2. The T5-ePaper-S3 (revision V2.3)

Sources: the vendor repository's `esp32s3` branch (README pin table, `src/ed047tc1.*`, `src/utilities.h`, schematic `T5-ePaper-S3-V2.3.pdf`), the ED047TC1 datasheet, epdiy's `ED047TC1` display and waveform. Checked on 2026-10-06; the board itself not yet.

### 2.1 Chips

| Part | Role | Bus / address |
|---|---|---|
| ESP32-S3-WROOM-1-N16R8 | Same module as the RLCD board: 16 MB flash, 8 MB octal PSRAM, native USB | — |
| ED047TC1 | 4.7″ e-paper, 960×540, 16 grays, 8-bit parallel source drivers, no controller: the MCU drives every frame | parallel |
| 74HCT4094 | 8-bit shift register with output latch: the panel's LE, STV, MODE, OE and the power rails' enables | GPIO 13 (data), 12 (clock), 0 (strobe) |
| LT1945 + CJ78L15 / CJ79L15 | Panel rails: +22 V, −20 V, +15 V, −15 V | enables on the shift register |
| LM358 + trimpot | VCOM, set by hand on the board; software can't change it | — |
| PCF8563 | RTC, with a 32.768 kHz crystal and a rechargeable MS412FE backup cell. INT is not wired to the ESP32 (the schematic's note "主控需增加此IO": the MCU needs this IO added) | I²C `0x51` |
| HX6610S | Li-ion charger, CHRG and STDBY drive LEDs only | — |
| AP2112K-3.3 | 3V3 LDO (V2.4: ME6217 or RT9080) | — |

No temperature or humidity sensor, no audio codec, no speaker.

### 2.2 GPIO map

| GPIO | Function | Notes |
|---|---|---|
| 0 | BOOT button, and the shift register's strobe | Strapping pin. An RTC GPIO: can wake the chip. See §8.3 |
| 1–8 | Panel D1–D7 (1–7), D0 (8) | i80 data bus |
| 12 / 13 | Shift register clock / data | Hold low in deep sleep (§5.4) |
| 14 | BAT_ADC = VBAT × 1/2 (100k/100k) | ADC2_CH3, shared with Wi-Fi (§8.4) |
| 17 / 18 | I²C SCL / SDA | RTC; touch on boards with it |
| 19 / 20 | USB D− / D+ | USB-Serial-JTAG, as on the RLCD board |
| 21 | KEY button | An RTC GPIO: can wake the chip |
| 38 | Panel CKV | RMT |
| 40 | Panel STH | |
| 41 | Panel CKH | i80 WR clock |
| 47 | Touch INT | Unused (no touch) |
| 11, 15, 16, 42 | microSD over SPI: SCK, MOSI, MISO, CS | Unused in the fork |
| 10, 39, 45, 48 | Free | 10 is analog-capable |

### 2.3 Shift-register bits

The vendor driver pushes eight bits, last first, then raises the strobe: output enable, mode, scan direction, STV, negative rail enable, positive rail enable, power disable, latch enable. Which bit reaches which output (QP0–QP7) is checked against the schematic at T1. Power-on order: power disable off, 100 µs, negative rails on, 500 µs, positive rails on, 100 µs, STV high. Power-off runs it backwards.

### 2.4 What the T5 lacks, against the RLCD board

| RLCD board | T5 | In the fork |
|---|---|---|
| SHTC3 temperature and humidity | none | `env.temp`, `env.hum`, `env.dew` and their minimum and maximum hidden; the settings `sensors.temp_offset_c` and `sensors.hum_offset_pct` hidden |
| PCF85063 Offset register | none (PCF8563) | No RTC trim (upstream D25): `rtc get` says so, the Info page leaves it out |
| RTC INT on a GPIO | not wired | Wakes come from the ESP32's timer (§8.2) |
| ES8311/ES7210 and a speaker | none | Nothing today; upstream's M8 needs a capability check when it merges |
| Panel LPM rate, contrast variants | n/a | `panel rate`, `panel fps`, `panel init`, `panel mode` and the menu's and web page's refresh-rate setting hidden |
| No RTC backup cell (upstream D9) | MS412FE fitted | The time survives a power-off unless the cell is flat; the VL flag says when it didn't |

### 2.5 Hardware gotchas (known before T1)

1. **LE and STV are on the shift register** on V2.3, so the S3's LCD peripheral can't generate the latch. Stock epdiy's S3 path needs both on GPIOs (its `lcd_driver.c` routes LE from HSYNC), which V2.4 has (IO48, IO45). Hence DT4.
2. **IO0 is both BOOT and the strobe.** Pressing BOOT while the driver shifts bits corrupts the latch. Held at reset, it enters download mode (upstream gotcha 22).
3. **Floating shift-register lines can switch the panel's rails on.** In deep sleep, data and clock must be held low and the strobe kept high by its pull-up (§5.4).
4. **ADC2 and Wi-Fi share hardware.** While Wi-Fi is on, a battery read can fail (ESP-IDF's ADC2 arbitration).
5. **VCOM is a trimpot.** If contrast looks wrong, it's set by hand on the board, not in software.
6. **The waveform has one temperature range** (epdiy's `ED047TC1`: 20–30 °C), so the missing temperature sensor costs nothing.
7. **The owner can reach only one button for now**, likely RESET. RESET boots the board and keeps it awake 2 s for a PC (upstream gotcha 11). Flashing relies on USB-Serial-JTAG's auto-reset; there is no BOOT for download mode.
8. **Two boards on one Mac** give two `/dev/cu.usbmodem*` ports, so `devlog` can't pick one. The T5's MAC is recorded at T1 and every command names its port (§10.2).

## 3. The fork

- **Remotes.** `origin` is `git@github.com:Vybo/reflbo-t5.git` (public, as upstream). `upstream` is `git@github.com:Vybo/reflbo.git`, fetch only (its push URL is set to `DISABLED`). The local clone is `~/reflbo-t5`, apart from upstream's `~/reflbo`.
- **Branches.** Work happens on the fork's `main`, one commit per task, Conventional Commits, as upstream (AGENTS.md §8). Plan branches follow upstream's practice (`plan/t1`, …).
- **Merging back.** When the owner asks: merge `upstream/main` into the fork, resolve, check both boards (§10), then one pull request to upstream. To keep that cheap:
  - shared code changes only where a seam needs it (§4), and mechanical changes (such as `UI_PX()`, §7.1) land as their own commits;
  - the fork's design lives in this file, not in edits across the upstream spec;
  - AGENTS.md gets one appended section for the T5 (pins, gotchas, commands), and its status line names the fork.
- **Upstream's tags.** `stable-m6d` stays upstream's stable firmware for the RLCD board. The fork tags its own stable T5 build (`t5-stable-tN`) once T1 runs on the panel.

## 4. Board selection and seams

### 4.1 Build

- Kconfig choice `REFLBO_BOARD` in `main/Kconfig.projbuild`: `REFLBO_BOARD_RLCD42` (default) or `REFLBO_BOARD_T5S3`.
- `sdkconfig.defaults.t5` holds the T5's overrides: the board choice, epdiy's options, the T5's idle default.
- `tools/idf.sh` reads `REFLBO_BOARD` (`rlcd42` by default, or `t5`). For `t5` it adds `-B build-t5 -D SDKCONFIG=sdkconfig.t5 -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.t5"`. Both boards build side by side.
- The partition table is the same: both boards have 16 MB flash. The T5 image carries only the T5's fonts and icons, the RLCD image only the RLCD's.

### 4.2 Pins

`components/board/include/board_pins.h` includes `board_pins_rlcd42.h` or `board_pins_t5s3.h`. The battery's ADC unit, channel and divider move into the pin headers (today `sensors.c:27` and `:29` hard-code them).

### 4.3 Capabilities

`board_caps.h` states, per board, at compile time: `BOARD_HAS_ENV_SENSOR`, `BOARD_HAS_AUDIO`, `BOARD_HAS_RTC_TRIM`, `BOARD_HAS_RTC_ALARM_WAKE`, `BOARD_HAS_LPM_RATE`, `BOARD_GRAYSCALE`. The catalogue (`/api/layouts`) publishes them with the panel's size, so the web page hides what the board can't do. Fields a board can't fill are left out of its catalogue and its menu's field lists; a preset from another board that names one draws the placeholder (upstream §5.3).

### 4.4 Display API

`display.h` stops including `st7305.h`. What changes for callers:

- `display_init(void)` and `display_init_warm(const display_state_t *)`. The RLCD's init variant comes from the settings, as today, through `display_set_variant()`, which stays and exists only where `BOARD_HAS_LPM_RATE` is set.
- `display_state_t` keeps its role in the snapshot; its contents are the board's (§5.5).
- `display_set_fast(bool)`: HPM on the RLCD; on the T5, fast updates that don't count toward the clean refresh (§5.3). It replaces the direct `st7305_set_mode()` calls in `app_menu.c`, `app_config.c` and `app_radar.c`.
- `display_commit(bool force)`: as today on the RLCD; on the T5 it finds the changed area and picks the update (§5.3).
- `display_clean(void)`: a clean refresh on the T5, a no-op on the RLCD.
- `display_sleep()` / `display_wake()`: night sleep (§5.6).

### 4.5 RTC

`components/rtc` keeps `pcf85063.c` and gains `pcf8563.c` and a small interface, `rtcchip_*`: read and write the time, the oscillator-lost flag (PCF85063 OSF, PCF8563 VL), and the capabilities (trim, precise set, alarm). `timekeeping.c` and `app.c` call `rtcchip_*` instead of `pcf85063_*`; the trim code runs only with `BOARD_HAS_RTC_TRIM`. On the RLCD every call ends in the same register writes as today.

### 4.6 Sensors

The SHTC3 code is built only with `BOARD_HAS_ENV_SENSOR`. The battery reading takes its ADC unit, channel and divider from the pin header. The battery model and its learning (`battery_model.c`, `battery_learn.c`) are shared.

## 5. Display stack on the T5

### 5.1 epdiy, vendored and patched

- `components/epdiy/` holds epdiy v2.1.3 (upstream commit `42c1612`, LGPL-3.0-or-later) with its licence headers. It is built only for `REFLBO_BOARD_T5S3`. Unused parts (examples, other boards' files, fonts) stay out.
- `components/epdiy/PATCHES.md` records the base and every change, so a newer epdiy can be taken by reapplying them.
- **The patch:** a third render method, `RENDER_METHOD_S3_SR`, selected by the Kconfig option `EPD_S3_SHIFT_REGISTER_LATCH`. It follows epdiy's ESP32 path (one row at a time, the latch through the board's `set_ctrl`) on the S3's hardware: each row's bytes over the LCD_CAM i80 bus (8 data lines, CKH as WR) by DMA, CKV pulses from RMT, STH as a GPIO. epdiy's waveform, lookup and high-level code stay as they are.
- `THIRD_PARTY.md` and `NOTICE` credit epdiy. The fork's own files stay Apache-2.0.

### 5.2 The `epaper` component

Our code, Apache-2.0, built only for the T5:

- The epdiy board definition for T5-S3 V2.3: the shift register (§2.3), the rails' order, `set_ctrl` for LE, STV, MODE and OE, no VCOM setting, a fixed 25 °C for the waveform.
- The board's side of `display` (§4.4): the 4 bpp framebuffer is the one epdiy's high-level API draws from (§6.1), so nothing is converted.
- Pure parts with host tests: the shift-register bit sequences, the changed-area search, the update policy (§5.3).

### 5.3 Updates

- **Fast update:** only the bounding box of what changed. DU when every pixel in it is black or white before and after; GL16 (gray, no flashing) otherwise.
- **Clean update:** GC16 over the whole panel. It flashes.
- **When a clean update runs:**
  - after `display.clean_after` fast updates (a new setting, 1–240, default 60: hourly at upstream's default 1-minute update);
  - on a preset switch (KEY, the cycle, the schedule);
  - on leaving the menu or the config screen;
  - at the end of night sleep;
  - on `panel clean` from the console;
  - at a cold boot and whenever the previous frame isn't known (§5.5).
- **`display_set_fast(true)`** (the menu, the radar's loop) makes updates fast and leaves them out of the count, since leaving the menu cleans anyway.
- **Seconds.** The T5 never draws seconds: a preset that asks for them shows hours and minutes there, and the web page says so on the T5. A refresh every second would keep the board awake and the panel's rails on.
- **The setting on the web's Display page** shows a sentence built from `display.update_min` and `display.clean_after`:
  - "The panel updates every 15 minutes without flashing, and flashes clean every 4th update, about once an hour."
  - "The panel updates every minute without flashing, and flashes clean every 60th update, about once an hour."
  - With `clean_after` 1: "The panel flashes clean at every update, every 15 minutes."
  - The interval reads "about every N minutes" under an hour, "about once an hour", "about every N hours", "about once a day" from 24 hours.
  - A second line: "Preset switches and leaving the menu flash clean too."
- **The menu** gets Display ▸ Clean refresh (the same values), in place of the RLCD's refresh rate.

DU, GL16 and GC16's times and currents on this panel are measured at T1 (`docs/power.md`). If GL16 is too slow for the menu, the menu draws black and white only on the T5.

### 5.4 Panel power and deep sleep

- The rails are on only during an update; `display_commit()` turns them on, updates, and turns them off.
- Before deep sleep: epdiy is de-initialised, the shift register gets all rails off and outputs disabled, data and clock (IO13, IO12) are held low with `gpio_hold_en()` and `gpio_deep_sleep_hold_en()`, IO0 is an input (its 10k pull-up keeps the strobe high, so the latch follows the register, which nothing clocks).
- After a wake, `epaper` releases the holds and sets the register again before anything else touches the panel.

### 5.5 The previous frame across deep sleep

PSRAM loses its contents in deep sleep, but a fast update needs to know what the panel shows. Rendering is a pure function of data, preset and time (upstream §5.3's rule), so:

- The snapshot keeps the CRC-32 of the frame the panel shows and the time it was rendered for.
- On a wake that redraws, the app first renders the shown frame from the snapshot's data at that time. If its CRC matches, that frame is the panel's state, and the new frame updates only what differs. If it doesn't match, or there is no snapshot (a cold boot), the update is a clean one.
- This costs one more render a wake and writes nothing to flash. The render's time on the T5 is measured at T4.

### 5.6 Night sleep

The panel keeps showing the dashboard. At the start of a night the status bar gets a moon and the end time in one fast update; nothing updates until the end time or a button, then a clean update (upstream §9.1 otherwise unchanged).

## 6. Grayscale gfx

### 6.1 Formats

- `gfx_fb_t` gains a format: `GFX_FMT_1BPP` (today's layout: row-major, MSB first, 1 = black) and `GFX_FMT_4BPP` (epdiy's: two pixels a byte, the first in the low nibble, 0 = black, 15 = white).
- The 1 bpp code paths stay as they are, so the RLCD's goldens don't move. Primitives branch on the format once per call, not per pixel.

### 6.2 Colours and blending

- `GFX_BLACK`, `GFX_WHITE` and `GFX_INVERT` stay; `GFX_GRAY(n)`, n = 1–14, joins them.
- On a 1 bpp buffer a gray is drawn as a 4×4 ordered dither. No RLCD drawing uses grays, so nothing changes there.
- Anti-aliased glyphs and icons blend their coverage with what's beneath: towards the ink's level on the background's. `GFX_INVERT` on 4 bpp inverts the level (15 − v).

### 6.3 Fonts and icons

- `tools/fontgen.py` and the icon generator gain a 4-bit output (coverage 0–15 a pixel) beside 1-bit. A font records its depth.
- Glyph width, height and advance widen from `uint8_t` to `uint16_t`, and offsets from `int8_t` to `int16_t`, so numerals over 255 px are possible. The RLCD's fonts are regenerated in the wider format; they draw the same.
- The T5's fonts and icons are generated at its sizes (§7.2) in 4 bits, and committed as C sources as today.

### 6.4 Where the T5 uses gray

Text and icon edges; the radar's rain intensity (in place of `radar_inks()`' dither); the solar chart's fill; the map's land and borders; the status bar's separators. Each is checked for legibility in the T3 renders.

### 6.5 Screenshots, previews, goldens

- `screenshot` prints a PGM (P5, 8-bit) between `-----BEGIN RLCD PGM-----` and `-----END RLCD PGM-----` on a 4 bpp board, and the PBM as today on 1 bpp. `tools/screenshot.py` reads both and writes a PNG.
- `/api/screenshot.bmp` and `/api/preview.bmp` send a 4-bit BMP with a 16-gray palette on the T5.
- The T5's goldens are PGM files in `test/host/golden/t5/`; `render_dashboard` and `render_screen` take `--board t5`. `tools/render.py` and `tools/docs_images.py` handle both.

## 7. Dense UI at 960×540

### 7.1 The UI profile

A per-board profile holds what today is spread as constants: the panel's size, the status bar's height, the font table for each size class (`ui_widget.c:22-35`) and the fit lists, icon sizes, the fixed layouts' slot tables (`ui_layout.c`), the menu's and screens' geometry (`ui_menu_draw.c`, `ui_config.c`, `ui_screens.c`), the split limits (`ui_split.h`), the radar and flight views' sizes (`radar.h`, `ui_radar.h`, `app_flights.c`) and the catalogue's numbers (`ui_catalog.c`, `ui_split.c`, `app_web.c`).

- The RLCD's profile holds today's numbers exactly.
- Fixed paddings inside widgets become `UI_PX(n)`: the identity on the RLCD, × 1.7 rounded on the T5. This lands as one mechanical commit.
- Duplicated literals (`PANEL_W`, `PANEL_H`, the preview's `gfx_fb_size(400,300)`, the web's radar `400`) read the profile.

### 7.2 Sizes on the T5

About 1.7× the RLCD's pixels (DT2): fonts 12 → 20, 16 → 26, 20 → 34, 28 → 46, numbers 48 → 80, 72 → 120, 110 → 180, 130 → 220; icons 16 → 26, 24 → 40, 48 → 80; the status bar 20 → 34. The exact sizes are settled in the T3 renders.

### 7.3 Layouts on the T5

Starting points for the renders (16:9, below the status bar):

| Layout | RLCD | T5 |
|---|---|---|
| Classic | main, date row, 4 small | main, date row, 6 small |
| Weather | now, today, hourly, 2 small | now, today, hourly, 3 small |
| Grid | 3×2 | 4×2 |
| Focus | main, 2 small | main, 3 small |
| Radar, Flights | 400×279 | full width, 16:9 |
| Solar, Energy | as built | rescaled, wider chart |
| Split | cells ≥ 40×20, ≤ 24 | the same ratio trees; cells ≥ 68×34, ≤ 24 |

The menu shows 8 rows. The config, first-run, critical-battery and QR screens keep their content, rescaled. The T5's built-in presets leave out the `env.*` fields.

### 7.4 Web UI

The page reads the panel's size, aspect ratio and capabilities from `/api/layouts` (today `web/style.css:98,101` fix 400 px and 4:3, and `web/app.js:675` the radar's 400). The split editor's grid follows the panel's aspect. The preview shows the gray BMP.

## 8. Time, wake, buttons, battery and power on the T5

### 8.1 RTC

`pcf8563.c`: the time in BCD, the century bit, the VL flag (the clock may be wrong) cleared when the time is set. No alarm and no CLKOUT use; CLKOUT is switched off at every boot, as upstream does for the PCF85063 (upstream gotcha 7). `rtc set` and SNTP write it to the second; the precise set to the millisecond (upstream D25) needs the PCF85063's STOP-bit timing and is left out.

### 8.2 Wake timing

- Wakes come from the ESP32's timer, whose slow clock (the internal RC oscillator) drifts with temperature.
- The board wakes early by a margin, reads the RTC, and waits in light sleep for the minute it renders.
- The margin is learned: after each wake the app compares where the timer woke it with the RTC, and keeps a correction in the snapshot. A pure function, host-tested.
- The RC clock's drift on this board is measured at T1; if it is large, the 8MD256 source is measured as an alternative.

### 8.3 Buttons

- KEY on IO21, BOOT on IO0, both active low with external pull-ups, with the same gestures and timings as upstream (upstream §5.6).
- IO0 is read only while the panel is idle: `epaper` tells the buttons task when it drives the strobe, and the task ignores IO0 then. A press that spans an update counts from the end of the update.
- Both are ext1 wake sources in deep sleep, and both wake light sleep.

### 8.4 Battery

GPIO14, ADC2 channel 3, 12 dB, divider 2 (the factor setting as upstream). The reading is taken right after a wake, before Wi-Fi starts; with Wi-Fi on, a failed read keeps the last value and is logged. The voltage-to-level model, the manual calibration and the learned curve are upstream's.

### 8.5 Power

The T5 idles in deep sleep (DT6). `docs/power.md` gets a T5 section: the deep-sleep floor (the vendor says about 380 µA; others report about 170 µA), each refresh mode's charge, a minute update's total, a sync's, and the average for upstream's default settings. Upstream's N2 goal (below 2 mA on average) is the yardstick.

## 9. Settings, menu, web and console

- **Settings:** `display.clean_after` (1–240, default 60) joins `display.*`, default in `components/storage` (AGENTS.md §8). On the T5, `display.lpm_hz`, `display.contrast`, `sensors.temp_offset_c` and `sensors.hum_offset_pct` stay in the file but nothing reads or shows them.
- **Menu:** Display ▸ Clean refresh on the T5, in place of Display ▸ Refresh rate. Items for missing hardware are hidden.
- **Web:** the Display page's clean refresh and sentence (§5.3); the capabilities hide the rest (§4.3).
- **Console** on the T5:
  - `panel status`: fast updates since the last clean one, the last update's mode, area and time.
  - `panel clean`: a clean update now.
  - `panel test`: the test pattern, then a 16-step gray ramp.
  - `panel sleep|wake` as upstream.
  - `panel mode|rate|fps|init`, `sensors`' SHTC3 part and the trim lines of `rtc get` are absent.
  - `screenshot`: PGM (§6.5).

## 10. Testing and verification

### 10.1 Levels

As upstream (AGENTS.md §7), for both boards:

1. **Build:** `tools/idf.sh build` and `REFLBO_BOARD=t5 tools/idf.sh build`, with no new warnings.
2. **Host:** ctest builds the UI for both profiles. The RLCD's goldens must not change, in every task. New tests: the PCF8563 codec, the shift-register sequences, the changed-area search and the update policy, the clean-refresh sentence, the wake margin, 4 bpp primitives and blending, the 4-bit font format, the RLCD's fonts in the wider format drawing the same.
3. **Device (T5):** flash, boot log, console, screenshots compared with the goldens. The test pattern and the gray ramp at T1 and T2.
4. **Owner:** the panel's look (contrast, ghosting, legibility of the dense layouts), current, and the buttons once reachable.

### 10.2 Rules at the board

- The T5's port is identified by its MAC (`esptool read_mac`), recorded in AGENTS.md at T1. Every `devlog`, `screenshot` and `flash` names its port.
- Never the RLCD board's port (DT8). If unsure which port is which, read the MAC first.
- No T5 image may turn the USB console off or sleep before it has been up 2 s after a reset: RESET is the only way back while BOOT can't be reached.
- Deep-sleep tests wake by timer.
- Nothing erases flash, NVS or the storage partition without the owner's word (upstream rule 3).

## 11. Milestones

Each gets its own plan in `docs/plans/`, written just before it starts.

| # | Milestone | Done when |
|---|---|---|
| T0 | The seams: the Kconfig board choice, `idf.sh`'s `REFLBO_BOARD`, the pin headers, capabilities, the display, RTC and sensor seams, the UI profile holding the RLCD's numbers; the T5 builds with a display that draws nothing and the RLCD's profile | Both boards build clean; the RLCD's goldens byte-identical; host tests pass |
| T1 | Bring-up: vendored epdiy with its patch, `epaper`, rails and deep sleep, the PCF8563, the buttons, the battery; every update clean; DU, GL16 and GC16 measured. Until T3 the app still draws the RLCD's 400×300 1 bpp frame, which `epaper` places in the panel's centre | The test pattern and the clock on the panel (owner confirms); the T5's MAC and the timings recorded |
| T2 | Grayscale gfx: 4 bpp, anti-aliased fonts and icons, PGM screenshots, gray BMPs | The gray ramp and anti-aliased text on the panel match their screenshots |
| T3 | The dense UI: the T5 profile, layouts, menu, screens, presets, the web page | The owner approves the renders; the T5's goldens committed; every layout on the panel |
| T4 | E-paper behaviour: fast and clean updates, the setting and its sentence, the previous frame across deep sleep, the radar's loop, night sleep, wake timing; power | Fast updates without artifacts across wakes; the average current measured |

## 12. Risks and open items

- **The epdiy patch** reaches into its render internals; a newer epdiy may need the patch reworked (`PATCHES.md`). If it proves unworkable at T1, the fallback is our own driver on epdiy's waveform data (the option the owner didn't pick).
- **Refresh times and currents are unknown** until T1. They set `clean_after`'s default, the menu's mode and the radar loop's frame time.
- **Ghosting** from fast updates between clean ones; the default may need lowering after the owner sees the panel.
- **The RC clock's drift** may need a larger wake margin, costing awake time.
- **BOOT on the strobe:** a press during an update could glitch the panel's control lines for that update; the next clean update repairs it.
- **Image size:** T5 fonts at 4 bits and up to 220 px; the app has about 1.5 MB free in its 4 MB slot at the fork's base.
- **Radar frames at 960 px** are about 2.4 times the RLCD's pixels; the frame file and decode buffers grow with them (upstream gotcha 36 caps snapshot blocks, not the radar's file).
- **Merge conflicts** with upstream's M7 in `components/ui`, mostly from `UI_PX()`. Merging `upstream/main` into the fork after each upstream milestone keeps them small.
- **V2.4 support** would use stock epdiy's S3 path (LE on IO48, STV on IO45) through a third board choice. Not planned.
- **Second button:** the owner can reach only one button (likely RESET) for now; the buttons' owner check waits.

## 13. Revision history

| Rev | Date | Change |
|---|---|---|
| r1 | 2026-10-06 | First draft from the owner's answers (DT1–DT5) and the agreed design sections |
