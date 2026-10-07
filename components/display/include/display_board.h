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
