#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "sdkconfig.h"

/* What each board's display keeps through deep sleep (the app's RTC-RAM snapshot), and the calls only
 * one board has (T5 spec §4.4). */

#if CONFIG_REFLBO_BOARD_T547

typedef struct {
    uint32_t last_crc;    /* CRC of the frame last committed */
    bool pushed;
    uint16_t since_clean; /* counted updates since the last clean one (T4) */
} display_state_t;

/* `panel status` (T5 spec §9). */
typedef struct {
    uint32_t updates;        /* since boot, that ran */
    uint32_t fast, clean;    /* of them (T4) */
    uint32_t failed;         /* refused by epdiy's RAM budget or a draw error */
    uint32_t last_ms;        /* the last update, from epdiy's start to its end */
    uint32_t min_internal;   /* the least free internal RAM seen with epdiy up, bytes; 0 before the first */
    uint16_t since_clean;    /* counted updates since the last clean one */
    uint8_t clean_after;     /* every Nth update is clean */
    uint8_t last_mode;       /* epaper_mode_t of the last update */
    int16_t last_x, last_y, last_w, last_h; /* its area */
} display_t5_stats_t;
void display_t5_stats(display_t5_stats_t *out);

/* `panel bench` (T5 spec §9): the test pattern drawn into the frame and clean (clear and GC16), its inverse in
 * GL16, the pattern again in DU. The app's next render and commit put the dashboard back. */
typedef struct {
    uint32_t clean_ms, gl16_ms, du_ms;
} display_t5_bench_t;
esp_err_t display_t5_bench(display_t5_bench_t *out);

/* `panel test` (T5 spec §9): gfx_draw_test_pattern_t5() into the frame, clean. The app's next render and
 * commit put the dashboard back. */
esp_err_t display_t5_test_pattern(void);

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
