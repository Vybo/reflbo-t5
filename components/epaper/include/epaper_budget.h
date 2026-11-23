#pragma once

#include <stdbool.h>
#include <stddef.h>

/*
 * The internal RAM epdiy needs to come up for an update (T5 spec §5.1, final review of T1): its line
 * queues, feed buffers, task stacks and the I2S DMA buffers, about 25 KB in blocks of at most 4 KB, plus
 * room for Wi-Fi while it runs. Its LUT falls back to PSRAM (P5); the rest asserts, so the board would
 * abort. Pure C, host-buildable.
 */

#define EPAPER_MIN_FREE_INTERNAL  (40 * 1024)
#define EPAPER_MIN_BLOCK_INTERNAL (8 * 1024)

#define EPAPER_LUT_BYTES (64 * 1024) /* EPD_LUT_64K, display_t547.c's option */
/* What internal RAM must keep beside the LUT for it to go there: epdiy's other allocations, 57 KB at T5b's board
 * check (129 KB free before epd_init(), 7.7 KB after it with the LUT inside), and about 40 KB for Wi-Fi meanwhile. */
#define EPAPER_LUT_SPARE_INTERNAL (96 * 1024)

/* Whether epdiy can come up: false skips the update, and the next commit tries again. */
bool epaper_internal_ram_ok(size_t free_bytes, size_t largest_block);

/* Whether the LUT has a block: in internal RAM, or else in PSRAM (P5), whose failure epdiy answers with
 * abort() (T3b review: RainViewer's hour of 960 px frames can fill PSRAM). False skips the update too. */
bool epaper_lut_ram_ok(size_t largest_internal, size_t largest_psram);

/* Where the LUT goes (P5, T5 spec DT18): internal RAM, 60 % faster (T1), when it holds the LUT with
 * EPAPER_LUT_SPARE_INTERNAL to spare; else PSRAM; with no block there either, internal RAM rather than no update. */
bool epaper_lut_internal(size_t free_internal, size_t largest_internal, size_t largest_psram);
