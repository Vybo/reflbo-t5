# reflbo on the LilyGo T5-4.7 (ESP32): design spec

- **Date:** 2026-10-06
- **Status:** r1 approved by the owner on 2026-10-06; r2 recorded T0 as built; r3, for the board the owner actually has (DT9), approved on 2026-10-07; r4 records T1 as built; r5 records T2 (§13)
- **Covers:** the fork `Vybo/reflbo-t5`, milestones T0–T4
- **Related:** the upstream design spec [`2026-09-25-firmware-design.md`](2026-09-25-firmware-design.md) (r44 at the fork's base), `AGENTS.md`

## 1. Purpose and scope

This fork ports reflbo to a second board, the LilyGo T5-4.7 e-paper board (4.7″, 960×540, 16 grays) on a classic ESP32. It keeps the Waveshare ESP32-S3-RLCD-4.2 working unchanged. Both boards build from one codebase, chosen at build time, so the fork can merge back into `Vybo/reflbo` later.

Until T1's bring-up the spec assumed LilyGo's newer T5-ePaper-S3. The board on the owner's desk turned out to be the ESP32 one (DT9), so r3 describes that board. The S3 work (T1 tasks 1–5, through commit `7232440`) stays in git's history; nothing of it is planned.

The fork starts from upstream `0d88d3f` (spec r44). Upstream's later milestones (M7 MQTT and Home Assistant, M8 audio, M9 microSD) are built upstream. The fork takes them when it merges `upstream/main`, before it goes back as one pull request.

The upstream spec stays authoritative for everything this spec doesn't change. This spec covers what the T5 needs and the seams both boards share. "The T5" means the LilyGo T5-4.7 throughout.

### 1.1 Requirements

| ID | Requirement |
|---|---|
| TR1 | The T5 runs the same features as upstream at the fork's base, minus the hardware the board lacks (§2.4): dashboards, presets and cycling, the menu, the schedule and night sleep, Wi-Fi and the web configurator, OTA, sync, weather, air quality, the radars, solar and energy |
| TR2 | The RLCD build keeps its behaviour, and its host goldens stay byte-identical |
| TR3 | One codebase; the board is a build-time choice, each with its own ESP-IDF target; the fork stays mergeable into upstream |
| TR4 | The T5 draws natively at 960×540, denser than the RLCD (more fields a screen, slightly smaller text), in 16 grays |
| TR5 | E-paper refresh: fast updates, then a clean refresh after a configurable number of them; the web page states the result in words |
| TR6 | Agent-verifiable as upstream: screenshots over USB (in gray), host renders and goldens for both boards |
| TR7 | Power: deep sleep between updates on the T5; its costs measured and recorded in `docs/power.md` |

### 1.2 Decisions

Fork decisions are numbered DT1, DT2, … so they don't collide with upstream's D-numbers. A decision later revised keeps its row and says so.

| # | Decision |
|---|---|
| DT1 | Owner, 2026-10-06: a fork, `Vybo/reflbo-t5`, based on upstream's current `main`, to be merged into the main repository later; later milestones are handled upstream. The owner's board is revision V2.3 without touch. LGPL is acceptable. Hardware the T5 lacks is dropped on that board. (Revised by DT9: the board is the ESP32 T5-4.7) |
| DT2 | Owner, 2026-10-06: the T5 UI is native and dense at 960×540, not a scaled RLCD canvas: about 1.7× the RLCD's pixel sizes, so a screen holds about 1.5× the content and text is about 15 % smaller physically. The owner approves the T5 layouts from host renders before goldens are committed |
| DT3 | Owner, 2026-10-06: the fork uses the panel's grayscale: a 4 bpp framebuffer, anti-aliased fonts and icons, gray shading. The RLCD stays 1 bpp |
| DT4 | Owner, 2026-10-06: epdiy (LGPL-3.0) is vendored into the repository, an exception to "dependencies come from the ESP Component Registry" (AGENTS.md §8). r3: on the ESP32 it runs its own I2S path and its own `epd_board_lilygo_t5_47` board definition unchanged; the S3 patches are gone, and one patch remains, the LUT's PSRAM fallback (§5.1) |
| DT5 | Owner, 2026-10-06: routine updates are fast (only the changed area, no flashing); a clean refresh follows a configurable number of them and every preset switch; the web page says what the setting means in words |
| DT6 | 2026-10-06: the T5 idles in deep sleep (upstream D3 keeps light sleep on the RLCD). Its panel keeps the image without power, and its 3V3 rail is an LDO, so deep sleep saves what it can't on the RLCD (upstream gotcha 23) |
| DT7 | 2026-10-06, revised 2026-10-07: the T5's two buttons are KEY on IO34 and BOOT's role on IO35; IO39, the third side button, stays free. The real BOOT (IO0) is never read: it strobes the panel's shift register. Until the owner can reach the side buttons, button behaviour is checked with the console's `btn` |
| DT8 | 2026-10-06: the fork never flashes the RLCD board, which upstream's sessions use. The fork's RLCD build is checked by its host goldens and a clean build |
| DT9 | Owner, 2026-10-07: the board on the desk is the ESP32 LilyGo T5-4.7 (ESP32-D0WD-V3 in a WROVER module, CH9102 USB-serial, MAC `34:ab:95:5e:5d:58`), not the T5-ePaper-S3; port to it. The ESP32 becomes the T5's ESP-IDF target |
| DT10 | Owner, 2026-10-07: no RTC module is added. The T5 keeps time with the ESP32's own clock between syncs (NTP) and loses it at power-off until the next sync (§8.1) |
| DT11 | 2026-10-07: the T5's console is UART0 through the CH9102. A PC can't be detected on it, so console input keeps the board awake for 5 minutes instead (§9) |
| DT12 | Owner, 2026-10-07 (T1 bring-up): if the ESP32's clock drifts too much, one NTP sync a day is acceptable (upstream's default schedule); a trim from sync to sync stays a proposal (§8.1, §12) |

### 1.3 Out of scope

- Touch (the owner's board has none), an RTC module (DT10), audio, the SHTC3's fields, the RTC trim, the microSD (upstream's M9).
- The T5-ePaper-S3 (§12).
- A new feature for either board. Anything beyond porting is a proposal for the owner, as upstream (upstream D10).

## 2. The LilyGo T5-4.7 (ESP32, revision V2.3)

Sources: the vendor repository `Xinyuan-LilyGO/LilyGo-EPD47` (schematic `T5-ePaper.pdf`, `src/utilities.h` and `src/ed047tc1.h` for the ESP32), epdiy's `epd_board_lilygo_t5_47.c`, the ED047TC1 datasheet, and the board itself on 2026-10-07 (esptool: ESP32-D0WD-V3 rev 3.0, 16 MB flash; USB: WCH CH9102, VID 0x1A86, PID 0x55D4).

### 2.1 Chips

| Part | Role | Bus / address |
|---|---|---|
| ESP32-WROVER-E (ESP32-D0WD-V3) | Dual-core Xtensa LX6, 240 MHz, Wi-Fi, BT (unused); 16 MB flash, 8 MB PSRAM of which 4 MB is mapped | — |
| CH9102 | USB-serial on UART0, with the usual DTR/RTS auto-reset to EN and IO0 | USB-C |
| ED047TC1 | 4.7″ e-paper, 960×540, 16 grays, 8-bit parallel source drivers, no controller: the MCU drives every frame | parallel |
| 74HCT4094 | 8-bit shift register with output latch: the panel's LE, STV, MODE, OE, PWR_EN and the rails' enables | IO23 (data), IO18 (clock), IO0 (strobe) |
| LT1945 + CJ78L15 / CJ79L15 | Panel rails: +22 V, −20 V, +15 V, −15 V | enables on the shift register |
| LM358 + trimpot | VCOM, set by hand on the board; software can't change it | — |
| HX6610S | Li-ion charger; CHRG and STDBY drive LEDs only | — |
| AP2112K-3.3 | 3V3 LDO | — |

No RTC, no temperature or humidity sensor, no audio codec, no speaker.

### 2.2 GPIO map

| GPIO | Function | Notes |
|---|---|---|
| 0 | The shift register's strobe; also BOOT | Strapping pin; an RTC GPIO. Never read as a button (DT7) |
| 1 / 3 | UART0 TX / RX | The console, through the CH9102 |
| 5 | Panel CKH | Strapping pin |
| 18 / 23 | Shift register clock / data | Digital pads: held through deep sleep with `gpio_deep_sleep_hold_en()` (§5.4) |
| 25 | Panel CKV | RMT |
| 26 | Panel STH | |
| 33, 32, 4, 19, 2, 27, 21, 22 | Panel D0–D7 | I2S1 data. IO2 is a strapping pin |
| 34 | KEY (side button) | Input only, external 100k pull-up; an RTC GPIO (ext0 wake) |
| 35 | BOOT's role (side button) | Input only, external 100k pull-up; an RTC GPIO (ext1 wake) |
| 39 | Third side button | Input only, external 100k pull-up; unused |
| 36 | BAT_ADC = VBAT × 1/2 (100k/100k, 1 %) | ADC1 channel 0, free of Wi-Fi |
| 12–15 | microSD over SPI (MISO, MOSI, SCK, CS); the touch connector's I²C on 14 (SCL) and 15 (SDA), INT on 13; the Grove connectors | Unused in the fork. IO12 and IO15 are strapping pins |

### 2.3 Shift-register bits

The bits go out last first, then the strobe rises: output enable, mode, PWR_EN (epdiy calls it "scan direction"), STV, negative rails, positive rails, power disable, latch enable. Power-on: PWR_EN on and power disable off, 100 µs, negative rails, 500 µs, positive rails, 100 µs, STV. Power-off runs it backwards and leaves PWR_EN off. epdiy's board definition does all of this; the fork only needs the "all off" word at boot and before deep sleep (§5.2).

### 2.4 What the T5 lacks, against the RLCD board

| RLCD board | T5 | In the fork |
|---|---|---|
| SHTC3 temperature and humidity | none | `env.temp`, `env.hum`, `env.dew` and their minimum and maximum hidden; the settings `sensors.temp_offset_c` and `sensors.hum_offset_pct` hidden |
| PCF85063 RTC | none (DT10) | The time is the ESP32's own clock (§8.1); no trim, no alarm |
| RTC INT on a GPIO | none | Wakes come from the ESP32's timer (§8.2) |
| ES8311/ES7210 and a speaker | none | Nothing today; upstream's M8 needs a capability check when it merges |
| Panel LPM rate, contrast variants | n/a | `panel rate`, `panel fps`, `panel init`, `panel mode` and the menu's and web page's refresh-rate setting hidden |
| Native USB console | CH9102 USB-serial | The console on UART0 at 115200 (§9) |

### 2.5 Hardware gotchas

1. **No RTC.** Power-off loses the time; the clock reads 1970 until a sync or a manual set (§8.1).
2. **IO0 is both BOOT and the strobe.** Pressing BOOT while the driver shifts bits corrupts the latch, which the next clean update repairs. Held at reset, it enters download mode.
3. **Floating shift-register lines can switch the panel's rails on.** In deep sleep, IO18 and IO23 are digital pads: they keep their level only with `gpio_hold_en()` plus `gpio_deep_sleep_hold_en()`; IO0 stays high on its pull-up (§5.4).
4. **The panel bus uses strapping pins** (IO0, IO2, IO5). They must be inputs or at their boot level at every reset; epdiy releases them when it powers the panel off.
5. **VCOM is a trimpot.** If contrast looks wrong, it's set by hand on the board, not in software.
6. **The waveform has one temperature range** (epdiy's `ED047TC1`: 20–30 °C), so the missing temperature sensor costs nothing.
7. **The ESP32 can't wake on "any of these pins low"** (its ext1 knows ALL_LOW and ANY_HIGH only), so KEY wakes through ext0 and BOOT's role through ext1 with one pin (§8.2).
8. **The CH9102's DTR/RTS reset the chip.** esptool resets into download mode with them, no button needed. A tool that opens the port must change DTR and RTS together, or it resets the board (§9, §10.2). `devlog`'s order (both asserted, then RTS released before DTR) leaves it running: checked at T1, the clock stayed valid across port sessions.
9. **Two boards on one Mac:** the RLCD is `/dev/cu.usbmodem*`, the T5 `/dev/cu.usbserial-*`. `devlog` picks a `usbmodem` port by itself, which is never the T5: every T5 command names its port.
10. **The ESP32's internal RAM is tighter than the S3's.** epdiy's LUT falls back to PSRAM when internal RAM is short (§5.1); T1 measures the free internal RAM with Wi-Fi and epdiy up.
11. **The owner can reach only RESET for now.** RESET boots the board and keeps it awake 2 s; esptool needs no button.
12. **esptool fails above 230400 baud on this CH9102** (macOS, T1): at 921600 and 460800 the stub's first reply after the baud change is corrupt. Every esptool and `flash` call on the T5 passes `-b 230400`; reading the whole 16 MB takes about 13 minutes.
13. **A UART wake loses its characters.** The bytes that wake the ESP32 from light sleep never reach the console, and the app slept again before the next line came, so a light-sleeping board never answered. A UART wake now holds the board 3 s (`UART_WAKE_HOLD_MS`); the next line, such as `devlog`'s nudge a second later, then holds it 5 minutes (DT11).
14. **Light sleep keeps the UART's output until the next wake.** ESP-IDF suspends the UART for light sleep with its FIFO full, so log lines printed just before came out a minute later. `power_sleep_light()` waits for the console UART's output first (up to 100 ms).
15. **Console input outranks `sleep test`.** The 5-minute hold from any console line came before the test's cycles in the power policy; `power_start_test()` clears the hold, so the cycles start at once.
16. **The slow clock drifts.** Over 30 one-minute deep-sleep cycles the 150 kHz RC ran about 0.9 % fast (13 minutes a day), the 8MD256 source about 0.1 % slow (85 s a day): the T5 uses 8MD256 (§8.1).
17. **`epd_init()` sets the board every time**, and epdiy warns "EPD board can only be set once!" from the second update on, routine wakes included. The fork sets epdiy's log tag to errors; P5's fallback logs as `epd` and still shows.
18. **The battery reads about 4.78 V with USB in**, the LiPo's charger voltage, so the gauge shows full while USB is connected and the battery's own voltage once unplugged. As upstream, charging is invisible to the firmware.
19. **The ESP32 refuses a light sleep of a millisecond** (`ESP_ERR_SLEEP_TOO_SHORT_SLEEP_DURATION`, which is `ESP_ERR_INVALID_ARG`). A light sleep that woke just before its minute asked for that much, got refused, and the loop went straight back to it without ticking: the board spun awake at 240 MHz for good (115–138 mA on the owner's meter). The app ticks a wake that has come (`sched_wake_due()`) instead of planning a sleep for it. The S3 sleeps that millisecond, so the RLCD never showed it; the shared fix goes upstream with the merge.
20. **USB power keeps the panel's rail on.** In LilyGo's schematic (`T5-4.7.pdf`, the CP2104 revision), the switched `3V3` that feeds the panel, its LT1945 boost converter, the ±15 V regulators, the VCOM amplifier and the blue LED is turned on through Q2 by either POWER_EN (D6, the shift register) or VBUS (D7). With USB in, it stays on whatever the firmware does, and the USB-serial chip runs too: the owner's USB meter read about 65 mA with the ESP32 in deep sleep and 67 mA in light sleep (T1, 2026-10-08), both the hardware's floor. The sleep current exists only on battery, where POWER_EN switches the rail off; LilyGo quotes about 380 µA. A USB meter can't measure it (§8.5).

## 3. The fork

- **Remotes.** `origin` is `git@github.com:Vybo/reflbo-t5.git` (public, as upstream). `upstream` is `git@github.com:Vybo/reflbo.git`, fetch only (its push URL is set to `DISABLED`). The local clone is `~/reflbo-t5`, apart from upstream's `~/reflbo`.
- **Branches.** Work happens on the fork's `main`, one commit per task, Conventional Commits, as upstream (AGENTS.md §8).
- **Merging back.** When the owner asks: merge `upstream/main` into the fork, resolve, check both boards (§10), then one pull request to upstream. To keep that cheap:
  - shared code changes only where a seam needs it (§4), and mechanical changes (such as `UI_PX()`, §7.1) land as their own commits;
  - the fork's design lives in this file, not in edits across the upstream spec;
  - AGENTS.md gets one appended section for the T5 (pins, gotchas, commands), and its status line names the fork.
- **Upstream's tags.** `stable-m6d` stays upstream's stable firmware for the RLCD board. The fork tags its own stable T5 build (`t5-stable-tN`) once T1 runs on the panel.

## 4. Board selection and seams

### 4.1 Build

- Kconfig choice `REFLBO_BOARD` in `main/Kconfig.projbuild`: `REFLBO_BOARD_RLCD42` (default) or `REFLBO_BOARD_T547`.
- Each board has its own ESP-IDF target: the RLCD's `esp32s3` (in the shared `sdkconfig.defaults`), the T5's `esp32`. `sdkconfig.defaults.t5`, applied after the shared file, sets the T5's target, quad PSRAM, the console on UART0, 240 MHz, epdiy's options and the T5's idle default.
- `tools/idf.sh` reads `REFLBO_BOARD` (`rlcd42` by default, or `t5`). For `t5` it adds `-B build-t5 -D REFLBO_BOARD=t5 -D SDKCONFIG=sdkconfig.t5 -D IDF_TARGET=esp32`. Both boards build side by side.
- The partition table is the same: both boards have 16 MB flash. The T5 image carries only the T5's fonts and icons, the RLCD image only the RLCD's.
- The ESP32 build sets `CONFIG_ESP32_REV_MIN_3`: the WROVER-E's chip is revision 3, and without the PSRAM cache workaround IRAM fits (about 12 KB free with epdiy, T2). An M7 merge that runs short can turn off `CONFIG_ESP_WIFI_IRAM_OPT`, `CONFIG_ESP_WIFI_RX_IRAM_OPT` or `CONFIG_LWIP_IRAM_OPTIMIZATION`.
- Chip differences live behind ESP-IDF's own feature macros (`SOC_*`, `CONFIG_IDF_TARGET_*`) where the code is chip-specific (the console, CPU power-down in light sleep, the ADC's calibration scheme, the deep-sleep wake), and behind `board_caps.h` where it is the board's.

### 4.2 Pins

`components/board/include/board_pins.h` includes `board_pins_rlcd42.h` or `board_pins_t547.h`. The battery's ADC unit, channel and divider are pins too.

### 4.3 Capabilities

`board_caps.h` states, per board, at compile time: `BOARD_NAME`, `BOARD_HAS_ENV_SENSOR`, `BOARD_HAS_AUDIO`, `BOARD_HAS_RTC_TRIM`, `BOARD_HAS_RTC_ALARM_WAKE`, `BOARD_HAS_RTC_PRECISE_SET`, `BOARD_HAS_LPM_RATE`; `BOARD_GRAYSCALE` joins at T2. The UI's profile carries the same as `UI_CAP_*` bits, and `main` checks with `_Static_assert` that they agree. The catalogue (`/api/layouts`) publishes them with the panel's size, so the web page hides what the board can't do. Fields a board can't fill are left out of its catalogue and its menu's field lists; a preset from another board that names one draws the placeholder (upstream §5.3).

### 4.4 Display API

`display.h` stops including `st7305.h`. What changes for callers:

- `display_init(void)` and `display_init_warm(const display_state_t *)`. The RLCD's init variant comes from the settings, as today, through `display_set_variant()`, which stays and exists only where `BOARD_HAS_LPM_RATE` is set.
- `display_init_lost(void)`: a deep-sleep wake without a valid snapshot (the RLCD attaches, sets 1 Hz and wakes the panel, as `app.c` did).
- `display_state_t` keeps its role in the snapshot; its contents are the board's (§5.5).
- `display_set_fast(bool)`: HPM on the RLCD; on the T5, fast updates that don't count toward the clean refresh (§5.3). It replaces the direct `st7305_set_mode()` calls in `app_menu.c`, `app_config.c` and `app_radar.c`.
- `display_commit(bool force)`: as today on the RLCD; on the T5 it finds the changed area and picks the update (§5.3).
- `display_clean(void)`: a clean refresh on the T5, a no-op on the RLCD.
- `display_sleep()` / `display_wake()`: night sleep (§5.6).

### 4.5 RTC

`components/rtc` keeps `pcf85063.c` behind a small interface, `rtcchip_*`: read and write the time, the time-lost flag, the alarm, and with the capabilities the precise set, the error against the system clock and the trim. `timekeeping.c` and `app.c` call `rtcchip_*`; the trim code runs only with `BOARD_HAS_RTC_TRIM`. On the RLCD every call ends in the same register writes as before T0. On the T5, `rtcchip_t547.c` is the system clock itself (§8.1).

### 4.6 Sensors

The SHTC3 code is built only with `BOARD_HAS_ENV_SENSOR`. The battery reading takes its ADC unit, channel and divider from the pin header, and the calibration scheme the chip has: curve fitting on the S3, line fitting on the ESP32. The battery model and its learning (`battery_model.c`, `battery_learn.c`) are shared.

## 5. Display stack on the T5

### 5.1 epdiy, vendored

- `components/epdiy/` holds epdiy 2.1.3 (tag `2.1.3`, commit `7c30780`, LGPL-3.0-or-later) with its licence headers, built only for `REFLBO_BOARD_T547`: the ESP32's I2S output path and the `epd_board_lilygo_t5_47` board definition, both as upstream has them.
- **One patch, P5:** the conversion LUT (64 KB) goes to PSRAM when internal RAM is short, or always with the Kconfig option `EPD_LUT_IN_PSRAM`, instead of `abort()`. The ESP32's internal RAM is tight with Wi-Fi up (§2.5).
- **The LUT stays in internal RAM** (T1): in PSRAM a full-panel GL16 took 1767 ms against 1101 ms (60 % slower) and a clean update 2.64 s against 2.22 s, so `EPD_LUT_IN_PSRAM` is off. Internal RAM free with epdiy up: about 126 KB with Wi-Fi off, about 58 KB in config mode with the AP up (30 KB the lowest since boot); the LUT fit both times, and P5's fallback covers a shorter day.
- **epdiy's other allocations have no fallback** (its line queues, feed buffers, task stacks and DMA buffers, about 25 KB, assert). An update starts only with at least 40 KB of internal RAM free and an 8 KB block (`epaper_budget.h`); short of that it's skipped, logged, and the next commit tries again (T1's final review).
- `components/epdiy/PATCHES.md` records the base and every change; `THIRD_PARTY.md` credits epdiy. The fork's own files stay Apache-2.0.

### 5.2 The `epaper` component

Our code, Apache-2.0, built only for the T5:

- **The shift register's "all off" word** (`epaper_sr.c`, pure, host-tested) and the functions that use it outside epdiy: rest at boot (the register powers up random), and the deep-sleep holds (§5.4).
- **The frame blit** (`epaper_frame.c`, pure, host-tested): the 1 bpp 400×300 frame into epdiy's 4 bpp framebuffer, in the panel's middle, until T3.
- **The board's side of `display`** (`components/display/display_t547.c`, §4.4), which uses the two: epdiy is brought up only for each update. Its LUT and line queues want ~80 KB of internal RAM, which Wi-Fi needs the rest of the time. From T2 the 4 bpp framebuffer is the one epdiy's high-level API draws from (§6.1), so nothing is converted.
- From T4: the changed-area search and the update policy (§5.3), pure and host-tested.

### 5.3 Updates

- **Fast update:** only the bounding box of what changed. DU when every pixel in it is black or white before and after; GL16 (gray, no flashing) otherwise.
- **Clean update:** the panel cleared, then GC16. It flashes.
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

Until T4 every update is a clean one. DU, GL16 and GC16's times and currents on this panel are measured at T1 (`docs/power.md`): for the whole panel, a clean update 2.22 s (the clear 1.37 s), GL16 1.10 s, DU 0.55 s. If GL16 is too slow for the menu, the menu draws black and white only on the T5.

### 5.4 Panel power and deep sleep

- The rails are on only during an update: epdiy powers them up, updates and powers them down, then the fork de-initialises it.
- At boot, before anything else touches the panel, the shift register gets the "all off" word.
- Before deep sleep: the shift register's clock and data (IO18, IO23) are driven low and held with `gpio_hold_en()` and `gpio_deep_sleep_hold_en()`; the strobe (IO0) is an input on its 10k pull-up, so the latch follows a register nothing clocks.
- After a wake, the holds are released before anything else touches the panel.

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
- `gfx_fb_init_fmt()` and `gfx_fb_size_fmt()` take the format; `gfx_fb_init()` and `gfx_fb_size()` stay 1 bpp, and a zeroed `format` is 1 bpp (T2).

### 6.2 Colours and blending

- `GFX_BLACK`, `GFX_WHITE` and `GFX_INVERT` stay; `GFX_GRAY(n)`, n = 1–14, joins them.
- On a 1 bpp buffer a gray is drawn as a 4×4 ordered dither. No RLCD drawing uses grays, so nothing changes there.
- Anti-aliased glyphs and icons blend their coverage with what's beneath: towards the ink's level on the background's. `GFX_INVERT` on 4 bpp inverts the level (15 − v).
- `gfx_pixel_coverage()` blends: it moves the level towards the ink by coverage / 15, rounded half away from zero; 1 bpp inks from coverage 8 (T2).

### 6.3 Fonts and icons

- `tools/fontgen.py` and the icon generator gain a 4-bit output (coverage 0–15 a pixel) beside 1-bit. A font records its depth.
- Glyph width, height and advance widen from `uint8_t` to `uint16_t`, and offsets from `int8_t` to `int16_t`, so numerals over 255 px are possible. The RLCD's fonts are regenerated in the wider format; they draw the same.
- The T5's fonts and icons are generated at its sizes (§7.2) in 4 bits, and committed as C sources as today.
- A font's and a bitmap's `bpp` is their last member (0 reads as 1, though the generators always write it: GCC's `-Wmissing-field-initializers` wants it); 4-bit rows put the first pixel in the high nibble. T2 generated `t5_sans_26` and `t5_thermometer_40` (`assets/icons/icons_t5.txt`) for the test pattern.

### 6.4 Where the T5 uses gray

Text and icon edges; the radar's rain intensity (in place of `radar_inks()`' dither); the solar chart's fill; the map's land and borders; the status bar's separators. Each is checked for legibility in the T3 renders.

On the owner's panel (T2, 2026-10-09) the waveform's levels 0–8 are distinct and 9–15 look nearly alike, so T3 takes its grays from the dark half. Ghosting shows through dark grays after a clean update (partly old burn-in); a clean update with more clear cycles is a T4 question.

### 6.5 Screenshots, previews, goldens

- `screenshot` prints a PGM (P5, 8-bit) between `-----BEGIN RLCD PGM-----` and `-----END RLCD PGM-----` on a 4 bpp board, and the PBM as today on 1 bpp. `tools/screenshot.py` reads both and writes a PNG.
- `/api/screenshot.bmp` and `/api/preview.bmp` send a 4-bit BMP with a 16-gray palette on the T5.
- The T5's goldens are PGM files in `test/host/golden/t5/`; `render_dashboard` and `render_screen` take `--board t5`. `tools/render.py` and `tools/docs_images.py` handle both.
- The T5 keeps a 960×540 4 bpp panel frame, epdiy's layout, that `display_screenshot_fb()` returns (T2). A T5 screenshot is about 690 KB of base64, about a minute at 115200 baud (66 s measured); `/api/screenshot.bmp`'s reply buffer is 264 KB on the T5.

## 7. Dense UI at 960×540

### 7.1 The UI profile

A per-board profile holds what today is spread as constants: the panel's size, the status bar's height, the font table for each size class (`ui_widget.c:22-35`) and the fit lists, icon sizes, the fixed layouts' slot tables (`ui_layout.c`), the menu's and screens' geometry (`ui_menu_draw.c`, `ui_config.c`, `ui_screens.c`), the split limits (`ui_split.h`), the radar and flight views' sizes (`radar.h`, `ui_radar.h`, `app_flights.c`) and the catalogue's numbers (`ui_catalog.c`, `ui_split.c`, `app_web.c`).

- The RLCD's profile holds today's numbers exactly.
- Fixed paddings inside widgets become `UI_PX(n)`: the identity on the RLCD, × 1.7 rounded on the T5. This lands as one mechanical commit.
- Duplicated literals (`PANEL_W`, `PANEL_H`, the preview's `gfx_fb_size(400,300)`, the web's radar `400`) read the profile.

T0 starts the profile with the panel's size, the status bar's height, the board's name and its capabilities (`ui_profile_t`), and `UI_STATUS_H`, the split area, the catalogue and the web preview read it. The layouts, fonts, screen geometry, split limits, view sizes and `UI_PX()` move in at T3, when the T5 has numbers of its own.

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

### 8.1 Time without an RTC (DT10)

- The time is the ESP32's system clock. Its RTC timer counts through deep sleep and software resets on the slow clock; a power-off resets it.
- `rtcchip_t547.c` is that clock as an RTC chip:
  - reading returns `time()`, valid once it is past 2026-01-01 (a power-on starts it at 1970);
  - writing sets it with `settimeofday()`;
  - the precise set is the system clock's own (`BOARD_HAS_RTC_PRECISE_SET`, trivially);
  - the error against the system clock is 0;
  - there is no alarm and no trim.
- `timekeeping.c` is unchanged on top of that.
- A sync's log line "RTC off by N ms" is then the system clock's drift since the last sync.
- After a power-off the time is invalid until a sync. If Wi-Fi is set up, the app syncs at boot; otherwise it asks for the time (upstream §7).
- **The slow clock is the 8MD256 source** (`CONFIG_RTC_CLK_SRC_INT_8MD256`, T1). Over 30 one-minute deep-sleep cycles, against the Mac's clock to about 0.1 s: the 150 kHz RC, ESP-IDF's default, ran about +9000 ppm (13 minutes a day fast); 8MD256 about −1000 ppm (85 s a day slow). Its cost in deep sleep (ESP-IDF: about 5 µA) waits for the owner's meter (§8.5).
- With one NTP sync a day (DT12, upstream's default), the clock is up to about 1.5 minutes off just before a sync. A trim from sync to sync, like upstream's RTC trim (D25): the drift each sync measures, applied to the time each deep sleep adds, is a proposal to the owner, not built (§12).

### 8.2 Wake timing

- Every wake comes from the ESP32's timer or a button. The timer and the clock run on the same slow clock, so a wake lands on the clock's minute however much that clock drifts. Upstream's RTC-alarm backup logic is off on boards without the alarm (T0: `ALARM_BACKUP_S` 0).
- No wake margin is needed, unlike r2's plan for the S3 board with its separate RTC.
- **Deep sleep:** KEY (IO34) wakes through ext0, BOOT's role (IO35) through ext1 with that one pin, ALL_LOW (§2.5). A button held at sleep time is left out of the wake sources, as upstream (D16).
- **Light sleep:** both buttons wake through GPIO wakeup, and the console's UART wakes it too (§9).

### 8.3 Buttons

- KEY on IO34, BOOT's role on IO35, both active low with external 100k pull-ups, with the same gestures and timings as upstream (upstream §5.6).
- IO39 stays free. IO0 is never read (DT7).

### 8.4 Battery

GPIO36, ADC1 channel 0, 12 dB, divider 2 (the factor setting as upstream), line-fitting calibration. ADC1 doesn't conflict with Wi-Fi. The voltage-to-level model, the manual calibration and the learned curve are upstream's.

### 8.5 Power

The T5 idles in deep sleep (DT6) and runs at 240 MHz while awake, epdiy's speed. `docs/power.md` gets a T5 section:
- the deep-sleep floor;
- each refresh mode's charge;
- a minute update's total, a sync's;
- the average for upstream's default settings;
- the slow clock's drift.

Upstream's N2 goal (below 2 mA on average) is the yardstick.

T1 (2026-10-07/08): a deep-sleep minute wakes for about 2.2 s, the clean update's length. On USB the owner's meter reads the hardware's floor, not the sleep current (§2.5 gotcha 20): about 107 mA awake at 240 MHz, 67 mA in light sleep, 65 mA in deep sleep. On battery (owner's multimeter in series with the LiPo, 2026-10-09): 0.3 mA in deep sleep and about 150 mA for the 2 s clean redraw each minute, about 6 mA on average at a 1-minute update; N2 (below 2 mA) needs T4's fast updates at that interval, and holds at 5 minutes and more with clean redraws (`docs/power.md`).

## 9. Settings, menu, web and console

- **Settings:** `display.clean_after` (1–240, default 60) joins `display.*`, default in `components/storage` (AGENTS.md §8). On the T5, `display.lpm_hz`, `display.contrast`, `sensors.temp_offset_c` and `sensors.hum_offset_pct` stay in the file but nothing reads or shows them.
- **Menu:** Display ▸ Clean refresh on the T5, in place of Display ▸ Refresh rate. Items for missing hardware are hidden.
- **Web:** the Display page's clean refresh and sentence (§5.3); the capabilities hide the rest (§4.3).
- **Console on the T5** (DT11):
  - UART0 at 115200 through the CH9102, line mode as upstream.
  - Console input keeps the board awake for 5 minutes. A PC can't be detected on UART, so this replaces upstream's tethered mode on this board.
  - The UART wakes light sleep; `power idle light` keeps a bench board reachable.
  - `panel status`: the updates since boot and the last one's time; from T4, fast updates since the last clean one and the last update's mode and area.
  - `panel clean`: a clean update now.
  - `panel test`: the T5's pattern (`gfx_draw_test_pattern_t5()`): the RLCD's, a 16-step gray ramp, anti-aliased text and an icon, over the whole panel until the next commit. A deep-idle board needs a reset first: the UART can't wake deep sleep.
  - `panel bench`: the clean, GL16 and DU times (T1).
  - `panel sleep|wake` as upstream.
  - `rtc get|set` on the system clock.
  - `panel mode|rate|fps|init`, `sensors`' SHTC3 part and the trim lines of `rtc get` are absent.
  - `screenshot`: PBM until T2, then PGM (§6.5).
- **Tools:** `tools/devlog.py` and `tools/screenshot.py` take `-p` for a `/dev/cu.usbserial-*` port. On such a port they open without resetting the board, changing DTR and RTS together, and `--reset` pulses RTS to reset it.

## 10. Testing and verification

### 10.1 Levels

As upstream (AGENTS.md §7), for both boards:

1. **Build:** `tools/idf.sh build` and `REFLBO_BOARD=t5 tools/idf.sh build`, with no new warnings.
2. **Host:**
   - ctest builds the UI for both profiles.
   - The RLCD's goldens must not change, in every task.
   - New tests: the shift register's "all off" word, the frame blit, the changed-area search and the update policy, the clean-refresh sentence, the system-clock RTC's validity rule, 4 bpp primitives and blending, the 4-bit font format, the RLCD's fonts in the wider format drawing the same, and the tools' port handling.
3. **Device (T5):** flash, boot log, console, screenshots compared with the goldens. The test pattern at T1, the gray ramp at T2.
4. **Owner:** the panel's look (contrast, ghosting, legibility of the dense layouts), current, and the buttons once reachable.

### 10.2 Rules at the board

- The T5 is `/dev/cu.usbserial-52D60046741`, MAC `34:ab:95:5e:5d:58` (2026-10-07). If the port name changes, the MAC decides (`esptool read_mac` resets only the board it talks to). Every `devlog`, `screenshot`, `esptool` and `flash` call names its port.
- Never the RLCD board's port (DT8).
- No T5 image may silence the console or sleep before it has been up 2 s after a reset.
- Deep-sleep tests wake by timer.
- Nothing erases flash, NVS or the storage partition without the owner's word (upstream rule 3). The factory flash is backed up before the first write.

## 11. Milestones

Each gets its own plan in `docs/plans/`, written just before it starts.

| # | Milestone | Done when |
|---|---|---|
| T0 | The seams: the Kconfig board choice, `idf.sh`'s `REFLBO_BOARD`, the pin headers, capabilities, the display, RTC and sensor seams, the UI profile with the panel's size, status bar, board and capabilities; the T5 builds with a display that draws nothing and the RLCD's profile | Both boards build clean; the RLCD's goldens byte-identical; host tests pass (done 2026-10-07) |
| T1 | Port and bring-up: the T5 as an ESP32 board (target, pins, console, wake, the ADC's scheme, the system-clock RTC, the tools); epdiy's ESP32 path with its board and patch P5, `epaper`, rails and deep sleep; every update clean; DU, GL16 and GC16, the slow clock's drift and the sleep current measured. Until T3 the app draws the RLCD's 400×300 1 bpp frame, which `epaper` places in the panel's middle | The test pattern and the clock on the panel (owner confirms); the timings, drift and current recorded (done 2026-10-07, the current waits for the owner's meter) |
| T2 | Grayscale gfx: 4 bpp, anti-aliased fonts and icons, PGM screenshots, gray BMPs | The gray ramp and anti-aliased text on the panel match their screenshots (done 2026-10-09) |
| T3 | The dense UI: the T5 profile, layouts, menu, screens, presets, the web page | The owner approves the renders; the T5's goldens committed; every layout on the panel |
| T4 | E-paper behaviour: fast and clean updates, the setting and its sentence, the previous frame across deep sleep, the radar's loop, night sleep; power | Fast updates without artifacts across wakes; the average current measured |

## 12. Risks and open items

- **The ESP32's internal RAM** with Wi-Fi, TLS, the web server and epdiy's update all at once. T1 measures it; the LUT falls back to PSRAM, and epdiy is up only for updates.
- **The slow clock's drift** without an RTC: T1 measured about −1000 ppm on 8MD256 (§8.1), up to about 1.5 minutes before a daily sync. The owner accepts a daily sync (DT12); the trim from sync to sync waits for the owner's decision.
- **Refresh currents are unknown** until the owner's meter (T1 measured the times, §5.3). They set `clean_after`'s default, the menu's mode and the radar loop's frame time.
- **A cold boot draws twice:** `display_init()` cleans the panel to its blank frame (1.6 s), then the first screen comes clean too. T4's fast updates make the second one fast.
- **Ghosting** from fast updates between clean ones; the default may need lowering after the owner sees the panel.
- **The CH9102's auto-reset:** a tool that opens the port the wrong way resets the board (§2.5).
- **Image size:** T5 fonts at 4 bits and up to 220 px; the app has about 1.5 MB free in its 4 MB slot.
- **Radar frames at 960 px** are about 2.4 times the RLCD's pixels; the frame file and decode buffers grow with them, in 4 MB of mapped PSRAM.
- **Merge conflicts** with upstream's M7 in `components/ui`, mostly from `UI_PX()`. Merging `upstream/main` into the fork after each upstream milestone keeps them small.
- **The T5-ePaper-S3** (LilyGo's newer board) would be a third board choice. Its V2.3 needs the S3 output patch from the fork's history (commit `7232440`); its V2.4 runs stock epdiy. Not planned.
- **Buttons:** the owner can reach only RESET for now; the buttons' owner check waits.

## 13. Revision history

| Rev | Date | Change |
|---|---|---|
| r1 | 2026-10-06 | First draft from the owner's answers (DT1–DT5) and the agreed design sections |
| r2 | 2026-10-07 | T0 as built: the capabilities' list (§4.3), `display_init_lost()` (§4.4), the profile's first members (§7.1, §11) |
| r3 | 2026-10-07 | The board is the ESP32 LilyGo T5-4.7 (DT9) without an RTC (DT10), with a UART console (DT11): the board (§2), the build's per-board target (§4.1), pins (§4.2), the capabilities (§4.3), the RTC (§4.5, §8.1), the ADC (§4.6), epdiy stock on the ESP32 with patch P5 (§5.1, DT4), `epaper` (§5.2), deep sleep (§5.4), wake and buttons (§8.2, §8.3, DT7), the battery (§8.4), the console and tools (§9), the rules at the board (§10.2), T1 (§11), the risks (§12) |
| r4 | 2026-10-08 | T1 as built: bring-up's findings (§2.5), the LUT and epdiy's RAM budget (§5.1), the refresh times (§5.3), the slow clock and its drift (§8.1), the current (§8.5, the owner's meter pending); DT12; the risks (§12) |
| r5 | 2026-10-09 | T2 as built: the formats and blending (§6.1–6.2), the asset formats (§6.3), the panel frame and screenshots (§6.5), `panel test` (§9), the panel's grays and ghosting (§6.4), the ESP32's minimum revision (§4.1) |
