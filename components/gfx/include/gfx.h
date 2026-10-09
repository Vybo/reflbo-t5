#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gfx_font.h"

/*
 * Drawing (spec §4.3, T5 spec §6). Two framebuffer formats:
 * - GFX_FMT_1BPP: row-major, MSB = leftmost pixel, bit 1 = black, the PBM P4 raster layout;
 * - GFX_FMT_4BPP: epdiy's, row-major, two pixels a byte, the even one in the low nibble, 0 = black, 15 = white.
 * Primitives pick the format's writer once per call. Pure C with no ESP-IDF headers, so it builds on the host.
 */

typedef enum {
    GFX_WHITE = 0,
    GFX_BLACK = 1,
    GFX_INVERT = 2,
} gfx_color_t;

/* Grays between black and white, the panel's levels: n = 1 (darkest) to 14 (lightest). On a 1 bpp buffer a
 * gray is a 4×4 ordered dither, (15 − n) / 15 of its pixels black (T5 spec §6.2). */
#define GFX_GRAY_BASE 16
#define GFX_GRAY(n) ((gfx_color_t)(GFX_GRAY_BASE + (n)))

typedef enum {
    GFX_FMT_1BPP = 0,
    GFX_FMT_4BPP = 1,
} gfx_format_t;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
} gfx_rect_t;

typedef struct {
    uint8_t *buf;
    int16_t width;
    int16_t height;
    int16_t stride;  /* bytes per row: (width + 7) / 8 at 1 bpp, (width + 1) / 2 at 4 bpp */
    gfx_rect_t clip; /* drawing is limited to this rectangle */
    uint8_t format;  /* gfx_format_t; a zeroed struct is 1 bpp */
} gfx_fb_t;

size_t gfx_fb_size(int16_t width, int16_t height);                         /* 1 bpp */
void gfx_fb_init(gfx_fb_t *fb, uint8_t *buf, int16_t width, int16_t height); /* 1 bpp */
size_t gfx_fb_size_fmt(gfx_format_t format, int16_t width, int16_t height);
void gfx_fb_init_fmt(gfx_fb_t *fb, uint8_t *buf, int16_t width, int16_t height, gfx_format_t format);
void gfx_clear(gfx_fb_t *fb, gfx_color_t color); /* whole buffer; ignores the clip */

gfx_rect_t gfx_rect_intersect(gfx_rect_t a, gfx_rect_t b); /* empty result has w = h = 0 */
void gfx_set_clip(gfx_fb_t *fb, gfx_rect_t clip);           /* intersected with the framebuffer */
void gfx_reset_clip(gfx_fb_t *fb);

void gfx_pixel(gfx_fb_t *fb, int x, int y, gfx_color_t color);
bool gfx_get_pixel(const gfx_fb_t *fb, int x, int y); /* true = black; false outside the buffer */
/* The pixel's level: 0 black to 15 white (1 bpp: 0 or 15); 15 outside the buffer. gfx_get_pixel() is level < 8. */
uint8_t gfx_get_level(const gfx_fb_t *fb, int x, int y);
/* Ink `color` at `coverage` (0 none to 15 full) over what's there, clipped like gfx_pixel(). 4 bpp moves the
 * level towards the ink's (towards 15 − v for GFX_INVERT) by coverage / 15, rounded; 1 bpp inks the pixel
 * from coverage 8 (T5 spec §6.2). */
void gfx_pixel_coverage(gfx_fb_t *fb, int x, int y, gfx_color_t color, uint8_t coverage);
void gfx_hline(gfx_fb_t *fb, int x, int y, int w, gfx_color_t color);
void gfx_vline(gfx_fb_t *fb, int x, int y, int h, gfx_color_t color);
void gfx_line(gfx_fb_t *fb, int x0, int y0, int x1, int y1, gfx_color_t color);
void gfx_rect(gfx_fb_t *fb, gfx_rect_t r, gfx_color_t color); /* outline, each pixel drawn once */
void gfx_fill_rect(gfx_fb_t *fb, gfx_rect_t r, gfx_color_t color);
void gfx_circle(gfx_fb_t *fb, int cx, int cy, int r, gfx_color_t color); /* outline, each pixel drawn once */
void gfx_fill_circle(gfx_fb_t *fb, int cx, int cy, int r, gfx_color_t color);
/* The pixels whose centres lie inside or on the triangle, in any vertex order. */
void gfx_fill_triangle(gfx_fb_t *fb, int x0, int y0, int x1, int y1, int x2, int y2, gfx_color_t color);

/* An image in the glyph format: rows padded to whole bytes, at 1 bpp MSB first with 1 = ink, at 4 bpp two
 * pixels a byte, the first in the high nibble, coverage 0-15. */
typedef struct {
    const uint8_t *bits;
    uint16_t width;
    uint16_t height;
    uint8_t bpp; /* 1 or 4; 0 (an initializer without it) reads as 1 */
} gfx_bitmap_t;

/* Draws the bitmap's ink in `color` with its top-left corner at (x, y), blending 4-bit coverage
 * (gfx_pixel_coverage()); other pixels are left alone. For an opaque image, fill its rectangle first. */
void gfx_bitmap(gfx_fb_t *fb, int x, int y, const gfx_bitmap_t *bm, gfx_color_t color);

/* PBM P4 image of the framebuffer: header "P4\n<w> <h>\n" followed by the raster. */
size_t gfx_pbm_size(const gfx_fb_t *fb);
size_t gfx_pbm_encode(const gfx_fb_t *fb, uint8_t *out, size_t out_size); /* 0 if out_size is too small */

/* 1-bit BMP of the framebuffer, for browsers (the web UI's preview and screenshot). */
size_t gfx_bmp_size(const gfx_fb_t *fb);
size_t gfx_bmp_encode(const gfx_fb_t *fb, uint8_t *out, size_t out_size); /* 0 if out_size is too small */

/* `text` as a QR code (ECC medium, versions up to 10), with its 4-module quiet zone, `scale`
 * pixels per module, its top-left corner at (x, y). Returns its side in pixels, or 0 if the text
 * doesn't fit. Not reentrant: one encoding buffer is shared. */
int gfx_qr(gfx_fb_t *fb, int x, int y, int scale, const char *text);
int gfx_qr_side(const char *text, int scale); /* what gfx_qr() would draw; 0 if it doesn't fit */


typedef enum {
    GFX_ALIGN_LEFT,
    GFX_ALIGN_CENTER,
    GFX_ALIGN_RIGHT,
} gfx_align_t;

/* Decodes one UTF-8 codepoint and advances *s. Returns 0 at the terminating NUL and U+FFFD for
 * malformed input; never reads past the NUL. */
uint32_t gfx_utf8_next(const char **s);

bool gfx_font_has_glyph(const gfx_font_t *font, uint32_t codepoint);
const gfx_glyph_t *gfx_font_glyph(const gfx_font_t *font, uint32_t codepoint); /* NULL if missing */
int gfx_text_width(const gfx_font_t *font, const char *utf8);
/* Draws one line with its baseline at `baseline`; returns the pen x after the text.
 * A codepoint missing from the font is drawn as a hollow box. */
int gfx_text(gfx_fb_t *fb, const gfx_font_t *font, int x, int baseline, const char *utf8, gfx_color_t color);
/* One line aligned in r, vertically centred on the font's line box, clipped to r. */
void gfx_text_in_rect(gfx_fb_t *fb, const gfx_font_t *font, gfx_rect_t r, gfx_align_t align, const char *utf8,
                      gfx_color_t color);
/* Copies `utf8` into `out`. If it is wider than max_width, cuts it at a codepoint and ends it
 * with an ellipsis ("…", or "..." if the font lacks it). Returns the width of the result. */
int gfx_text_ellipsize(const gfx_font_t *font, const char *utf8, int max_width, char *out, size_t out_size);

/* The text functions treat a NULL string as empty. */
