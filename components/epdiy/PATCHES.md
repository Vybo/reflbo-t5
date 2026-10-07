# epdiy, vendored

- **Upstream:** https://github.com/vroland/epdiy, tag `2.1.3` (commit `7c30780`), LGPL-3.0-or-later (`LICENSE`).
- **Vendored:** `src/` and `LICENSE`, unmodified in commit `645266d` (`chore(epdiy): vendor epdiy 2.1.3 unmodified`); `git diff 645266d -- components/epdiy/src` shows every patch.
- **Why vendored:** patch P5, which the ESP32's tight internal RAM needs (T5 spec §2.5, DT4).
- **Used as upstream has it:** the ESP32's I2S output path and the `epd_board_lilygo_t5_47` board definition, for the LilyGo T5-4.7 (ESP32).
- **To update:** vendor the new tag the same way, reapply the patches below.

| # | Where | Change |
|---|---|---|
| P1 | `Kconfig` (new) | `EPD_LUT_IN_PSRAM` |
| P5 | `src/render.c` | The conversion LUT in PSRAM with `EPD_LUT_IN_PSRAM`, and a PSRAM fallback instead of `abort()` when internal RAM is short |
| P6 | `CMakeLists.txt` (new, replaces upstream's) | Builds only on the T5 and only the ESP32 I2S path's sources and the lilygo_t5_47 board; no `idf_component.yml` |

History: the fork's T1 first assumed LilyGo's T5-ePaper-S3 and carried P2 (the I2S render method on an S3) and P4 (an S3 i80 row bus) for it, commits `2bb37f3` and `7232440`; T1's port to the ESP32 board removed them.
