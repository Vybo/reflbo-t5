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

/* Whether epdiy can come up: false skips the update, and the next commit tries again. */
bool epaper_internal_ram_ok(size_t free_bytes, size_t largest_block);
