#pragma once

#include "epd_board.h"
#include "esp_err.h"

/*
 * The epdiy board definition for the LilyGo T5-ePaper-S3 V2.3 (T5 spec §5.2): the panel's control lines
 * and rails through the 74HCT4094, rows through the patched epdiy's S3 i80 bus (components/epdiy,
 * PATCHES.md P4), CKV from RMT. Belongs to the app task, as the display does.
 */
extern const EpdBoardDefinition epaper_board_t5s3;

/* The shift register to rest (rails and outputs off), its clock and data driven low (also through light
 * sleep), and the strobe (IO0, the BOOT button) released to its pull-up. At boot before anything else
 * touches the panel, and after every epd_deinit() (T5 spec §5.4). */
void epaper_board_idle(void);
/* Holds the shift register's clock and data low through deep sleep, so nothing can clock the rails on. */
esp_err_t epaper_board_prepare_deep_sleep(void);
/* Releases those holds: after a deep-sleep wake, or after epaper_board_prepare_deep_sleep() failed. */
void epaper_board_release_hold(void);
