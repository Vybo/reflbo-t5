# T0: board seams — implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the board a build-time choice and put seams between shared code and board hardware (pins, capabilities, display, RTC, sensors, wake, UI profile), so the LilyGo T5-ePaper-S3 builds beside the RLCD board while the RLCD build behaves exactly as before.

**Architecture:** A Kconfig choice `REFLBO_BOARD` (RLCD42 default, T5S3) selected by `REFLBO_BOARD=t5 tools/idf.sh`, which builds in `build-t5/` with `sdkconfig.t5`. Board facts live in `board_pins_<board>.h` and `board_caps.h` (`BOARD_HAS_*`); board-specific sources are picked by conditional `SRCS` in component CMake files (never conditional `REQUIRES`). `display.h` becomes board-neutral, `rtcchip.h` fronts the RTC chip, `ui_profile.h` gives the UI the panel size, status bar, board name and capabilities. On the T5 the display and RTC are stubs until T1.

**Tech Stack:** ESP-IDF v5.5.5 (C17), CMake/Kconfig, Unity host tests, Python unittest for `tools/idf.sh`.

**Spec:** [`docs/specs/2026-10-06-t5-board-design.md`](../specs/2026-10-06-t5-board-design.md) (r1; this plan's Task 7 brings it to r2), with the upstream spec [`docs/specs/2026-09-25-firmware-design.md`](../specs/2026-09-25-firmware-design.md) for everything it doesn't change.

## Global Constraints

- Work only in `~/reflbo-t5` (the fork). `upstream` is fetch-only; never push to it.
- ESP-IDF v5.5.5 through `tools/idf.sh` (AGENTS.md §6); target `esp32s3`.
- The RLCD build's behaviour is unchanged, and its host goldens stay byte-identical: after every task `ctest` passes and `git status --porcelain test/host/golden` prints nothing (T5 spec TR2).
- Every RLCD code path ends in the same driver calls as before the task (same `st7305_*`, `pcf85063_*`, SHTC3 and codec writes, same order).
- Both boards build with no warnings once the T5 build is expected to pass (from Task 5); ESP-IDF builds with `-Werror`.
- A component's `REQUIRES`/`PRIV_REQUIRES` never depend on Kconfig: ESP-IDF reads them before Kconfig values exist (early expansion). Only `SRCS` may.
- List sources explicitly in `SRCS` (AGENTS.md §8). ESP-IDF style: 4-space indent, `snake_case`, one `TAG` per module.
- No board is flashed in T0. Never flash the RLCD board from the fork (DT8).
- Commits: Conventional Commits, imperative, one per task, **no trailers and no AI attribution** (AGENTS.md §8; the owner's rule).
- AGENTS.md §10 (created in Task 1) is updated in the same commit as each new command or seam (AGENTS.md quick rule 6).

## Review Focus

1. **The RLCD build drifting while code moves.** Expectation: identical behaviour. Each task's review compares every moved call against the old code (Global Constraints). Task 4 also pins `display_state_t`'s size, because the RTC-RAM snapshot holds it.
2. **RLCD-only drivers ending up in the T5 image** (ST7305, SHTC3, the codec standby, PCF85063). Expectation: none of their symbols are in `build-t5/reflbo.elf`. Task 5 checks with `nm`.
3. **The two builds sharing a config.** For example, a plain `tools/idf.sh build` after a T5 build picking up `sdkconfig.t5`. Expectation: `build/` stays RLCD42 and `build-t5/` stays T5S3. Task 1 checks both generated `sdkconfig.h` files, and Task 5 checks them again.
4. **T5 flash or erase going to the wrong place.** Expectation: they still need `-p`, and use `build-t5/`. Task 1's `test_idf_sh.py` covers both.
5. **Timer-only wakes on the T5 logging "RTC alarm missed" every minute.** Expectation: on a board without the alarm wake the timer *is* the tick, so there's no warning and no 5 s backup delay. Task 5 makes the alarm backup `0` and the warnings conditional. Its review step reads every `ALARM_BACKUP_S` use.

## Not in T0

- No panel driver, epdiy, PCF8563 driver, grayscale, or T5 layouts: those are T1–T3.
- The T5 build still uses the RLCD's 400×300 geometry, through `ui_profile_t5s3`, until T3.
- The menu and web page still offer RLCD-only settings on the T5 (refresh rate, sensor offsets). Hiding them by capability is T3.

---

### Task 1: The board as a build choice

**Files:**
- Modify: `main/Kconfig.projbuild` (add the choice; RLCD-only panel init depends on it)
- Modify: `CMakeLists.txt`
- Create: `sdkconfig.defaults.t5`
- Modify: `tools/idf.sh`
- Modify: `.gitignore`
- Modify: `tools/tests/test_idf_sh.py`
- Modify: `AGENTS.md` (header note, new §10)

**Interfaces:**
- Produces: `CONFIG_REFLBO_BOARD_RLCD42` / `CONFIG_REFLBO_BOARD_T5S3` (Kconfig); the CMake cache variable `REFLBO_BOARD` (`rlcd42` | `t5`); the environment variable `REFLBO_BOARD` read by `tools/idf.sh`.

- [ ] **Step 1: Write the failing `idf.sh` tests**

In `tools/tests/test_idf_sh.py`, make `run_idf_sh` take a board and keep an inherited `REFLBO_BOARD` out, then add the tests. Replace the `run_idf_sh` method with:

```python
    def run_idf_sh(self, *args, espport=None, board=None):
        env = {k: v for k, v in os.environ.items() if k not in ("ESPPORT", "REFLBO_BOARD")}
        env["REFLBO_IDF_PATH"] = str(self.fake.root)
        if espport:
            env["ESPPORT"] = espport
        if board is not None:
            env["REFLBO_BOARD"] = board
        return subprocess.run(["bash", str(IDF_SH), *args], env=env, capture_output=True, text=True)
```

Add these methods to `PortGuardTest`:

```python
    def test_t5_builds_in_its_own_directory_with_its_own_sdkconfig(self):
        result = self.run_idf_sh("build", board="t5")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.fake.calls_made(),
                         ["args=-B build-t5 -D REFLBO_BOARD=t5 -D SDKCONFIG=sdkconfig.t5 build ESPPORT="])

    def test_rlcd42_is_the_default_board(self):
        result = self.run_idf_sh("build", board="rlcd42")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.fake.calls_made(), ["args=build ESPPORT="])

    def test_an_unknown_board_is_refused(self):
        result = self.run_idf_sh("build", board="t7")
        self.assertEqual(result.returncode, 2)
        self.assertIn("REFLBO_BOARD", result.stderr)
        self.assertEqual(self.fake.calls_made(), [])

    def test_t5_flash_still_needs_a_port(self):
        result = self.run_idf_sh("flash", board="t5")
        self.assertEqual(result.returncode, 2)
        self.assertEqual(self.fake.calls_made(), [])

    def test_t5_flash_uses_the_t5_build(self):
        result = self.run_idf_sh("-p", "/dev/cu.usbmodemTEST", "flash", board="t5")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.fake.calls_made(),
                         ["args=-B build-t5 -D REFLBO_BOARD=t5 -D SDKCONFIG=sdkconfig.t5 -p /dev/cu.usbmodemTEST flash ESPPORT="])

    def test_exec_ignores_the_board(self):
        result = self.run_idf_sh("exec", "true", board="t5")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.fake.calls_made(), [])
```

- [ ] **Step 2: Run them and see them fail**

Run: `cd ~/reflbo-t5/tools && python3 -m unittest tests.test_idf_sh -v`
Expected: `test_t5_builds_in_its_own_directory_with_its_own_sdkconfig`, `test_an_unknown_board_is_refused` and `test_t5_flash_uses_the_t5_build` FAIL; the others pass.

- [ ] **Step 3: Teach `idf.sh` the board**

In `tools/idf.sh`, insert this block right after the port guard's closing `fi` (before `IDF_PATH=...`):

```bash
# The board (T5 spec §4.1): REFLBO_BOARD=t5 builds the LilyGo T5-ePaper-S3 in build-t5/ with its own
# sdkconfig.t5; unset or rlcd42, the Waveshare RLCD-4.2 in build/. `exec` commands don't build.
board_args=()
case "${REFLBO_BOARD:-rlcd42}" in
    rlcd42) ;;
    t5) board_args=(-B build-t5 -D REFLBO_BOARD=t5 -D SDKCONFIG=sdkconfig.t5) ;;
    *)
        echo "idf.sh: REFLBO_BOARD must be rlcd42 or t5, not '${REFLBO_BOARD}'" >&2
        exit 2
        ;;
esac
```

Replace the last line `exec idf.py "$@"` with:

```bash
exec idf.py "${board_args[@]}" "$@"
```

Also extend the usage comment at the top of the file with one line after `#   tools/idf.sh -p /dev/cu.usbmodem1101 flash`:

```bash
#   REFLBO_BOARD=t5 tools/idf.sh build          # the T5-ePaper-S3, in build-t5/
```

- [ ] **Step 4: Run the tests and see them pass**

Run: `cd ~/reflbo-t5/tools && python3 -m unittest tests.test_idf_sh -v`
Expected: all tests PASS (5 old, 6 new).

- [ ] **Step 5: Add the Kconfig choice**

In `main/Kconfig.projbuild`, insert as the first entry inside `menu "reflbo"`:

```
    choice REFLBO_BOARD
        prompt "Board"
        default REFLBO_BOARD_RLCD42
        help
            The board this image is for (T5 spec §4.1). tools/idf.sh picks it from the environment:
            REFLBO_BOARD unset or rlcd42 builds into build/, t5 into build-t5/ with sdkconfig.t5.
        config REFLBO_BOARD_RLCD42
            bool "Waveshare ESP32-S3-RLCD-4.2"
        config REFLBO_BOARD_T5S3
            bool "LilyGo T5-ePaper-S3 (V2.3)"
    endchoice
```

In the same file, make the ST7305 init choice RLCD-only by adding a `depends on` line right after `prompt "Panel init sequence"`:

```
    choice REFLBO_PANEL_INIT
        prompt "Panel init sequence"
        depends on REFLBO_BOARD_RLCD42
        default REFLBO_PANEL_INIT_FACTORY
```

- [ ] **Step 6: Pick the defaults and sdkconfig per board in CMake**

Replace the top-level `CMakeLists.txt` with:

```cmake
cmake_minimum_required(VERSION 3.16)

# The board (T5 spec §4.1). tools/idf.sh passes -D REFLBO_BOARD=t5 for the LilyGo T5-ePaper-S3,
# which builds in build-t5/ with its own sdkconfig.t5 and sdkconfig.defaults.t5 on top of the
# shared defaults. Unset or rlcd42: the Waveshare RLCD-4.2, in build/ with sdkconfig.
set(SDKCONFIG_DEFAULTS "sdkconfig.defaults")
if(REFLBO_BOARD STREQUAL "t5")
    set(SDKCONFIG "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.t5")
    list(APPEND SDKCONFIG_DEFAULTS "sdkconfig.defaults.t5")
elseif(DEFINED REFLBO_BOARD AND NOT REFLBO_BOARD STREQUAL "rlcd42")
    message(FATAL_ERROR "REFLBO_BOARD must be rlcd42 or t5, not '${REFLBO_BOARD}'")
endif()
# Personal overrides (e.g. dev Wi-Fi) go in the gitignored sdkconfig.defaults.local.
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults.local")
    list(APPEND SDKCONFIG_DEFAULTS "sdkconfig.defaults.local")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(reflbo)
```

(`project.cmake` reads `SDKCONFIG` at its top, so the variable must be set before the `include`. `idf.sh` also passes `-D SDKCONFIG=sdkconfig.t5`, so idf.py's own Python side, `set-target` included, works on the same file.)

Create `sdkconfig.defaults.t5`:

```
# LilyGo T5-ePaper-S3 (T5 spec §4.1), loaded after sdkconfig.defaults by REFLBO_BOARD=t5 tools/idf.sh.
CONFIG_REFLBO_BOARD_T5S3=y

# DT6: the e-paper keeps its image without power and the 3V3 rail is an LDO, so idle in deep sleep.
CONFIG_REFLBO_IDLE_DEFAULT_DEEP=y
```

In `.gitignore`, after the line `sdkconfig.old`, add:

```
sdkconfig.t5
sdkconfig.t5.old
```

- [ ] **Step 7: Build both boards and check each kept its own config**

Run (a fresh clone has no `sdkconfig` yet, so set the target once first, AGENTS.md §6):

```bash
cd ~/reflbo-t5 && tools/idf.sh set-target esp32s3 && tools/idf.sh build 2>&1 | grep -E "warning:|error:|Project build complete"
REFLBO_BOARD=t5 tools/idf.sh build 2>&1 | grep -E "warning:|error:|Project build complete"
grep -h "define CONFIG_REFLBO_BOARD_" build/config/sdkconfig.h build-t5/config/sdkconfig.h
tools/idf.sh build 2>&1 | grep -E "warning:|error:|Project build complete"
grep -h "define CONFIG_REFLBO_BOARD_" build/config/sdkconfig.h
```

Expected: both builds end with "Project build complete". (The T5 build compiles the RLCD code unchanged at this point.) The first `grep` prints `#define CONFIG_REFLBO_BOARD_RLCD42 1` then `#define CONFIG_REFLBO_BOARD_T5S3 1`. The second RLCD build still prints `CONFIG_REFLBO_BOARD_RLCD42 1`. `ls sdkconfig sdkconfig.t5` shows both files.

If the T5 build stops with an `IDF_TARGET` mismatch, run `REFLBO_BOARD=t5 tools/idf.sh set-target esp32s3` once and build again. That renames only `sdkconfig.t5`, never `sdkconfig`, because `SDKCONFIG` is passed.

- [ ] **Step 8: Run the host tests (nothing may move)**

Run: `cd ~/reflbo-t5 && cmake -S test/host -B build-host -G Ninja && cmake --build build-host && ctest --test-dir build-host --output-on-failure 2>&1 | tail -3 && git status --porcelain test/host/golden`
Expected: `100% tests passed`; no golden listed.

- [ ] **Step 9: Start AGENTS.md's fork section**

In `AGENTS.md`, right after the `> **Scope.** ...` paragraph, add:

```markdown
> **Fork.** This is `Vybo/reflbo-t5`: reflbo on a second board, the LilyGo T5-ePaper-S3. §10 says what differs from upstream `Vybo/reflbo`.
```

Append at the end of the file:

```markdown
## 10. The T5-ePaper-S3 fork

This repository is the fork `Vybo/reflbo-t5`. It adds a second board, the LilyGo T5-ePaper-S3 (revision V2.3), keeps the RLCD board as it is, and merges back into `Vybo/reflbo` later (DT1). Upstream's own milestones are built upstream. Its design: [`docs/specs/2026-10-06-t5-board-design.md`](docs/specs/2026-10-06-t5-board-design.md) (the T5 spec), with decisions DT1, DT2, … (§1.2) and milestones T0–T4 (§11).

- **Status:** T0 (board seams) in progress: [`docs/plans/2026-10-06-t0-board-seams.md`](docs/plans/2026-10-06-t0-board-seams.md).
- **Remotes:** `origin` is `git@github.com:Vybo/reflbo-t5.git`. `upstream` is `git@github.com:Vybo/reflbo.git`, fetch only (its push URL is `DISABLED`).
- **Build:** `REFLBO_BOARD=t5 tools/idf.sh build` builds the T5 in `build-t5/` with `sdkconfig.t5`: the shared `sdkconfig.defaults`, then `sdkconfig.defaults.t5`. Without `REFLBO_BOARD` (or with `rlcd42`), the RLCD board builds in `build/` as before. Flash and erase need `-p` for either board, and `REFLBO_BOARD=t5` for the T5. `exec` commands ignore the board.
- **Seams:** none yet.
- **Rules** (T5 spec §10.2): never flash the RLCD board from the fork (DT8). Every task keeps the RLCD's goldens byte-identical. With both boards connected, name the port and check its MAC first. On the T5 the owner can reach only RESET for now. A component's `REQUIRES` never depend on the board, since ESP-IDF reads them before Kconfig; only `SRCS` do.
- **T5 gotchas:** the T5 spec §2.5 lists the board's known ones. Add new ones there and here as they turn up.
```

- [ ] **Step 10: Commit**

```bash
cd ~/reflbo-t5 && git add main/Kconfig.projbuild CMakeLists.txt sdkconfig.defaults.t5 tools/idf.sh .gitignore tools/tests/test_idf_sh.py AGENTS.md
git commit -m "build: choose the board at build time (REFLBO_BOARD, T0)"
```

---

### Task 2: Board pins and capabilities; the sensors take theirs

**Files:**
- Create: `components/board/include/board_caps.h`
- Create: `components/board/include/board_pins_rlcd42.h`
- Create: `components/board/include/board_pins_t5s3.h`
- Modify: `components/board/include/board_pins.h` (becomes a dispatcher)
- Modify: `components/board/board.c`
- Modify: `components/sensors/sensors.c`, `components/sensors/CMakeLists.txt`
- Modify: `main/app_ui.c` (`app_ui_sample`)
- Modify: `components/diag/diag_cmd_sensors.c` (`sensors_body`)
- Modify: `AGENTS.md` (§10 Seams)

**Interfaces:**
- Produces: `board_caps.h` with `BOARD_NAME` (`"rlcd42"` | `"t5s3"`), `BOARD_HAS_ENV_SENSOR`, `BOARD_HAS_AUDIO`, `BOARD_HAS_RTC_TRIM`, `BOARD_HAS_RTC_ALARM_WAKE`, `BOARD_HAS_LPM_RATE` (each `0` or `1`). From the pin headers, for both boards: `BOARD_PIN_BOOT`, `BOARD_PIN_KEY`, `BOARD_PIN_I2C_SDA`, `BOARD_PIN_I2C_SCL`, `BOARD_PIN_BAT_ADC`, `BOARD_BAT_ADC_UNIT`, `BOARD_BAT_ADC_CHANNEL`, `BOARD_BAT_DIVIDER`. `BOARD_PIN_RTC_INT` and `BOARD_PIN_PA_CTRL` exist only on the RLCD.
- `sensors_sample_env()` returns `ESP_ERR_NOT_SUPPORTED` on a board without `BOARD_HAS_ENV_SENSOR`.

- [ ] **Step 1: Create `board_caps.h`**

```c
#pragma once

#include "sdkconfig.h"

/*
 * What each board has (T5 spec §4.3), at compile time: code for hardware a board lacks is left out
 * with #if. The UI's profile states the same through its UI_CAP_* bits (ui_profile.h), and main
 * checks that the two agree.
 */
#if CONFIG_REFLBO_BOARD_T5S3
#define BOARD_NAME               "t5s3"
#define BOARD_HAS_ENV_SENSOR     0 /* no SHTC3 */
#define BOARD_HAS_AUDIO          0
#define BOARD_HAS_RTC_TRIM       0 /* PCF8563: no Offset register */
#define BOARD_HAS_RTC_ALARM_WAKE 0 /* the PCF8563's INT isn't wired: the ESP32's timer wakes the board */
#define BOARD_HAS_LPM_RATE       0 /* e-paper: no refresh rate to set */
#else
#define BOARD_NAME               "rlcd42"
#define BOARD_HAS_ENV_SENSOR     1
#define BOARD_HAS_AUDIO          1
#define BOARD_HAS_RTC_TRIM       1
#define BOARD_HAS_RTC_ALARM_WAKE 1
#define BOARD_HAS_LPM_RATE       1
#endif
```

- [ ] **Step 2: Split the pin map per board**

Move the current pin map into a new file:

```bash
cd ~/reflbo-t5 && git mv components/board/include/board_pins.h components/board/include/board_pins_rlcd42.h
```

Append to `components/board/include/board_pins_rlcd42.h`:

```c

/* Battery: VBAT through a 200k/100k divider (spec §8). The values are esp_adc's, expanded where
 * the ADC is set up. */
#define BOARD_BAT_ADC_UNIT    ADC_UNIT_1
#define BOARD_BAT_ADC_CHANNEL ADC_CHANNEL_3 /* GPIO4 */
#define BOARD_BAT_DIVIDER     3
```

Create `components/board/include/board_pins_t5s3.h`:

```c
#pragma once

/* LilyGo T5-ePaper-S3, revision V2.3 (T5 spec §2.2: the vendor's README pin table and schematic). */

#define BOARD_PIN_BOOT    0  /* BOOT button: active low, external 10k pull-up; also the 74HCT4094's strobe */
#define BOARD_PIN_KEY     21 /* KEY button: active low, external 10k pull-up */
#define BOARD_PIN_I2C_SDA 18 /* the PCF8563; 10k pull-ups */
#define BOARD_PIN_I2C_SCL 17
#define BOARD_PIN_BAT_ADC 14 /* ADC2_CH3 = VBAT x 1/2 */

/* The panel (T1): 8-bit parallel data and its clocks; the 74HCT4094 holds its control lines and rails. */
#define BOARD_PIN_EPD_D0  8
#define BOARD_PIN_EPD_D1  1
#define BOARD_PIN_EPD_D2  2
#define BOARD_PIN_EPD_D3  3
#define BOARD_PIN_EPD_D4  4
#define BOARD_PIN_EPD_D5  5
#define BOARD_PIN_EPD_D6  6
#define BOARD_PIN_EPD_D7  7
#define BOARD_PIN_EPD_CKV 38
#define BOARD_PIN_EPD_STH 40
#define BOARD_PIN_EPD_CKH 41
#define BOARD_PIN_SR_DATA 13
#define BOARD_PIN_SR_CLK  12
#define BOARD_PIN_SR_STR  BOARD_PIN_BOOT

/* Battery: VBAT through a 100k/100k divider. ADC2 is shared with Wi-Fi (T5 spec §2.5). */
#define BOARD_BAT_ADC_UNIT    ADC_UNIT_2
#define BOARD_BAT_ADC_CHANNEL ADC_CHANNEL_3 /* GPIO14 */
#define BOARD_BAT_DIVIDER     2
```

Create the new `components/board/include/board_pins.h`:

```c
#pragma once

#include "sdkconfig.h"

/* The board's pins (T5 spec §4.2): one header per board. */
#if CONFIG_REFLBO_BOARD_T5S3
#include "board_pins_t5s3.h"
#else
#include "board_pins_rlcd42.h"
#endif
```

- [ ] **Step 3: Leave the audio and RLCD devices out of `board.c` on the T5**

In `components/board/board.c`:

1. Add `#include "board_caps.h"` after `#include "board.h"`.
2. Wrap the codec tables and `write_codec()` in `#if BOARD_HAS_AUDIO` ... `#endif`. That is everything from the comment `/* Standby writes from esp_codec_dev ...` to the end of `write_codec()`.
3. Replace the `k_devices` initialiser in `report_devices()` with:

```c
    } k_devices[] = {
#if CONFIG_REFLBO_BOARD_T5S3
        { 0x51, "PCF8563" },
#else
        { 0x18, "ES8311" }, { 0x40, "ES7210" }, { 0x51, "PCF85063" }, { 0x70, "SHTC3" },
#endif
    };
```

4. In `board_init()`, wrap the three `PA_CTRL` lines (`gpio_config_t pa = ...` through `gpio_set_level(BOARD_PIN_PA_CTRL, 0);`) in `#if BOARD_HAS_AUDIO` ... `#endif`.
5. Wrap the codec standby block inside `if (cold)` (from `err = write_codec(0x18, ...` to the closing `}` of its `else`) in `#if BOARD_HAS_AUDIO` ... `#endif`, leaving `report_devices();` outside it.

- [ ] **Step 4: Sensors take the board's battery channel; the SHTC3 is optional**

In `components/sensors/CMakeLists.txt`, add `board` to `PRIV_REQUIRES`:

```cmake
                       PRIV_REQUIRES board esp_adc util)
```

In `components/sensors/sensors.c`:

1. Add `#include "board_caps.h"` and `#include "board_pins.h"` before `#include "esp_adc/adc_cali.h"`.
2. Delete the two lines `#define BAT_CHANNEL ...` and `#define BAT_DIVIDER ...`.
3. Wrap `s_shtc3`'s declaration and the functions `shtc3_command`, `shtc3_wake`, `shtc3_check_id` and `read_env_once` in `#if BOARD_HAS_ENV_SENSOR` ... `#endif`. They form two blocks: the declaration and the first three functions, then `read_env_once`.
4. Replace `adc_init()` with:

```c
static esp_err_t adc_init(void)
{
    adc_oneshot_unit_init_cfg_t unit = { .unit_id = BOARD_BAT_ADC_UNIT };
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&unit, &s_adc), TAG, "ADC unit");
    adc_oneshot_chan_cfg_t chan = { .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT };
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(s_adc, BOARD_BAT_ADC_CHANNEL, &chan), TAG, "ADC channel");
    adc_cali_curve_fitting_config_t cali = {
        .unit_id = BOARD_BAT_ADC_UNIT,
        .chan = BOARD_BAT_ADC_CHANNEL,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali, &s_cali) != ESP_OK) {
        ESP_LOGW(TAG, "no ADC calibration in eFuse; battery readings are approximate");
        s_cali = NULL;
    }
    return ESP_OK;
}
```

5. Replace `sensors_init()` with:

```c
esp_err_t sensors_init(i2c_master_bus_handle_t bus, bool cold)
{
#if BOARD_HAS_ENV_SENSOR
    if (s_shtc3 == NULL) {
        i2c_device_config_t cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = SHTC3_ADDR,
            .scl_speed_hz = 400000,
        };
        ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &cfg, &s_shtc3), TAG, "add SHTC3");
    }
#else
    (void)bus;
#endif
    if (cold) {
        battery_gauge_init(&s_state.gauge);
#if BOARD_HAS_ENV_SENSOR
        esp_err_t err = shtc3_check_id();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "SHTC3 not ready: %s", esp_err_to_name(err));
        }
#endif
    }
    return adc_init();
}
```

6. Replace `sensors_sample_env()` with:

```c
esp_err_t sensors_sample_env(time_t now)
{
#if BOARD_HAS_ENV_SENSOR
    int t, h;
    esp_err_t err = read_env_once(&t, &h);
    if (err != ESP_OK) {
        err = read_env_once(&t, &h); /* spec §16: retry once */
    }
    ESP_RETURN_ON_ERROR(err, TAG, "SHTC3");
    s_state.env = (sensors_env_t){
        .valid = true,
        .temp_c100 = t + s_temp_offset_c100,
        .hum_pct100 = shtc3_offset_humidity(h, s_hum_offset_pct100),
        .time = now,
    };
    return ESP_OK;
#else
    (void)now;
    return ESP_ERR_NOT_SUPPORTED; /* no climate sensor on this board (T5 spec §2.4) */
#endif
}
```

7. In `sensors_sample_battery()`, replace the three uses of `BAT_CHANNEL` with `BOARD_BAT_ADC_CHANNEL`, and `BAT_DIVIDER` with `BOARD_BAT_DIVIDER`.

- [ ] **Step 5: Callers stay quiet about a sensor the board doesn't have**

In `main/app_ui.c` `app_ui_sample()`, replace

```c
    } else {
        ESP_LOGW(TAG, "SHTC3: %s; keeping the last reading", esp_err_to_name(err));
    }
```

with

```c
    } else if (err != ESP_ERR_NOT_SUPPORTED) { /* not supported: a board without the SHTC3 (T5 spec §2.4) */
        ESP_LOGW(TAG, "SHTC3: %s; keeping the last reading", esp_err_to_name(err));
    }
```

In `components/diag/diag_cmd_sensors.c` `sensors_body()`, replace

```c
    esp_err_t err = sensors_sample_env(time(NULL));
    if (err != ESP_OK) {
```

with

```c
    esp_err_t err = sensors_sample_env(time(NULL));
    if (err == ESP_ERR_NOT_SUPPORTED) {
        printf("sensors: this board has no temperature sensor\n");
        return 1;
    }
    if (err != ESP_OK) {
```

- [ ] **Step 6: Build the RLCD and run the host tests**

Run: `cd ~/reflbo-t5 && tools/idf.sh build 2>&1 | grep -E "warning:|error:|Project build complete" && cmake --build build-host && ctest --test-dir build-host --output-on-failure 2>&1 | tail -1 && git status --porcelain test/host/golden`
Expected: "Project build complete" and no `warning:` lines, `100% tests passed`, no golden listed. The T5 build is **not** expected to pass yet: `st7305.c` still compiles for it and its LCD pins don't exist on the T5 (fixed in Task 4).

- [ ] **Step 7: Review the RLCD paths**

Confirm by reading the diff (`git diff`):
- On the RLCD, `board_init()` still configures PA_CTRL, creates the I²C bus, installs the ISR service, then on a cold boot reports the same four devices and writes both codec tables.
- `sensors_init()`, `sensors_sample_env()` and `sensors_sample_battery()` run the same calls with ADC unit 1, channel 3 and divider 3.

- [ ] **Step 8: Note the seam in AGENTS.md**

In AGENTS.md §10, replace `- **Seams:** none yet.` with:

```markdown
- **Seams:** `board_pins.h` includes `board_pins_rlcd42.h` or `board_pins_t5s3.h`. The battery's ADC unit, channel and divider are pins too. `board_caps.h` says what each board has (`BOARD_NAME`, `BOARD_HAS_*`). Code for missing hardware is left out with `#if`: on the T5, `sensors_sample_env()` returns `ESP_ERR_NOT_SUPPORTED`, and the codec standby and PA_CTRL are absent.
```

- [ ] **Step 9: Commit**

```bash
cd ~/reflbo-t5 && git add components/board components/sensors main/app_ui.c components/diag/diag_cmd_sensors.c AGENTS.md
git commit -m "feat(board): pin maps and capabilities per board (T0)"
```

---

### Task 3: The RTC behind `rtcchip`

**Files:**
- Create: `components/rtc/include/rtcchip.h`
- Create: `components/rtc/rtcchip_rlcd42.c`
- Create: `components/rtc/rtcchip_t5s3.c`
- Modify: `components/rtc/CMakeLists.txt`
- Modify: `components/timekeeping/timekeeping.c`
- Modify: `main/app.c` (`pcf85063_*` calls, the trim start)
- Modify: `main/app_web.c` (the Info page's trim)
- Modify: `components/diag/diag_cmd_sensors.c` (`rtc get`)
- Modify: `AGENTS.md` (§10 Seams)

**Interfaces:**
- Consumes: `board_caps.h` (Task 2).
- Produces:
  - `esp_err_t rtcchip_init(i2c_master_bus_handle_t bus)`
  - `esp_err_t rtcchip_read(time_t *utc, bool *valid)`
  - `esp_err_t rtcchip_write(time_t utc)`
  - `esp_err_t rtcchip_set_alarm(time_t wake)`: `ESP_OK` and no effect without `BOARD_HAS_RTC_ALARM_WAKE`
  - with `BOARD_HAS_RTC_TRIM` only: `rtcchip_write_precise(int64_t *set_at_ms)`, `rtcchip_error_ms(int64_t *error_ms)`, `rtcchip_set_offset(int steps)`
  - `timekeeping_trim_start()` and `timekeeping_trim()` stay declared, but are defined only with `BOARD_HAS_RTC_TRIM`.

- [ ] **Step 1: Create `rtcchip.h`**

```c
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "board_caps.h"
#include "driver/i2c_master.h"
#include "esp_err.h"

/*
 * The board's RTC chip (T5 spec §4.5): the PCF85063A on the RLCD board (pcf85063.h), the PCF8563 on
 * the T5. Stores UTC. What a chip does beyond keeping time is in board_caps.h:
 * BOARD_HAS_RTC_ALARM_WAKE and BOARD_HAS_RTC_TRIM. Call from the app task only.
 */

/* Configures the chip and keeps its time. */
esp_err_t rtcchip_init(i2c_master_bus_handle_t bus);
/* `valid` is false while the chip says its oscillator stopped (the time was lost). */
esp_err_t rtcchip_read(time_t *utc, bool *valid);
/* Sets the time to the second and clears the oscillator flag. */
esp_err_t rtcchip_write(time_t utc);
/* Arms the alarm for `wake` (a whole minute) and clears a pending one. Without
 * BOARD_HAS_RTC_ALARM_WAKE there is nothing to arm: ESP_OK, and the ESP32's timer wakes the board. */
esp_err_t rtcchip_set_alarm(time_t wake);

#if BOARD_HAS_RTC_TRIM
/* The PCF85063's precise set, its error against the system clock and its Offset register
 * (spec §7, D25; pcf85063.h). */
esp_err_t rtcchip_write_precise(int64_t *set_at_ms);
esp_err_t rtcchip_error_ms(int64_t *error_ms);
esp_err_t rtcchip_set_offset(int steps);
#endif
```

- [ ] **Step 2: The two implementations**

Create `components/rtc/rtcchip_rlcd42.c`:

```c
#include "rtcchip.h"

#include "pcf85063.h"

/* The RLCD board's PCF85063A (spec §7): rtcchip_* passes straight through. */

esp_err_t rtcchip_init(i2c_master_bus_handle_t bus)
{
    return pcf85063_init(bus);
}

esp_err_t rtcchip_read(time_t *utc, bool *valid)
{
    return pcf85063_read(utc, valid);
}

esp_err_t rtcchip_write(time_t utc)
{
    return pcf85063_write(utc);
}

esp_err_t rtcchip_set_alarm(time_t wake)
{
    return pcf85063_set_alarm(wake);
}

esp_err_t rtcchip_write_precise(int64_t *set_at_ms)
{
    return pcf85063_write_precise(set_at_ms);
}

esp_err_t rtcchip_error_ms(int64_t *error_ms)
{
    return pcf85063_error_ms(error_ms);
}

esp_err_t rtcchip_set_offset(int steps)
{
    return pcf85063_set_offset(steps);
}
```

Create `components/rtc/rtcchip_t5s3.c`:

```c
#include "rtcchip.h"

/* T5-ePaper-S3, T0: no PCF8563 driver yet; T1 brings it (T5 spec §8.1). Reads fail, so the clock stays
 * invalid; there's no alarm to arm, as the chip's INT isn't wired. */

esp_err_t rtcchip_init(i2c_master_bus_handle_t bus)
{
    (void)bus;
    return ESP_OK;
}

esp_err_t rtcchip_read(time_t *utc, bool *valid)
{
    *utc = 0;
    *valid = false;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t rtcchip_write(time_t utc)
{
    (void)utc;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t rtcchip_set_alarm(time_t wake)
{
    (void)wake;
    return ESP_OK; /* nothing to arm: the ESP32's timer wakes the board (T5 spec §8.2) */
}
```

Replace `components/rtc/CMakeLists.txt` with:

```cmake
# The board's RTC chip (spec §7, T5 spec §4.5): the PCF85063A on the RLCD board, the PCF8563 on the
# T5 (T1). pcf85063_regs.c is pure C and also built on the host. REQUIRES stay the same for both
# boards: ESP-IDF reads them before Kconfig.
if(CONFIG_REFLBO_BOARD_T5S3)
    set(srcs "rtcchip_t5s3.c")
else()
    set(srcs "pcf85063.c" "pcf85063_regs.c" "rtcchip_rlcd42.c")
endif()
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       REQUIRES board esp_driver_i2c
                       PRIV_REQUIRES util)
```

- [ ] **Step 3: `timekeeping.c` through `rtcchip`, the trim only where the chip has one**

Replace `components/timekeeping/timekeeping.c` with:

```c
#include "timekeeping.h"

#include <stdlib.h>
#include <sys/time.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "rtcchip.h"
#include "timekeeping_sync.h"

static const char *TAG = "timekeeping";

static bool s_valid;

esp_err_t timekeeping_init(const char *tz_posix)
{
    ESP_RETURN_ON_FALSE(setenv("TZ", tz_posix, 1) == 0, ESP_ERR_NO_MEM, TAG, "TZ");
    tzset();
    return ESP_OK;
}

esp_err_t timekeeping_load_from_rtc(bool at_edge)
{
    time_t utc;
    bool valid;
    ESP_RETURN_ON_ERROR(rtcchip_read(&utc, &valid), TAG, "RTC read");
    struct timeval now;
    gettimeofday(&now, NULL);
    if (timekeeping_rtc_resync(now.tv_sec, utc, at_edge)) {
        struct timeval tv = { .tv_sec = utc };
        settimeofday(&tv, NULL);
    }
    s_valid = valid;
    return ESP_OK;
}

bool timekeeping_valid(void)
{
    return s_valid;
}

static int64_t clock_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

#if BOARD_HAS_RTC_TRIM

#define NVS_KEY_TRIM "rtc_trim"

static rtc_trim_t s_trim;
static bool s_trim_up;

static void trim_save(void)
{
    uint8_t rec[TRIM_RECORD_LEN];
    timekeeping_trim_pack(&s_trim, rec);
    nvs_handle_t nvs;
    if (nvs_open("sys", NVS_READWRITE, &nvs) == ESP_OK) {
        if (nvs_set_blob(nvs, NVS_KEY_TRIM, rec, sizeof(rec)) == ESP_OK) {
            nvs_commit(nvs);
        }
        nvs_close(nvs);
    }
}

/* The kept trim, once a boot: a deep-sleep wake keeps the chip's offset but not this copy. A routine
 * wake has no NVS yet (gotcha 11), so it tries again next time rather than take the defaults. */
static void trim_load(void)
{
    if (s_trim_up) {
        return;
    }
    timekeeping_trim_init(&s_trim);
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("sys", NVS_READONLY, &nvs);
    if (err == ESP_OK) {
        uint8_t rec[TRIM_RECORD_LEN];
        size_t len = sizeof(rec);
        if (nvs_get_blob(nvs, NVS_KEY_TRIM, rec, &len) == ESP_OK && !timekeeping_trim_unpack(&s_trim, rec, len)) {
            timekeeping_trim_init(&s_trim);
        }
        nvs_close(nvs);
    }
    s_trim_up = err != ESP_ERR_NVS_NOT_INITIALIZED;
}

esp_err_t timekeeping_trim_start(void)
{
    trim_load();
    ESP_LOGI(TAG, "RTC trim %d steps", s_trim.offset);
    return rtcchip_set_offset(s_trim.offset); /* the chip loses it with its power (D9) */
}

const rtc_trim_t *timekeeping_trim(void)
{
    trim_load();
    return &s_trim;
}

esp_err_t timekeeping_apply_true_time(int64_t true_utc_us, int64_t mono_us, int64_t *moved_ms)
{
    trim_load();
    int64_t error_ms = 0; /* the RTC against the system clock, while the RTC still keeps its time */
    bool measured = s_valid && rtcchip_error_ms(&error_ms) == ESP_OK;
    int64_t true_now = true_utc_us + (esp_timer_get_time() - mono_us);
    int64_t ahead_ms = (clock_us() - true_now) / 1000; /* the system clock against the truth */
    struct timeval tv = { .tv_sec = (time_t)(true_now / 1000000), .tv_usec = (suseconds_t)(true_now % 1000000) };
    settimeofday(&tv, NULL);
    *moved_ms = -ahead_ms;
    if (measured && timekeeping_trim_measure(&s_trim, true_now / 1000, error_ms + ahead_ms)) {
        ESP_LOGI(TAG, "RTC drift %ld ppb: trim now %d steps", (long)s_trim.drift_ppb, s_trim.offset);
        ESP_RETURN_ON_ERROR(rtcchip_set_offset(s_trim.offset), TAG, "RTC offset");
    } else if (measured) {
        ESP_LOGI(TAG, "RTC off by %lld ms", (long long)(error_ms + ahead_ms));
    }
    int64_t set_at_ms = 0;
    ESP_RETURN_ON_ERROR(rtcchip_write_precise(&set_at_ms), TAG, "RTC write");
    timekeeping_trim_set(&s_trim, set_at_ms);
    trim_save();
    s_valid = true;
    return ESP_OK;
}

esp_err_t timekeeping_set_utc(time_t utc)
{
    ESP_RETURN_ON_ERROR(rtcchip_write(utc), TAG, "RTC write");
    trim_load(); /* also after a deep-sleep wake: the measurement must not span this set */
    if (s_trim.set_at_ms != 0) {
        timekeeping_trim_forget(&s_trim);
        trim_save();
    }
    struct timeval tv = { .tv_sec = utc };
    settimeofday(&tv, NULL);
    s_valid = true;
    ESP_LOGI(TAG, "time set to %lld", (long long)utc);
    return ESP_OK;
}

#else /* an RTC without trim or a precise set (T5 spec §2.4, §8.1) */

esp_err_t timekeeping_apply_true_time(int64_t true_utc_us, int64_t mono_us, int64_t *moved_ms)
{
    int64_t true_now = true_utc_us + (esp_timer_get_time() - mono_us);
    int64_t ahead_ms = (clock_us() - true_now) / 1000; /* the system clock against the truth */
    struct timeval tv = { .tv_sec = (time_t)(true_now / 1000000), .tv_usec = (suseconds_t)(true_now % 1000000) };
    settimeofday(&tv, NULL);
    *moved_ms = -ahead_ms;
    ESP_RETURN_ON_ERROR(rtcchip_write(tv.tv_sec), TAG, "RTC write"); /* to the second */
    s_valid = true;
    return ESP_OK;
}

esp_err_t timekeeping_set_utc(time_t utc)
{
    ESP_RETURN_ON_ERROR(rtcchip_write(utc), TAG, "RTC write");
    struct timeval tv = { .tv_sec = utc };
    settimeofday(&tv, NULL);
    s_valid = true;
    ESP_LOGI(TAG, "time set to %lld", (long long)utc);
    return ESP_OK;
}

#endif
```

In `components/timekeeping/include/timekeeping.h`, extend the trim comment to say where it exists. Replace

```c
/* The RTC trim (spec §7, D25), kept in NVS `sys/rtc_trim`. Loads it and writes the offset to the RTC;
 * call once NVS is up (a cold boot, or the first wake that stays awake). */
```

with

```c
/* The RTC trim (spec §7, D25), kept in NVS `sys/rtc_trim`. Loads it and writes the offset to the RTC;
 * call once NVS is up (a cold boot, or the first wake that stays awake). Defined only on boards with
 * BOARD_HAS_RTC_TRIM (board_caps.h). */
```

- [ ] **Step 4: Callers**

In `main/app.c`:

1. Replace `#include "pcf85063.h"` with `#include "rtcchip.h"`.
2. Replace `pcf85063_set_alarm(` with `rtcchip_set_alarm(` (two places: `schedule_next()` and `enter_night_sleep()`).
3. Replace `ESP_RETURN_ON_ERROR(pcf85063_init(board_i2c()), TAG, "RTC");` with `ESP_RETURN_ON_ERROR(rtcchip_init(board_i2c()), TAG, "RTC");`.
4. Wrap the cold-boot trim start in `boot()`:

```c
#if BOARD_HAS_RTC_TRIM
    if (wake == POWER_WAKE_COLD) {
        err = timekeeping_trim_start(); /* the RTC lost its trim with its power, or a reset kept it: write it */
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "RTC trim: %s", esp_err_to_name(err));
        }
    }
#endif
```

In `main/app_web.c`, add `#include "board_caps.h"` before `#include "cJSON.h"`, and wrap the Info page's trim lines (from `const rtc_trim_t *trim = timekeeping_trim();` to the closing `}` of the `drift_ppb` check) in `#if BOARD_HAS_RTC_TRIM` ... `#endif`. The page already shows a missing `rtc` object as no trim (`web/app.js:355`).

In `components/diag/diag_cmd_sensors.c`, replace `#include "pcf85063.h"` with `#include "rtcchip.h"`, and `pcf85063_read(&utc, &valid)` with `rtcchip_read(&utc, &valid)`. Wrap the trim lines in `rtc_body` (from `const rtc_trim_t *trim = timekeeping_trim();` to the closing `}` of the `drift_ppb` check) in `#if BOARD_HAS_RTC_TRIM` ... `#endif`, keeping the final `printf("\n");` outside the `#if`. Then on a board without trim, the `trim` line's text and the newline it ended with are both gone, except for that last `printf("\n")`.

- [ ] **Step 5: Build the RLCD and run the host tests**

Run: `cd ~/reflbo-t5 && tools/idf.sh build 2>&1 | grep -E "warning:|error:|Project build complete" && cmake --build build-host && ctest --test-dir build-host --output-on-failure 2>&1 | tail -1 && git status --porcelain test/host/golden`
Expected: "Project build complete" and no `warning:` lines, `100% tests passed`, no golden listed (`test_pcf85063_regs` still runs: the host build compiles `pcf85063_regs.c` itself).

- [ ] **Step 6: Review the RLCD paths**

In `git diff`, check every `rtcchip_*` call on the RLCD against the `pcf85063_*` call it replaced, in the same place and order. `timekeeping.c`'s RLCD half must be the old file with only the calls renamed and `clock_us()` moved up.

- [ ] **Step 7: Note the seam in AGENTS.md**

In AGENTS.md §10 Seams, append this sentence:

```markdown
`rtcchip.h` fronts the RTC chip (`rtcchip_rlcd42.c` over the PCF85063; on the T5 a stub until T1). The trim and the precise set exist only with `BOARD_HAS_RTC_TRIM`.
```

- [ ] **Step 8: Commit**

```bash
cd ~/reflbo-t5 && git add components/rtc components/timekeeping main/app.c main/app_web.c components/diag/diag_cmd_sensors.c AGENTS.md
git commit -m "refactor(rtc): reach the RTC chip through rtcchip (T0)"
```

---

### Task 4: A board-neutral display API

**Files:**
- Modify: `components/display/include/display.h`
- Create: `components/display/include/display_board.h`
- Rename: `components/display/display.c` → `components/display/display_rlcd42.c` (then modify)
- Create: `components/display/display_t5s3.c`
- Modify: `components/display/CMakeLists.txt`
- Modify: `components/st7305/CMakeLists.txt`
- Modify: `main/app.c`, `main/app_menu.c`, `main/app_config.c`, `main/app_radar.c`, `main/app_ui.c`
- Replace: `components/diag/diag_cmd_display.c`
- Modify: `AGENTS.md` (§10 Seams)

**Interfaces:**
- Consumes: `board_caps.h` (`BOARD_HAS_LPM_RATE`).
- Produces in `display.h`:
  - `esp_err_t display_init(void)`
  - `esp_err_t display_init_warm(const display_state_t *state)`
  - `esp_err_t display_init_lost(void)`
  - `void display_export(display_state_t *out)`
  - `esp_err_t display_prepare_deep_sleep(void)`, `void display_cancel_deep_sleep(void)`
  - `gfx_fb_t *display_fb(void)`
  - `esp_err_t display_sleep(void)`, `esp_err_t display_wake(void)`, `bool display_asleep(void)`
  - `esp_err_t display_commit(bool force)`
  - `esp_err_t display_set_fast(bool fast)`
  - `esp_err_t display_clean(void)`
- On the RLCD only, from `display_board.h`: `esp_err_t display_set_variant(st7305_variant_t variant)`.

- [ ] **Step 1: The board's display state**

Create `components/display/include/display_board.h`:

```c
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "sdkconfig.h"

/* What each board's display keeps through deep sleep (the app's RTC-RAM snapshot), and the calls only
 * one board has (T5 spec §4.4). */

#if CONFIG_REFLBO_BOARD_T5S3

typedef struct {
    uint32_t last_crc; /* CRC of the frame last committed */
    bool pushed;
} display_state_t;

#else

#include "st7305.h"

typedef struct {
    st7305_variant_t variant;
    st7305_mode_t mode;
    st7305_lpm_rate_t lpm_rate;
    uint32_t last_crc; /* CRC of the frame the panel shows */
    bool pushed;
    bool asleep; /* sleep-in: night sleep (spec §9.1) */
} display_state_t;
_Static_assert(sizeof(display_state_t) == 20, "the RLCD's snapshot layout is unchanged (T5 spec TR2)");

/* Re-initialises the panel with another init sequence and pushes the current frame. */
esp_err_t display_set_variant(st7305_variant_t variant);

#endif
```

Check the asserted size before relying on it. Before this task, `display_state_t` is three 4-byte enums, a `uint32_t` and two `bool`s, which is 20 bytes on Xtensa. If the build reports a different size, the old struct had that size: put the old size in the assert, and never change the struct.

- [ ] **Step 2: Rewrite `display.h`**

Replace `components/display/include/display.h` with:

```c
#pragma once

#include <stdbool.h>

#include "display_board.h"
#include "esp_err.h"
#include "gfx.h"

/* Display service (spec §4.1, T5 spec §4.4): owns the canonical framebuffer (in PSRAM) and puts it on
 * the board's panel. The API is the same on every board; display_board.h holds what isn't.
 *
 * Not thread-safe: the display and its panel belong to the app task (spec §3.2). Console commands
 * that draw or push run there through the diag executor. */

esp_err_t display_init(void); /* cold start: white frame, then idle */
/* Deep-sleep wake: the panel still shows the last frame; attach without reset or clear. */
esp_err_t display_init_warm(const display_state_t *state);
/* Deep-sleep wake without a valid snapshot: the panel kept running and may have slept for the night,
 * so attach without a reset and wake it. A failed wake is logged, not returned. */
esp_err_t display_init_lost(void);
void display_export(display_state_t *out);
/* Holds the panel's pins for deep sleep. The last display call before sleeping. */
esp_err_t display_prepare_deep_sleep(void);
void display_cancel_deep_sleep(void); /* after display_prepare_deep_sleep() failed */
gfx_fb_t *display_fb(void); /* NULL until display_init has allocated the framebuffer */
/* Night sleep (spec §9.1). On the RLCD the panel stops scanning and its image fades; waking pushes the
 * frame again. */
esp_err_t display_sleep(void);
esp_err_t display_wake(void);
bool display_asleep(void);
/* Puts the framebuffer on the panel if it changed since the last commit (CRC32), or always when force
 * is set. */
esp_err_t display_commit(bool force);
/* Fast updates for interaction (the menu, config mode, the radar's loop): HPM on the RLCD; on the T5,
 * fast updates left out of the clean refresh's count (T5 spec §5.3). */
esp_err_t display_set_fast(bool fast);
/* A clean refresh (T5 spec §5.3): nothing to do on the RLCD. */
esp_err_t display_clean(void);
```

- [ ] **Step 3: The RLCD implementation**

```bash
cd ~/reflbo-t5 && git mv components/display/display.c components/display/display_rlcd42.c
```

In `components/display/display_rlcd42.c`:

1. After `static const char *TAG = "display";` add:

```c

#if CONFIG_REFLBO_PANEL_INIT_XIAOZHI
#define PANEL_VARIANT ST7305_VARIANT_XIAOZHI
#else
#define PANEL_VARIANT ST7305_VARIANT_FACTORY
#endif
```

2. Replace the function header `esp_err_t display_init(st7305_variant_t variant)` and its `st7305_init(variant)` call:

```c
esp_err_t display_init(void)
{
    ESP_RETURN_ON_ERROR(alloc_fb(), TAG, "framebuffer");
    ESP_RETURN_ON_ERROR(st7305_init(PANEL_VARIANT), TAG, "panel init");
```

The rest of the function stays as it is.

3. Append:

```c

esp_err_t display_init_lost(void)
{
    display_state_t fallback = { .variant = PANEL_VARIANT, .mode = ST7305_MODE_LPM, .lpm_rate = ST7305_LPM_1HZ,
                                 .asleep = true };
    ESP_RETURN_ON_ERROR(display_init_warm(&fallback), TAG, "panel attach");
    st7305_set_lpm_rate(ST7305_LPM_1HZ); /* assumed, so send it; safe without a reset */
    esp_err_t err = display_wake();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "panel wake: %s", esp_err_to_name(err));
    }
    return ESP_OK;
}

esp_err_t display_set_fast(bool fast)
{
    return st7305_set_mode(fast ? ST7305_MODE_HPM : ST7305_MODE_LPM);
}

esp_err_t display_clean(void)
{
    return ESP_OK; /* the RLCD shows each push as it is */
}
```

- [ ] **Step 4: The T5 stub**

Create `components/display/display_t5s3.c`:

```c
#include "display.h"

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "util_crc32.h"

/* T5-ePaper-S3 (T5 spec §5), T0: the API only. The framebuffer has the RLCD's 400×300 at 1 bpp
 * until T3 (T5 spec §11), and nothing reaches the panel until T1 brings epdiy and `epaper`. */

#define FB_W 400
#define FB_H 300

static const char *TAG = "display";

static gfx_fb_t s_fb; /* s_fb.buf stays NULL until display_init */
static uint32_t s_last_crc;
static bool s_pushed;

static esp_err_t alloc_fb(void)
{
    ESP_RETURN_ON_FALSE(s_fb.buf == NULL, ESP_ERR_INVALID_STATE, TAG, "already initialised");
    uint8_t *buf = heap_caps_calloc(1, gfx_fb_size(FB_W, FB_H), MALLOC_CAP_SPIRAM);
    ESP_RETURN_ON_FALSE(buf != NULL, ESP_ERR_NO_MEM, TAG, "framebuffer");
    gfx_fb_init(&s_fb, buf, FB_W, FB_H);
    return ESP_OK;
}

esp_err_t display_init(void)
{
    ESP_RETURN_ON_ERROR(alloc_fb(), TAG, "framebuffer");
    ESP_LOGW(TAG, "no panel driver yet (T1)");
    return ESP_OK;
}

esp_err_t display_init_warm(const display_state_t *state)
{
    ESP_RETURN_ON_ERROR(alloc_fb(), TAG, "framebuffer");
    s_last_crc = state->last_crc;
    s_pushed = state->pushed;
    return ESP_OK;
}

esp_err_t display_init_lost(void)
{
    return display_init();
}

void display_export(display_state_t *out)
{
    *out = (display_state_t){ .last_crc = s_last_crc, .pushed = s_pushed };
}

esp_err_t display_prepare_deep_sleep(void)
{
    return ESP_OK;
}

void display_cancel_deep_sleep(void)
{
}

gfx_fb_t *display_fb(void)
{
    return s_fb.buf != NULL ? &s_fb : NULL;
}

esp_err_t display_sleep(void)
{
    return ESP_OK;
}

esp_err_t display_wake(void)
{
    return ESP_OK;
}

bool display_asleep(void)
{
    return false;
}

esp_err_t display_commit(bool force)
{
    ESP_RETURN_ON_FALSE(s_fb.buf != NULL, ESP_ERR_INVALID_STATE, TAG, "not initialised");
    uint32_t crc = util_crc32(0, s_fb.buf, gfx_fb_size(FB_W, FB_H));
    if (!force && s_pushed && crc == s_last_crc) {
        return ESP_OK;
    }
    s_last_crc = crc; /* T1: the panel update goes here */
    s_pushed = true;
    return ESP_OK;
}

esp_err_t display_set_fast(bool fast)
{
    (void)fast;
    return ESP_OK;
}

esp_err_t display_clean(void)
{
    return ESP_OK;
}
```

- [ ] **Step 5: Pick the sources per board**

Replace `components/display/CMakeLists.txt` with:

```cmake
# Display service: canonical framebuffer + panel pushes (spec §4.1, T5 spec §4.4). One source per board;
# REQUIRES stay the same for both, as ESP-IDF reads them before Kconfig.
if(CONFIG_REFLBO_BOARD_T5S3)
    set(srcs "display_t5s3.c")
else()
    set(srcs "display_rlcd42.c")
endif()
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       REQUIRES gfx st7305
                       PRIV_REQUIRES esp_timer util)
```

Replace `components/st7305/CMakeLists.txt` with:

```cmake
# ST7305 reflective LCD (spec §4.2): the RLCD board's panel. On the T5 the component is its headers only.
# st7305_frame.c is pure C and also built on the host.
if(CONFIG_REFLBO_BOARD_T5S3)
    set(srcs "")
else()
    set(srcs "st7305.c" "st7305_frame.c")
endif()
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES board esp_driver_gpio esp_driver_spi esp_lcd util)
```

- [ ] **Step 6: Callers use the neutral API**

In `main/app.c`:

1. Delete the `#if CONFIG_REFLBO_PANEL_INIT_XIAOZHI` / `#define PANEL_VARIANT` block (five lines) and the line `#include "st7305.h"`.
2. In `boot()`, replace the lost-snapshot branch

```c
    } else if (wake != POWER_WAKE_COLD) {
        /* Woke from deep sleep without a valid snapshot: the panel still runs, don't reset it. It
         * may have been asleep for the night, so wake it anyway (SLPOUT is harmless otherwise). */
        display_state_t fallback = { .variant = PANEL_VARIANT, .mode = ST7305_MODE_LPM, .lpm_rate = ST7305_LPM_1HZ,
                                     .asleep = true };
        ESP_RETURN_ON_ERROR(display_init_warm(&fallback), TAG, "display");
        st7305_set_lpm_rate(ST7305_LPM_1HZ); /* assumed, so send it; safe without a reset */
        err = display_wake();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "panel wake: %s", esp_err_to_name(err));
        }
    } else {
        ESP_RETURN_ON_ERROR(display_init(PANEL_VARIANT), TAG, "display");
    }
```

with

```c
    } else if (wake != POWER_WAKE_COLD) {
        /* Woke from deep sleep without a valid snapshot: the panel still runs, don't reset it. It
         * may have been asleep for the night, so wake it anyway (SLPOUT is harmless otherwise). */
        ESP_RETURN_ON_ERROR(display_init_lost(), TAG, "display");
    } else {
        ESP_RETURN_ON_ERROR(display_init(), TAG, "display");
    }
```

In `main/app_menu.c`, `main/app_config.c` and `main/app_radar.c`, replace each panel mode switch. These are six calls: `app_menu_open`/`app_menu_close`, `app_config_enter`/`app_config_exit`, and `app_radar_loop_start`/`app_radar_loop_stop`. Use `display_set_fast(true)` for `st7305_set_mode(ST7305_MODE_HPM)` and `display_set_fast(false)` for `st7305_set_mode(ST7305_MODE_LPM)`, keeping each comment. Change the log texts: `"HPM: %s"` → `"fast: %s"`, `"LPM: %s"` → `"slow: %s"`, `"loop: HPM: %s"` → `"loop: fast: %s"`, `"loop: LPM: %s"` → `"loop: slow: %s"`. Then:
- in `app_menu.c` and `app_config.c`, delete `#include "st7305.h"` (`display.h` is already included);
- in `app_radar.c`, replace `#include "st7305.h"` with `#include "display.h"`.

In `main/app_ui.c`, add `#include "board_caps.h"` before `#include "display.h"`, and wrap the LPM-rate block in `app_ui_apply_settings()` (from `int quarter_hz = s.settings.lpm_quarter_hz, rate = 0;` to the closing `}` of `if (display_fb() != NULL && st7305_lpm_rate() ...)`) in `#if BOARD_HAS_LPM_RATE` ... `#endif`.

- [ ] **Step 7: The console's `panel` per board**

Replace `components/diag/diag_cmd_display.c` with:

```c
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board_caps.h"
#include "diag_internal.h"
#include "display.h"
#include "esp_console.h"
#include "esp_heap_caps.h"
#include "gfx_test_pattern.h"
#include "util_base64.h"
#if BOARD_HAS_LPM_RATE
#include "st7305.h"
#endif

#define PBM_CHUNK 57 /* bytes per line: 76 base64 characters */

static int screenshot_body(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    gfx_fb_t *fb = display_fb();
    if (fb == NULL) {
        printf("screenshot: display not initialised\n");
        return 1;
    }
    size_t size = gfx_pbm_size(fb);
    uint8_t *pbm = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (pbm == NULL) {
        printf("screenshot: out of memory\n");
        return 1;
    }
    size_t len = gfx_pbm_encode(fb, pbm, size);
    char line[80];
    printf("-----BEGIN RLCD PBM-----\n");
    for (size_t off = 0; off < len; off += PBM_CHUNK) {
        size_t chunk = len - off < PBM_CHUNK ? len - off : PBM_CHUNK;
        util_base64_encode(pbm + off, chunk, line, sizeof(line));
        printf("%s\n", line);
    }
    printf("-----END RLCD PBM-----\n");
    free(pbm);
    return 0;
}

#if BOARD_HAS_LPM_RATE

#define PANEL_USAGE                                                                                                  \
    "panel status | test | clear | mode <hpm|lpm> | rate <0.25|0.5|1|2|4|8> | fps [s] | sleep | wake | init "      \
    "<factory|xiaozhi>"
#define PANEL_HELP "panel status | test | clear | mode <hpm|lpm> | rate <0.25|0.5|1|2|4|8> | fps [s] | init <factory|xiaozhi>"

static int print_status(void)
{
    printf("panel %s, mode %s, lpm rate %s Hz%s\n", st7305_variant() == ST7305_VARIANT_XIAOZHI ? "xiaozhi" : "factory",
           st7305_mode() == ST7305_MODE_LPM ? "lpm" : "hpm", st7305_lpm_rate_name(st7305_lpm_rate()),
           st7305_asleep() ? ", asleep" : "");
    return 0;
}

/* Parses "0.25" ... "8"; false for anything else. */
static bool parse_lpm_rate(const char *text, st7305_lpm_rate_t *rate)
{
    for (int r = ST7305_LPM_0_25HZ; r <= ST7305_LPM_8HZ; r++) {
        if (strcmp(text, st7305_lpm_rate_name((st7305_lpm_rate_t)r)) == 0) {
            *rate = (st7305_lpm_rate_t)r;
            return true;
        }
    }
    return false;
}

/* Measures the panel frame rate from its TE pulses over `seconds`. */
static int print_frame_rate(int seconds)
{
    uint32_t frames = 0;
    esp_err_t err = st7305_count_frames((uint32_t)seconds * 1000u, &frames);
    if (err != ESP_OK) {
        printf("panel: %s\n", esp_err_to_name(err));
        return 1;
    }
    unsigned centi_hz = (unsigned)(frames * 100u / (unsigned)seconds);
    printf("panel fps: %lu frames in %d s = %u.%02u Hz\n", (unsigned long)frames, seconds, centi_hz / 100,
           centi_hz % 100);
    return 0;
}

/* Parses a whole number of seconds from 1 to 30. */
static bool parse_seconds(const char *text, int *seconds)
{
    char *end;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || value < 1 || value > 30) {
        return false;
    }
    *seconds = (int)value;
    return true;
}

#else /* e-paper (T5 spec §9): its own status and commands come with its driver (T1) */

#define PANEL_USAGE "panel status | test | clear | sleep | wake"
#define PANEL_HELP  PANEL_USAGE

static int print_status(void)
{
    printf("panel e-paper%s\n", display_asleep() ? ", asleep" : "");
    return 0;
}

#endif

static int panel_body(int argc, char **argv)
{
    esp_err_t err = ESP_ERR_INVALID_ARG;
    gfx_fb_t *fb = display_fb();
    if (fb == NULL) {
        printf("panel: display not initialised\n");
        return 1;
    }
#if BOARD_HAS_LPM_RATE
    st7305_lpm_rate_t rate;
    int seconds = 4;
#endif
    if (argc == 2 && strcmp(argv[1], "status") == 0) {
        return print_status();
    } else if (argc == 2 && strcmp(argv[1], "test") == 0) {
        gfx_draw_test_pattern(fb);
        err = display_commit(false);
    } else if (argc == 2 && strcmp(argv[1], "clear") == 0) {
        gfx_clear(fb, GFX_WHITE);
        err = display_commit(false);
    } else if (argc == 2 && strcmp(argv[1], "sleep") == 0) {
        err = display_sleep();
    } else if (argc == 2 && strcmp(argv[1], "wake") == 0) {
        err = display_wake();
#if BOARD_HAS_LPM_RATE
    } else if ((argc == 2 || argc == 3) && strcmp(argv[1], "fps") == 0 &&
               (argc == 2 || parse_seconds(argv[2], &seconds))) {
        return print_frame_rate(seconds);
    } else if (argc == 3 && strcmp(argv[1], "mode") == 0 && strcmp(argv[2], "hpm") == 0) {
        err = st7305_set_mode(ST7305_MODE_HPM);
    } else if (argc == 3 && strcmp(argv[1], "mode") == 0 && strcmp(argv[2], "lpm") == 0) {
        err = st7305_set_mode(ST7305_MODE_LPM);
    } else if (argc == 3 && strcmp(argv[1], "rate") == 0 && parse_lpm_rate(argv[2], &rate)) {
        err = st7305_set_lpm_rate(rate);
    } else if (argc == 3 && strcmp(argv[1], "init") == 0 && strcmp(argv[2], "factory") == 0) {
        err = display_set_variant(ST7305_VARIANT_FACTORY);
    } else if (argc == 3 && strcmp(argv[1], "init") == 0 && strcmp(argv[2], "xiaozhi") == 0) {
        err = display_set_variant(ST7305_VARIANT_XIAOZHI);
#endif
    } else {
        printf("usage: %s\n", PANEL_USAGE);
        return 1;
    }
    if (err != ESP_OK) {
        printf("panel: %s\n", esp_err_to_name(err));
        return 1;
    }
    return print_status();
}

static int cmd_screenshot(int argc, char **argv)
{
    return diag_on_owner(screenshot_body, argc, argv);
}

static int cmd_panel(int argc, char **argv)
{
    return diag_on_owner(panel_body, argc, argv);
}

esp_err_t diag_register_display_commands(void)
{
    const esp_console_cmd_t cmds[] = {
        { .command = "screenshot", .help = "Print the framebuffer as base64 PBM between markers", .func = &cmd_screenshot },
        { .command = "panel", .help = PANEL_HELP, .func = &cmd_panel },
    };
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        esp_err_t err = esp_console_cmd_register(&cmds[i]);
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}
```

On the RLCD the usage line, the help text and every branch's effect are what they were. The branches' order changed, but their conditions don't overlap.

- [ ] **Step 8: Build the RLCD and run the host tests**

Run: `cd ~/reflbo-t5 && tools/idf.sh build 2>&1 | grep -E "warning:|error:|Project build complete" && cmake --build build-host && ctest --test-dir build-host --output-on-failure 2>&1 | tail -1 && git status --porcelain test/host/golden`
Expected: "Project build complete" (the `_Static_assert` on `display_state_t` holds), `100% tests passed`, no golden listed.

- [ ] **Step 9: Review the RLCD paths**

In `git diff`, check:
- `display_rlcd42.c` is the old `display.c` plus `PANEL_VARIANT`, `display_init_lost`, `display_set_fast` and `display_clean`.
- `display_init_lost()` runs exactly the old fallback: attach, rate 1 Hz, wake, log a failed wake.
- The six mode switches call `st7305_set_mode()` with the same mode as before.
- The LPM-rate block in `app_ui.c` is untouched inside its `#if`.

- [ ] **Step 10: Note the seam in AGENTS.md**

In AGENTS.md §10 Seams, append:

```markdown
`display.h` is board-neutral (`display_rlcd42.c`, `display_t5s3.c`; the board's snapshot state and RLCD-only calls in `display_board.h`). `display_set_fast()` replaces direct HPM/LPM switches, and `display_clean()` is the T5's clean refresh. On the T5 the display is a stub until T1, and `st7305` is headers only.
```

- [ ] **Step 11: Commit**

```bash
cd ~/reflbo-t5 && git add components/display components/st7305/CMakeLists.txt main/app.c main/app_menu.c main/app_config.c main/app_radar.c main/app_ui.c components/diag/diag_cmd_display.c AGENTS.md
git commit -m "refactor(display): a board-neutral display API (T0)"
```

---

### Task 5: Waking without the RTC's INT; the T5 builds

**Files:**
- Modify: `components/power/power.c`
- Modify: `main/app.c` (alarm backup, missed-alarm warnings, RTC INT)
- Modify: `AGENTS.md` (§10 Seams)

**Interfaces:**
- Consumes: `board_caps.h` (`BOARD_HAS_RTC_ALARM_WAKE`), `rtcchip_set_alarm()` (Task 3).
- Produces: `ALARM_BACKUP_S` in `app.c` (`BACKUP_S` with the alarm, `0` without); `POWER_WAKE_RTC` never occurs on a board without the alarm wake.

- [ ] **Step 1: `power.c` without the RTC's INT where there is none**

In `components/power/power.c`:

1. Add `#include "board_caps.h"` before `#include "board_pins.h"`.
2. Replace `static const gpio_num_t k_wake_pins[] = { BOARD_PIN_RTC_INT, BOARD_PIN_KEY, BOARD_PIN_BOOT };` with:

```c
#if BOARD_HAS_RTC_ALARM_WAKE
static const gpio_num_t k_wake_pins[] = { BOARD_PIN_RTC_INT, BOARD_PIN_KEY, BOARD_PIN_BOOT };
#define RTC_INT_WAKE_BIT BIT64(BOARD_PIN_RTC_INT)
#else
static const gpio_num_t k_wake_pins[] = { BOARD_PIN_KEY, BOARD_PIN_BOOT }; /* INT isn't wired (T5 spec §8.2) */
#define RTC_INT_WAKE_BIT 0
#endif

/* The edge each wake pin's interrupt uses while awake: the RTC's INT falls, the buttons go both ways. */
static gpio_int_type_t awake_edge(gpio_num_t pin)
{
#if BOARD_HAS_RTC_ALARM_WAKE
    return pin == BOARD_PIN_RTC_INT ? GPIO_INTR_NEGEDGE : GPIO_INTR_ANYEDGE;
#else
    (void)pin;
    return GPIO_INTR_ANYEDGE;
#endif
}
```

3. In `decode_boot_wake()`, wrap

```c
        if (pins & BIT64(BOARD_PIN_RTC_INT)) {
            return POWER_WAKE_RTC;
        }
```

in `#if BOARD_HAS_RTC_ALARM_WAKE` ... `#endif`.

4. In `power_sleep_light()`, replace

```c
        gpio_set_intr_type(k_wake_pins[i], k_wake_pins[i] == BOARD_PIN_RTC_INT ? GPIO_INTR_NEGEDGE : GPIO_INTR_ANYEDGE);
```

with

```c
        gpio_set_intr_type(k_wake_pins[i], awake_edge(k_wake_pins[i]));
```

and wrap the RTC branch of the wake decision:

```c
#if BOARD_HAS_RTC_ALARM_WAKE
    } else if (gpio_get_level(BOARD_PIN_RTC_INT) == 0) {
        wake = POWER_WAKE_RTC;
#endif
    } else if (err == ESP_OK && (esp_sleep_get_wakeup_causes() & BIT(ESP_SLEEP_WAKEUP_TIMER))) {
```

5. In `power_sleep_deep()`, wrap the two `rtc_gpio_pullup_en(BOARD_PIN_RTC_INT);` / `rtc_gpio_pulldown_dis(BOARD_PIN_RTC_INT);` lines in `#if BOARD_HAS_RTC_ALARM_WAKE` ... `#endif`, and replace `BIT64(BOARD_PIN_RTC_INT) | button_wake_bits(held)` with `RTC_INT_WAKE_BIT | button_wake_bits(held)`.

- [ ] **Step 2: The app's ticks without an alarm**

In `main/app.c`:

1. Add `#include "board_caps.h"` before `#include "board.h"`.
2. After `#define BACKUP_S 5 ...`, add:

```c
/* On a board without the RTC alarm's wake (T5 spec §8.2) the timer is the tick itself, not its backup. */
#if BOARD_HAS_RTC_ALARM_WAKE
#define ALARM_BACKUP_S BACKUP_S
#else
#define ALARM_BACKUP_S 0
#endif
```

3. Replace every other use of `BACKUP_S`: `sleep_until()`, the TIMER case of `handle_wake()`, `enter_night_sleep()`, the timer-wake warning in `boot()`, and the awake loop's backup tick.

```c
/* The timer wake: a cycle switch or seconds tick if one comes before the alarm, else the alarm's backup. */
static time_t sleep_until(void)
{
    return s_wake_at < s_next_alarm ? s_wake_at : s_next_alarm + ALARM_BACKUP_S;
}
```

```c
    case POWER_WAKE_TIMER: /* a cycle switch or seconds tick, or the alarm's backup */
        if (BOARD_HAS_RTC_ALARM_WAKE && time(NULL) >= s_next_alarm + ALARM_BACKUP_S) {
            ESP_LOGW(TAG, "RTC alarm missed; backup wake");
        }
        on_tick(false, false);
        break;
```

```c
    time_t wake = until + ALARM_BACKUP_S;
```

```c
    if (BOARD_HAS_RTC_ALARM_WAKE && wake == POWER_WAKE_TIMER && time(NULL) >= s_next_alarm + ALARM_BACKUP_S) {
        ESP_LOGW(TAG, "RTC alarm missed; backup wake");
    }
```

```c
        } else if (err == ESP_OK && time(NULL) >= s_next_alarm + ALARM_BACKUP_S) {
            if (BOARD_HAS_RTC_ALARM_WAKE) {
                ESP_LOGW(TAG, "RTC alarm missed; backup tick");
            }
            on_tick(false, false);
        } else if (err == ESP_OK && time(NULL) >= s_wake_at) {
```

Afterwards, `grep -n "BACKUP_S" main/app.c` must show only the `#define BACKUP_S` line, the two `ALARM_BACKUP_S` defines, and the uses above.

4. Wrap `on_rtc_int()` (the ISR, `static void IRAM_ATTR on_rtc_int(void *arg) { ... }`) in `#if BOARD_HAS_RTC_ALARM_WAKE` ... `#endif`, and make `start_rtc_int()`:

```c
static esp_err_t start_rtc_int(void)
{
#if BOARD_HAS_RTC_ALARM_WAKE
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << BOARD_PIN_RTC_INT,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, /* open drain, no external pull-up */
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io), TAG, "RTC INT pin");
    return gpio_isr_handler_add(BOARD_PIN_RTC_INT, on_rtc_int, NULL);
#else
    return ESP_OK; /* the RTC's INT isn't wired (T5 spec §8.2) */
#endif
}
```

- [ ] **Step 3: Build both boards**

Run:

```bash
cd ~/reflbo-t5 && tools/idf.sh build 2>&1 | grep -E "warning:|error:|Project build complete"
REFLBO_BOARD=t5 tools/idf.sh build 2>&1 | grep -E "warning:|error:|Project build complete"
```

Expected: both end with "Project build complete", and no `warning:` or `error:` lines. If the T5 build reports a symbol or pin that only the RLCD has, leave it out with the matching `BOARD_HAS_*` the way Tasks 2–5 do. Never add an RLCD pin to `board_pins_t5s3.h`.

- [ ] **Step 4: The T5 image carries no RLCD driver, and each build kept its board**

Run:

```bash
cd ~/reflbo-t5 && tools/idf.sh exec xtensa-esp32s3-elf-nm build-t5/reflbo.elf > build-t5/nm.txt
for s in st7305_ shtc3_ pcf85063_ write_codec; do printf '%s %s\n' "$s" "$(grep -c "$s" build-t5/nm.txt)"; done
grep -h "define CONFIG_REFLBO_BOARD_" build/config/sdkconfig.h build-t5/config/sdkconfig.h
```

Expected: `st7305_ 0`, `shtc3_ 0`, `pcf85063_ 0`, `write_codec 0`; then `CONFIG_REFLBO_BOARD_RLCD42 1` and `CONFIG_REFLBO_BOARD_T5S3 1`.

- [ ] **Step 5: Host tests**

Run: `cd ~/reflbo-t5 && cmake --build build-host && ctest --test-dir build-host --output-on-failure 2>&1 | tail -1 && git status --porcelain test/host/golden`
Expected: `100% tests passed`, no golden listed.

- [ ] **Step 6: Review the RLCD paths**

In `git diff`, check that on the RLCD (`BOARD_HAS_RTC_ALARM_WAKE` 1):
- `ALARM_BACKUP_S` is `BACKUP_S`.
- Every warning still logs under the same condition.
- `k_wake_pins` and the ext1 mask are the old ones.
- `awake_edge()` gives NEGEDGE for GPIO15 and ANYEDGE for the buttons.

- [ ] **Step 7: Note the seam in AGENTS.md**

In AGENTS.md §10 Seams, append:

```markdown
Without `BOARD_HAS_RTC_ALARM_WAKE` the ESP32's timer is the wake: `power.c` leaves the RTC's INT out of its wake pins, and `app.c`'s `ALARM_BACKUP_S` is 0, with no "alarm missed" warnings.
```

- [ ] **Step 8: Commit**

```bash
cd ~/reflbo-t5 && git add components/power/power.c main/app.c AGENTS.md
git commit -m "feat(power): timer wakes on boards without the RTC alarm; the T5 builds (T0)"
```

---

### Task 6: The UI's profile of the board

**Files:**
- Create: `components/ui/include/ui_profile.h`
- Create: `components/ui/ui_profile.c`
- Modify: `components/ui/CMakeLists.txt`
- Modify: `components/ui/include/ui_layout.h` (`UI_STATUS_H`)
- Modify: `components/ui/ui_split.c` (`ui_split_area`)
- Modify: `components/ui/ui_catalog.c`
- Modify: `main/app.c` (profile choice and caps check), `main/app_web.c` (the preview's size)
- Create: `test/host/test_ui_profile.c`
- Modify: `test/host/CMakeLists.txt`, `test/host/test_ui_catalog.c`
- Modify: `AGENTS.md` (§10 Seams)

**Interfaces:**
- Consumes: `board_caps.h` (in `main` only; `ui` stays free of ESP-IDF and board headers).
- Produces:
  - `typedef struct { const char *board; int16_t width, height; int16_t status_h; uint32_t caps; } ui_profile_t;`
  - `extern const ui_profile_t ui_profile_rlcd42, ui_profile_t5s3;`
  - `const ui_profile_t *ui_profile(void);`
  - `void ui_profile_use(const ui_profile_t *profile);` (`NULL` restores the RLCD)
  - `const char *ui_cap_name(uint32_t cap);`
  - `UI_CAP_ENV_SENSOR`, `UI_CAP_AUDIO`, `UI_CAP_RTC_TRIM`, `UI_CAP_RTC_ALARM_WAKE`, `UI_CAP_LPM_RATE`, `UI_CAPS_RLCD42`, `UI_CAPS_T5S3`
  - `UI_STATUS_H` becomes `(ui_profile()->status_h)`.
  - `/api/layouts` gains `"board"` (string) and `"caps"` (array of cap names).

- [ ] **Step 1: Write the failing profile tests**

Create `test/host/test_ui_profile.c`:

```c
#include "ui_layout.h"
#include "ui_profile.h"
#include "ui_split.h"
#include "unity.h"

void setUp(void)
{
}

void tearDown(void)
{
    ui_profile_use(NULL);
}

static void test_the_rlcd_is_the_default_profile(void)
{
    TEST_ASSERT_EQUAL_PTR(&ui_profile_rlcd42, ui_profile());
    TEST_ASSERT_EQUAL_STRING("rlcd42", ui_profile()->board);
    TEST_ASSERT_EQUAL_INT(400, ui_profile()->width);
    TEST_ASSERT_EQUAL_INT(300, ui_profile()->height);
    TEST_ASSERT_EQUAL_INT(20, UI_STATUS_H);
}

static void test_use_switches_the_profile_and_null_restores_the_rlcd(void)
{
    ui_profile_use(&ui_profile_t5s3);
    TEST_ASSERT_EQUAL_STRING("t5s3", ui_profile()->board);
    ui_profile_use(NULL);
    TEST_ASSERT_EQUAL_PTR(&ui_profile_rlcd42, ui_profile());
}

/* T0 (T5 spec §11): the T5 keeps the RLCD's geometry until T3 and differs only in what it has. */
static void test_the_t5_has_the_rlcd_geometry_and_none_of_its_capabilities(void)
{
    TEST_ASSERT_EQUAL_INT(400, ui_profile_t5s3.width);
    TEST_ASSERT_EQUAL_INT(300, ui_profile_t5s3.height);
    TEST_ASSERT_EQUAL_INT(20, ui_profile_t5s3.status_h);
    TEST_ASSERT_EQUAL_HEX32(0, ui_profile_t5s3.caps);
    TEST_ASSERT_EQUAL_HEX32(UI_CAP_ENV_SENSOR | UI_CAP_AUDIO | UI_CAP_RTC_TRIM | UI_CAP_RTC_ALARM_WAKE | UI_CAP_LPM_RATE,
                            ui_profile_rlcd42.caps);
}

static void test_the_rlcd_split_area_is_unchanged(void)
{
    gfx_rect_t a = ui_split_area();
    TEST_ASSERT_EQUAL_INT(0, a.x);
    TEST_ASSERT_EQUAL_INT(21, a.y);
    TEST_ASSERT_EQUAL_INT(400, a.w);
    TEST_ASSERT_EQUAL_INT(279, a.h);
}

static void test_the_split_area_and_status_bar_follow_the_profile(void)
{
    static const ui_profile_t wide = { "test", 960, 540, 34, 0 };
    ui_profile_use(&wide);
    TEST_ASSERT_EQUAL_INT(34, UI_STATUS_H);
    gfx_rect_t a = ui_split_area();
    TEST_ASSERT_EQUAL_INT(0, a.x);
    TEST_ASSERT_EQUAL_INT(35, a.y);
    TEST_ASSERT_EQUAL_INT(960, a.w);
    TEST_ASSERT_EQUAL_INT(505, a.h);
}

static void test_each_capability_has_a_name(void)
{
    TEST_ASSERT_EQUAL_STRING("env_sensor", ui_cap_name(UI_CAP_ENV_SENSOR));
    TEST_ASSERT_EQUAL_STRING("audio", ui_cap_name(UI_CAP_AUDIO));
    TEST_ASSERT_EQUAL_STRING("rtc_trim", ui_cap_name(UI_CAP_RTC_TRIM));
    TEST_ASSERT_EQUAL_STRING("rtc_alarm_wake", ui_cap_name(UI_CAP_RTC_ALARM_WAKE));
    TEST_ASSERT_EQUAL_STRING("lpm_rate", ui_cap_name(UI_CAP_LPM_RATE));
    TEST_ASSERT_NULL(ui_cap_name(1u << 31));
    TEST_ASSERT_NULL(ui_cap_name(UI_CAP_AUDIO | UI_CAP_RTC_TRIM)); /* one bit at a time */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_rlcd_is_the_default_profile);
    RUN_TEST(test_use_switches_the_profile_and_null_restores_the_rlcd);
    RUN_TEST(test_the_t5_has_the_rlcd_geometry_and_none_of_its_capabilities);
    RUN_TEST(test_the_rlcd_split_area_is_unchanged);
    RUN_TEST(test_the_split_area_and_status_bar_follow_the_profile);
    RUN_TEST(test_each_capability_has_a_name);
    return UNITY_END();
}
```

In `test/host/CMakeLists.txt`, after `reflbo_host_test(test_ui_split ui)`, add:

```cmake
reflbo_host_test(test_ui_profile ui)
```

In `test/host/test_ui_catalog.c`, add `#include "ui_profile.h"` after `#include "ui_catalog.h"`, then this test before `main()`:

```c
static bool has_string(const cJSON *array, const char *text)
{
    const cJSON *item;
    cJSON_ArrayForEach(item, array)
    {
        if (cJSON_IsString(item) && strcmp(item->valuestring, text) == 0) {
            return true;
        }
    }
    return false;
}

/* T5 spec §4.3: the page learns the board and what it has from the catalogue. */
static void test_layouts_name_the_board_and_its_capabilities(void)
{
    TEST_ASSERT_TRUE(ui_catalog_layouts_json(s_out, sizeof(s_out)) > 0);
    s_root = cJSON_Parse(s_out);
    TEST_ASSERT_NOT_NULL(s_root);
    TEST_ASSERT_EQUAL_STRING("rlcd42", str(s_root, "board"));
    TEST_ASSERT_EQUAL_INT(20, num(s_root, "status_h"));
    const cJSON *caps = cJSON_GetObjectItemCaseSensitive(s_root, "caps");
    TEST_ASSERT_EQUAL_INT(5, cJSON_GetArraySize(caps));
    TEST_ASSERT_TRUE(has_string(caps, "env_sensor"));
    TEST_ASSERT_TRUE(has_string(caps, "lpm_rate"));
    cJSON_Delete(s_root);

    ui_profile_use(&ui_profile_t5s3);
    TEST_ASSERT_TRUE(ui_catalog_layouts_json(s_out, sizeof(s_out)) > 0);
    ui_profile_use(NULL);
    s_root = cJSON_Parse(s_out);
    TEST_ASSERT_NOT_NULL(s_root);
    TEST_ASSERT_EQUAL_STRING("t5s3", str(s_root, "board"));
    TEST_ASSERT_EQUAL_INT(400, num(s_root, "width"));
    TEST_ASSERT_EQUAL_INT(0, cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(s_root, "caps")));
}
```

and register it in `main()` after `RUN_TEST(test_layouts_publish_the_split_rules);`:

```c
    RUN_TEST(test_layouts_name_the_board_and_its_capabilities);
```

- [ ] **Step 2: Run them and see them fail**

Run: `cd ~/reflbo-t5 && cmake -S test/host -B build-host -G Ninja && cmake --build build-host 2>&1 | grep -m3 "error"`
Expected: compile errors: `ui_profile.h` not found.

- [ ] **Step 3: The profile**

Create `components/ui/include/ui_profile.h`:

```c
#pragma once

#include <stdint.h>

/*
 * The board as the UI sees it (T5 spec §7.1): the panel's size, the status bar, the board's name and
 * what it has. Pure C, host-buildable. T0 gives both boards the RLCD's geometry; T3 gives the T5 its
 * own (960×540) and moves the layouts, fonts and screen geometry in here.
 */

/* UI_CAP_* bits: hardware a board has, shown by the UI and the web page only where it exists. main
 * checks them against board_caps.h's BOARD_HAS_*. */
#define UI_CAP_ENV_SENSOR     (1u << 0) /* temperature and humidity (env.*) */
#define UI_CAP_AUDIO          (1u << 1)
#define UI_CAP_RTC_TRIM       (1u << 2)
#define UI_CAP_RTC_ALARM_WAKE (1u << 3)
#define UI_CAP_LPM_RATE       (1u << 4) /* the panel's refresh rate setting */

#define UI_CAPS_RLCD42 (UI_CAP_ENV_SENSOR | UI_CAP_AUDIO | UI_CAP_RTC_TRIM | UI_CAP_RTC_ALARM_WAKE | UI_CAP_LPM_RATE)
#define UI_CAPS_T5S3   0u

typedef struct {
    const char *board; /* "rlcd42", "t5s3": as BOARD_NAME (board_caps.h) */
    int16_t width, height;
    int16_t status_h; /* the status bar; its line is the row below it */
    uint32_t caps;    /* UI_CAP_* */
} ui_profile_t;

extern const ui_profile_t ui_profile_rlcd42;
extern const ui_profile_t ui_profile_t5s3;

/* The profile in use: ui_profile_rlcd42 until ui_profile_use(). */
const ui_profile_t *ui_profile(void);
/* Sets the profile in use; NULL restores the RLCD's. The firmware calls it once at boot. */
void ui_profile_use(const ui_profile_t *profile);
/* "env_sensor", "audio", "rtc_trim", "rtc_alarm_wake", "lpm_rate"; NULL for anything but one known bit. */
const char *ui_cap_name(uint32_t cap);
```

Create `components/ui/ui_profile.c`:

```c
#include "ui_profile.h"

#include <stddef.h>

const ui_profile_t ui_profile_rlcd42 = { "rlcd42", 400, 300, 20, UI_CAPS_RLCD42 };
/* T0: the RLCD's geometry until T3 (T5 spec §11); only the board and its capabilities differ. */
const ui_profile_t ui_profile_t5s3 = { "t5s3", 400, 300, 20, UI_CAPS_T5S3 };

static const ui_profile_t *s_profile = &ui_profile_rlcd42;

const ui_profile_t *ui_profile(void)
{
    return s_profile;
}

void ui_profile_use(const ui_profile_t *profile)
{
    s_profile = profile != NULL ? profile : &ui_profile_rlcd42;
}

const char *ui_cap_name(uint32_t cap)
{
    switch (cap) {
    case UI_CAP_ENV_SENSOR:
        return "env_sensor";
    case UI_CAP_AUDIO:
        return "audio";
    case UI_CAP_RTC_TRIM:
        return "rtc_trim";
    case UI_CAP_RTC_ALARM_WAKE:
        return "rtc_alarm_wake";
    case UI_CAP_LPM_RATE:
        return "lpm_rate";
    default:
        return NULL;
    }
}
```

In `components/ui/CMakeLists.txt`, add `"ui_profile.c"` to `SRCS` after `"ui_layout.c"`. The host build picks it up through its glob.

- [ ] **Step 4: The status bar, split area and catalogue read the profile**

In `components/ui/include/ui_layout.h`, add `#include "ui_profile.h"` after `#include "ui_fields.h"`, and replace `#define UI_STATUS_H 20` with:

```c
#define UI_STATUS_H (ui_profile()->status_h) /* 20 on the RLCD (T5 spec §7.1) */
```

In `components/ui/ui_split.c`, delete `#define PANEL_W 400` and `#define PANEL_H 300`, and replace `ui_split_area()` with:

```c
gfx_rect_t ui_split_area(void)
{
    const ui_profile_t *p = ui_profile();
    return (gfx_rect_t){ 0, (int16_t)(p->status_h + 1), p->width, (int16_t)(p->height - p->status_h - 1) };
}
```

In `components/ui/ui_catalog.c`, delete `#define PANEL_W 400` and `#define PANEL_H 300`, add `#include "ui_profile.h"` after `#include "ui_layout.h"`, and replace the first three lines after `cJSON *root = cJSON_CreateObject();` in `ui_catalog_layouts_json()` with:

```c
    const ui_profile_t *p = ui_profile();
    cJSON_AddStringToObject(root, "board", p->board);
    cJSON_AddNumberToObject(root, "width", p->width);
    cJSON_AddNumberToObject(root, "height", p->height);
    cJSON_AddNumberToObject(root, "status_h", p->status_h);
    cJSON *caps = cJSON_AddArrayToObject(root, "caps");
    for (uint32_t bit = 1; bit != 0; bit <<= 1) {
        const char *name = ui_cap_name(bit);
        if ((p->caps & bit) != 0 && name != NULL) {
            cJSON_AddItemToArray(caps, cJSON_CreateString(name));
        }
    }
```

- [ ] **Step 5: Run the tests and see them pass**

Run: `cd ~/reflbo-t5 && cmake --build build-host && ctest --test-dir build-host --output-on-failure 2>&1 | tail -3 && git status --porcelain test/host/golden`
Expected: `100% tests passed` (with `test_ui_profile` and the new catalogue test), no golden listed.

- [ ] **Step 6: The firmware picks its profile and checks its capabilities**

In `main/app.c`, add `#include "ui_profile.h"` after `#include "ui_screens.h"`, and after `static const char *TAG = "app";` add:

```c

/* The UI's profile of this board (T5 spec §4.3, §7.1); its capabilities must say what board_caps.h says. */
#if CONFIG_REFLBO_BOARD_T5S3
#define UI_PROFILE_BOARD ui_profile_t5s3
#define UI_CAPS_BOARD    UI_CAPS_T5S3
#else
#define UI_PROFILE_BOARD ui_profile_rlcd42
#define UI_CAPS_BOARD    UI_CAPS_RLCD42
#endif
_Static_assert(((UI_CAPS_BOARD & UI_CAP_ENV_SENSOR) != 0) == BOARD_HAS_ENV_SENSOR, "UI caps: env sensor");
_Static_assert(((UI_CAPS_BOARD & UI_CAP_AUDIO) != 0) == BOARD_HAS_AUDIO, "UI caps: audio");
_Static_assert(((UI_CAPS_BOARD & UI_CAP_RTC_TRIM) != 0) == BOARD_HAS_RTC_TRIM, "UI caps: RTC trim");
_Static_assert(((UI_CAPS_BOARD & UI_CAP_RTC_ALARM_WAKE) != 0) == BOARD_HAS_RTC_ALARM_WAKE, "UI caps: RTC alarm");
_Static_assert(((UI_CAPS_BOARD & UI_CAP_LPM_RATE) != 0) == BOARD_HAS_LPM_RATE, "UI caps: LPM rate");
```

Make the profile the first thing `boot()` does: insert `ui_profile_use(&UI_PROFILE_BOARD);` as the first statement of `boot()`, before `esp_err_t err = power_init();`.

In `main/app_web.c`, add `#include "ui_profile.h"` after `#include "ui_dashboard.h"`, and make `preview_fb()` take the panel's size:

```c
/* The framebuffer the previews draw into; the display's own stays as it is. */
static gfx_fb_t *preview_fb(void)
{
    static gfx_fb_t fb;
    static uint8_t *buf;
    if (buf == NULL) {
        const ui_profile_t *p = ui_profile();
        buf = heap_caps_malloc(gfx_fb_size(p->width, p->height), MALLOC_CAP_SPIRAM);
        if (buf == NULL) {
            return NULL;
        }
        gfx_fb_init(&fb, buf, p->width, p->height);
    }
    return &fb;
}
```

- [ ] **Step 7: Build both boards**

Run: `cd ~/reflbo-t5 && tools/idf.sh build 2>&1 | grep -E "warning:|error:|Project build complete" && REFLBO_BOARD=t5 tools/idf.sh build 2>&1 | grep -E "warning:|error:|Project build complete"`
Expected: "Project build complete" twice, no `warning:` or `error:`. A failing `_Static_assert` means `ui_profile.h` and `board_caps.h` disagree: fix the profile's caps, not the assert.

- [ ] **Step 8: The web page's tests still pass**

Run: `cd ~/reflbo-t5 && node --test test/web/test_app.mjs 2>&1 | tail -3`
Expected: all pass. The page doesn't read `board` or `caps` yet (T3), and its fixture keeps `width`, `height` and `status_h`.

- [ ] **Step 9: Note the seam in AGENTS.md**

In AGENTS.md §10 Seams, append:

```markdown
`ui_profile.h` is the UI's view of the board (`ui_profile()`: panel size, status bar, board name, `UI_CAP_*`), published in `/api/layouts` as `board` and `caps`. `main` picks it at boot and checks with `_Static_assert` that its capabilities match `board_caps.h`. `UI_STATUS_H` reads the profile. The T5's profile has the RLCD's geometry until T3.
```

- [ ] **Step 10: Commit**

```bash
cd ~/reflbo-t5 && git add components/ui test/host/test_ui_profile.c test/host/CMakeLists.txt test/host/test_ui_catalog.c main/app.c main/app_web.c AGENTS.md
git commit -m "feat(ui): the UI's profile of the board, in the catalogue (T0)"
```

---

### Task 7: Record T0

**Files:**
- Modify: `docs/specs/2026-10-06-t5-board-design.md` (r2)
- Modify: `AGENTS.md` (§10 Status)

- [ ] **Step 1: Bring the spec to r2**

In `docs/specs/2026-10-06-t5-board-design.md`:

1. In the header, change `- **Status:** Draft for the owner's review (r1)` to `- **Status:** Approved by the owner on 2026-10-06 (r1); r2 records T0 as built (§13)`.
2. In §4.3, replace the sentence that lists the capabilities with: `` `board_caps.h` states, per board, at compile time: `BOARD_NAME`, `BOARD_HAS_ENV_SENSOR`, `BOARD_HAS_AUDIO`, `BOARD_HAS_RTC_TRIM`, `BOARD_HAS_RTC_ALARM_WAKE`, `BOARD_HAS_LPM_RATE`; `BOARD_GRAYSCALE` joins at T2. The UI's profile carries the same as `UI_CAP_*` bits, and `main` checks with `_Static_assert` that they agree. ``
3. In §4.4's list, after the `display_init_warm` bullet, add: ``- `display_init_lost(void)`: a deep-sleep wake without a valid snapshot (the RLCD attaches, sets 1 Hz and wakes the panel, as `app.c` did).``
4. In §7.1, append the paragraph: `T0 starts the profile with the panel's size, the status bar's height, the board's name and its capabilities (`ui_profile_t`), and `UI_STATUS_H`, the split area, the catalogue and the web preview read it. The layouts, fonts, screen geometry, split limits, view sizes and `UI_PX()` move in at T3, when the T5 has numbers of its own.`
5. In §11's T0 row, replace `the UI profile holding the RLCD's numbers` with `the UI profile with the panel's size, status bar, board and capabilities`.
6. In §13, add the row: `| r2 | 2026-10-06 | T0 as built: the capabilities' list (§4.3), `display_init_lost()` (§4.4), the profile's first members (§7.1, §11) |`

- [ ] **Step 2: AGENTS.md's status**

In AGENTS.md §10, replace the Status bullet with:

```markdown
- **Status:** T0 is done (2026-10-06): the board is a build-time choice, with seams for pins, capabilities, the display, the RTC, wakes and the UI's profile. The T5 builds with a display and an RTC that do nothing yet, and its image holds no RLCD driver. Next: T1, bring-up (its plan is written before it starts; the owner connects the T5 for it).
```

- [ ] **Step 3: Final checks**

Run:

```bash
cd ~/reflbo-t5 && tools/idf.sh build 2>&1 | grep -E "warning:|error:|Project build complete" && REFLBO_BOARD=t5 tools/idf.sh build 2>&1 | grep -E "warning:|error:|Project build complete" \
  && cmake --build build-host && ctest --test-dir build-host --output-on-failure 2>&1 | tail -1 \
  && git status --porcelain test/host/golden && git log --oneline upstream/main..HEAD
```

Expected: two "Project build complete" and no `warning:`, `100% tests passed`, no golden, and the log ahead of `upstream/main` shows the spec, this plan (and any fixes to it), and one commit for each of Tasks 1–6.

- [ ] **Step 4: Commit**

```bash
cd ~/reflbo-t5 && git add docs/specs/2026-10-06-t5-board-design.md AGENTS.md
git commit -m "docs: T0 as built (T5 spec r2)"
```
