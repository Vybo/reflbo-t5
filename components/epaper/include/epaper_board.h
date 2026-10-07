#pragma once

#include "esp_err.h"

/*
 * The LilyGo T5-4.7's 74HCT4094 outside epdiy (T5 spec §5.2, §5.4): epdiy's lilygo_t5_47 board drives it
 * during updates; the fork puts it to rest at boot and after each update, and holds its lines through deep
 * sleep. Belongs to the app task, as the display does.
 */

/* Rails and outputs off (epaper_sr_off()), clock and data driven low (also through light sleep), the
 * strobe (IO0) released to its pull-up. At boot before anything else touches the panel, and after every
 * epd_deinit(). */
void epaper_board_idle(void);
/* Holds the clock and data low through deep sleep: they're digital pads (T5 spec §2.5). */
esp_err_t epaper_board_prepare_deep_sleep(void);
/* Releases those holds: after a deep-sleep wake, or after epaper_board_prepare_deep_sleep() failed. */
void epaper_board_release_hold(void);
