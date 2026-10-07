# epdiy, vendored

- **Upstream:** https://github.com/vroland/epdiy, tag `2.1.3` (commit `7c30780`), LGPL-3.0-or-later (`LICENSE`).
- **Vendored:** `src/` and `LICENSE`, unmodified in the first commit (`chore(epdiy): vendor epdiy 2.1.3 unmodified`); the patches below come after it, so `git diff <that commit> -- components/epdiy` shows them all.
- **Why:** the LilyGo T5-ePaper-S3 V2.3 latches each row through a 74HCT4094 (LE and STV are on the shift register), but epdiy on the ESP32-S3 always uses its LCD path, which needs LE on a GPIO for HSYNC (T5 spec §2.5, DT4).
- **To update:** vendor the new tag the same way, reapply P1-P6, check the I2S path's interfaces (`output_i2s/i2s_data_bus.h`, `render_i2s.c`) still match P4.

| # | Where | Change |
|---|---|---|
| P1 | `Kconfig` (new) | `EPD_S3_SHIFT_REGISTER_LATCH` and `EPD_LUT_IN_PSRAM` |
| P2 | `src/output_common/render_method.h`, `render_method.c` | With `EPD_S3_SHIFT_REGISTER_LATCH`, the ESP32-S3 uses the I2S render method (row by row, the latch through the board's `set_ctrl`) |
| P3 | (none) | Reserved: `i2s_data_bus.c`, the ESP32's bus, `lut.S` and `diff.S`, the S3's vector code for the LCD path, are left out of the build instead of patched |
| P4 | `src/output_i2s/i2s_data_bus_s3.c` (new) | The I2S path's data bus on the S3: LCD_CAM in i80 mode through `esp_lcd`, one DMA transfer a row, STH on the DC line |
| P5 | `src/render.c` | The conversion LUT in PSRAM with `EPD_LUT_IN_PSRAM`, and a PSRAM fallback instead of `abort()` when internal RAM is short |
| P6 | `CMakeLists.txt` (new, replaces upstream's) | Builds only on the T5 and only the I2S path's sources; no `idf_component.yml` |

The board definition for the T5-ePaper-S3 V2.3 is reflbo's own (`components/epaper/epaper_board.c`, Apache-2.0), written to epdiy's `EpdBoardDefinition` interface.
