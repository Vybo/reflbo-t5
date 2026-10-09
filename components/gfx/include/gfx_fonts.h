#pragma once

#include "gfx_font.h"

/* Bitmap fonts rendered from DejaVu Sans 2.37 by tools/gen_fonts.sh (see THIRD_PARTY.md). */
extern const gfx_font_t gfx_font_sans_12;
extern const gfx_font_t gfx_font_sans_16;
extern const gfx_font_t gfx_font_sans_20;
extern const gfx_font_t gfx_font_sans_bold_16;
extern const gfx_font_t gfx_font_sans_bold_20;
extern const gfx_font_t gfx_font_sans_bold_28;
/* DejaVu Sans Condensed Bold; digits, space, % + , - . / : ° and the minus sign only */
extern const gfx_font_t gfx_font_num_cb_48;
extern const gfx_font_t gfx_font_num_cb_72;
extern const gfx_font_t gfx_font_num_cb_110;
extern const gfx_font_t gfx_font_num_cb_130;
/* The T5's 4-bit anti-aliased fonts (T5 spec §6.3, §7.2): DejaVu Sans, Sans Bold and Sans Condensed Bold
 * (digits) at about 1.7× the RLCD's sizes. Only the T5's image carries them. */
extern const gfx_font_t gfx_font_t5_sans_20;
extern const gfx_font_t gfx_font_t5_sans_26;
extern const gfx_font_t gfx_font_t5_sans_34;
extern const gfx_font_t gfx_font_t5_bold_26;
extern const gfx_font_t gfx_font_t5_bold_34;
extern const gfx_font_t gfx_font_t5_bold_46;
extern const gfx_font_t gfx_font_t5_time_30; /* a time's characters: digits, ":", " ", A, M, P */
extern const gfx_font_t gfx_font_t5_num_80;
extern const gfx_font_t gfx_font_t5_num_120;
extern const gfx_font_t gfx_font_t5_num_180;
extern const gfx_font_t gfx_font_t5_num_220;
