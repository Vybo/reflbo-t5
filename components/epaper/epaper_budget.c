#include "epaper_budget.h"

bool epaper_internal_ram_ok(size_t free_bytes, size_t largest_block)
{
    return free_bytes >= EPAPER_MIN_FREE_INTERNAL && largest_block >= EPAPER_MIN_BLOCK_INTERNAL;
}

bool epaper_lut_ram_ok(size_t largest_internal, size_t largest_psram)
{
    return largest_internal >= EPAPER_LUT_BYTES || largest_psram >= EPAPER_LUT_BYTES;
}
