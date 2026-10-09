#pragma once

#include "gfx.h"

/* Draws the display test pattern (spec §4.2) into a 400x300 framebuffer: border, origin marker,
 * corner labels, glyph samples, a 10-px grid, two checkerboards and crossing diagonals. */
void gfx_draw_test_pattern(gfx_fb_t *fb);
/* The T5's (T5 spec §9, `panel test`): the RLCD's pattern, then on a 4 bpp buffer a 16-step gray ramp with
 * its levels, anti-aliased text and an icon in black on white and in white on black. Meant for 960×540; on
 * 1 bpp it is the RLCD's pattern alone. */
void gfx_draw_test_pattern_t5(gfx_fb_t *fb);
