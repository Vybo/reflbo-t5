#pragma once

#include <stdint.h>

#include "gfx.h"

/* Draws a 1 bpp frame (gfx: 1 = black) into epdiy's 4 bpp framebuffer (two pixels a byte, the even one in
 * the low nibble; 0 black, 15 white) with its top left at (x0, y0), clipped to dst_w × dst_h (dst_w
 * even). Everything else in `dst` becomes white. T1-T2: the RLCD's 400×300 frame in the panel's middle
 * (T5 spec §11). Pure C, host-buildable. */
void epaper_frame_blit_1bpp(const gfx_fb_t *src, uint8_t *dst, int dst_w, int dst_h, int x0, int y0);
