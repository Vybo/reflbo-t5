#include "gfx.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static int min_int(int a, int b)
{
    return a < b ? a : b;
}

static int max_int(int a, int b)
{
    return a > b ? a : b;
}

size_t gfx_fb_size(int16_t width, int16_t height)
{
    return gfx_fb_size_fmt(GFX_FMT_1BPP, width, height);
}

size_t gfx_fb_size_fmt(gfx_format_t format, int16_t width, int16_t height)
{
    int stride = format == GFX_FMT_4BPP ? (width + 1) / 2 : (width + 7) / 8;
    return (size_t)stride * (size_t)height;
}

void gfx_fb_init(gfx_fb_t *fb, uint8_t *buf, int16_t width, int16_t height)
{
    gfx_fb_init_fmt(fb, buf, width, height, GFX_FMT_1BPP);
}

void gfx_fb_init_fmt(gfx_fb_t *fb, uint8_t *buf, int16_t width, int16_t height, gfx_format_t format)
{
    fb->buf = buf;
    fb->width = width;
    fb->height = height;
    fb->format = (uint8_t)format;
    fb->stride = (int16_t)(format == GFX_FMT_4BPP ? (width + 1) / 2 : (width + 7) / 8);
    gfx_reset_clip(fb);
}

/* 4×4 Bayer thresholds for grays on 1 bpp. */
static const uint8_t k_bayer4[4][4] = {
    { 0, 8, 2, 10 },
    { 12, 4, 14, 6 },
    { 3, 11, 1, 9 },
    { 15, 7, 13, 5 },
};

/* The level an ink paints: 0 black, 15 white, n for GFX_GRAY(n). */
static uint8_t ink_level(gfx_color_t color)
{
    if (color == GFX_BLACK) {
        return 0;
    }
    if (color >= GFX_GRAY_BASE) {
        int n = (int)color - GFX_GRAY_BASE;
        return (uint8_t)(n > 15 ? 15 : n);
    }
    return 15;
}

static void put1(gfx_fb_t *fb, int x, int y, gfx_color_t color)
{
    uint8_t *byte = &fb->buf[y * fb->stride + (x >> 3)];
    uint8_t mask = (uint8_t)(0x80u >> (x & 7));
    if (color >= GFX_GRAY_BASE) { /* (15 − n) / 15 of the pixels black, by the 4×4 thresholds */
        color = k_bayer4[y & 3][x & 3] * 15 < (15 - ink_level(color)) * 16 ? GFX_BLACK : GFX_WHITE;
    }
    switch (color) {
    case GFX_BLACK:
        *byte |= mask;
        break;
    case GFX_WHITE:
        *byte &= (uint8_t)~mask;
        break;
    case GFX_INVERT:
        *byte ^= mask;
        break;
    default:
        break;
    }
}

static void put4_level(gfx_fb_t *fb, int x, int y, uint8_t level)
{
    uint8_t *byte = &fb->buf[y * fb->stride + (x >> 1)];
    int shift = (x & 1) ? 4 : 0; /* the even pixel in the low nibble */
    *byte = (uint8_t)((*byte & ~(0x0Fu << shift)) | ((unsigned)level << shift));
}

static uint8_t level4(const gfx_fb_t *fb, int x, int y)
{
    return (uint8_t)((fb->buf[y * fb->stride + (x >> 1)] >> ((x & 1) ? 4 : 0)) & 0x0F);
}

static void put4(gfx_fb_t *fb, int x, int y, gfx_color_t color)
{
    put4_level(fb, x, y, color == GFX_INVERT ? (uint8_t)(15 - level4(fb, x, y)) : ink_level(color));
}

typedef void (*put_fn)(gfx_fb_t *fb, int x, int y, gfx_color_t color);

static put_fn writer(const gfx_fb_t *fb)
{
    return fb->format == GFX_FMT_4BPP ? put4 : put1;
}

void gfx_clear(gfx_fb_t *fb, gfx_color_t color)
{
    size_t size = (size_t)fb->stride * (size_t)fb->height;
    if (color == GFX_INVERT) { /* 1 bpp flips each bit, 4 bpp makes each nibble 15 − v */
        for (size_t i = 0; i < size; i++) {
            fb->buf[i] ^= 0xFF;
        }
        return;
    }
    if (fb->format == GFX_FMT_4BPP) {
        uint8_t v = ink_level(color);
        memset(fb->buf, (int)(v | (v << 4)), size);
        return;
    }
    if (color >= GFX_GRAY_BASE) {
        for (int y = 0; y < fb->height; y++) {
            for (int x = 0; x < fb->width; x++) {
                put1(fb, x, y, color);
            }
        }
        return;
    }
    memset(fb->buf, color == GFX_BLACK ? 0xFF : 0x00, size);
}

gfx_rect_t gfx_rect_intersect(gfx_rect_t a, gfx_rect_t b)
{
    int x0 = max_int(a.x, b.x);
    int y0 = max_int(a.y, b.y);
    int x1 = min_int(a.x + a.w, b.x + b.w);
    int y1 = min_int(a.y + a.h, b.y + b.h);
    if (x1 <= x0 || y1 <= y0) {
        return (gfx_rect_t){ (int16_t)x0, (int16_t)y0, 0, 0 };
    }
    return (gfx_rect_t){ (int16_t)x0, (int16_t)y0, (int16_t)(x1 - x0), (int16_t)(y1 - y0) };
}

void gfx_set_clip(gfx_fb_t *fb, gfx_rect_t clip)
{
    fb->clip = gfx_rect_intersect(clip, (gfx_rect_t){ 0, 0, fb->width, fb->height });
}

void gfx_reset_clip(gfx_fb_t *fb)
{
    fb->clip = (gfx_rect_t){ 0, 0, fb->width, fb->height };
}

static bool visible(const gfx_fb_t *fb, int x, int y)
{
    const gfx_rect_t *c = &fb->clip;
    if (x < c->x || y < c->y || x >= c->x + c->w || y >= c->y + c->h) {
        return false;
    }
    return x >= 0 && y >= 0 && x < fb->width && y < fb->height; /* a clip set by hand may reach outside */
}

void gfx_pixel(gfx_fb_t *fb, int x, int y, gfx_color_t color)
{
    if (visible(fb, x, y)) {
        writer(fb)(fb, x, y, color);
    }
}

uint8_t gfx_get_level(const gfx_fb_t *fb, int x, int y)
{
    if (x < 0 || y < 0 || x >= fb->width || y >= fb->height) {
        return 15;
    }
    if (fb->format == GFX_FMT_4BPP) {
        return level4(fb, x, y);
    }
    return (fb->buf[y * fb->stride + (x >> 3)] & (0x80u >> (x & 7))) ? 0 : 15;
}

bool gfx_get_pixel(const gfx_fb_t *fb, int x, int y)
{
    return gfx_get_level(fb, x, y) < 8;
}

void gfx_pixel_coverage(gfx_fb_t *fb, int x, int y, gfx_color_t color, uint8_t coverage)
{
    if (coverage == 0) {
        return;
    }
    if (fb->format != GFX_FMT_4BPP || coverage >= 15) {
        if (coverage >= 8) {
            gfx_pixel(fb, x, y, color);
        }
        return;
    }
    if (!visible(fb, x, y)) {
        return;
    }
    int d = level4(fb, x, y);
    int t = color == GFX_INVERT ? 15 - d : ink_level(color);
    int num = (t - d) * coverage;
    int step = num >= 0 ? (num + 7) / 15 : -((-num + 7) / 15); /* rounded half away from zero */
    put4_level(fb, x, y, (uint8_t)(d + step));
}

void gfx_hline(gfx_fb_t *fb, int x, int y, int w, gfx_color_t color)
{
    const gfx_rect_t *c = &fb->clip;
    if (y < c->y || y >= c->y + c->h || y < 0 || y >= fb->height) {
        return;
    }
    int x0 = max_int(max_int(x, c->x), 0);
    int x1 = min_int(min_int(x + w, c->x + c->w), fb->width);
    put_fn put = writer(fb);
    for (int i = x0; i < x1; i++) {
        put(fb, i, y, color);
    }
}

void gfx_vline(gfx_fb_t *fb, int x, int y, int h, gfx_color_t color)
{
    const gfx_rect_t *c = &fb->clip;
    if (x < c->x || x >= c->x + c->w || x < 0 || x >= fb->width) {
        return;
    }
    int y0 = max_int(max_int(y, c->y), 0);
    int y1 = min_int(min_int(y + h, c->y + c->h), fb->height);
    put_fn put = writer(fb);
    for (int i = y0; i < y1; i++) {
        put(fb, x, i, color);
    }
}

enum { OUT_LEFT = 1, OUT_RIGHT = 2, OUT_TOP = 4, OUT_BOTTOM = 8 };

static int outcode(const gfx_rect_t *c, int64_t x, int64_t y)
{
    int code = 0;
    if (x < c->x) {
        code |= OUT_LEFT;
    } else if (x >= c->x + c->w) {
        code |= OUT_RIGHT;
    }
    if (y < c->y) {
        code |= OUT_TOP;
    } else if (y >= c->y + c->h) {
        code |= OUT_BOTTOM;
    }
    return code;
}

/* Cohen-Sutherland: trims the segment to the clip so a huge line costs no more than its visible part. */
static bool clip_line(const gfx_rect_t *c, int64_t *x0, int64_t *y0, int64_t *x1, int64_t *y1)
{
    int64_t left = c->x, right = c->x + c->w - 1, top = c->y, bottom = c->y + c->h - 1;
    int code0 = outcode(c, *x0, *y0);
    int code1 = outcode(c, *x1, *y1);
    for (;;) {
        if ((code0 | code1) == 0) {
            return true;
        }
        if (code0 & code1) {
            return false;
        }
        int code = code0 ? code0 : code1;
        int64_t dx = *x1 - *x0, dy = *y1 - *y0, x, y;
        if (code & OUT_BOTTOM) {
            y = bottom;
            x = *x0 + dx * (bottom - *y0) / dy;
        } else if (code & OUT_TOP) {
            y = top;
            x = *x0 + dx * (top - *y0) / dy;
        } else if (code & OUT_RIGHT) {
            x = right;
            y = *y0 + dy * (right - *x0) / dx;
        } else {
            x = left;
            y = *y0 + dy * (left - *x0) / dx;
        }
        if (code == code0) {
            *x0 = x;
            *y0 = y;
            code0 = outcode(c, x, y);
        } else {
            *x1 = x;
            *y1 = y;
            code1 = outcode(c, x, y);
        }
    }
}

void gfx_line(gfx_fb_t *fb, int x0, int y0, int x1, int y1, gfx_color_t color)
{
    int64_t ax = x0, ay = y0, bx = x1, by = y1;
    if (fb->clip.w <= 0 || fb->clip.h <= 0 || !clip_line(&fb->clip, &ax, &ay, &bx, &by)) {
        return;
    }
    int64_t dx = bx > ax ? bx - ax : ax - bx;
    int64_t dy = by > ay ? ay - by : by - ay; /* negative */
    int sx = ax < bx ? 1 : -1;
    int sy = ay < by ? 1 : -1;
    int64_t err = dx + dy;
    for (;;) {
        gfx_pixel(fb, (int)ax, (int)ay, color);
        if (ax == bx && ay == by) {
            break;
        }
        int64_t e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            ax += sx;
        }
        if (e2 <= dx) {
            err += dx;
            ay += sy;
        }
    }
}

void gfx_rect(gfx_fb_t *fb, gfx_rect_t r, gfx_color_t color)
{
    if (r.w <= 0 || r.h <= 0) {
        return;
    }
    gfx_hline(fb, r.x, r.y, r.w, color);
    if (r.h > 1) {
        gfx_hline(fb, r.x, r.y + r.h - 1, r.w, color);
    }
    if (r.h > 2) {
        gfx_vline(fb, r.x, r.y + 1, r.h - 2, color);
        if (r.w > 1) {
            gfx_vline(fb, r.x + r.w - 1, r.y + 1, r.h - 2, color);
        }
    }
}

void gfx_fill_rect(gfx_fb_t *fb, gfx_rect_t r, gfx_color_t color)
{
    gfx_rect_t area = gfx_rect_intersect(r, fb->clip);
    for (int y = area.y; y < area.y + area.h; y++) {
        gfx_hline(fb, area.x, y, area.w, color);
    }
}

void gfx_circle(gfx_fb_t *fb, int cx, int cy, int r, gfx_color_t color)
{
    if (r < 0) {
        return;
    }
    if (r == 0) {
        gfx_pixel(fb, cx, cy, color);
        return;
    }
    /* Midpoint circle; the octant loop visits each pixel once except where octants meet, so
     * collect the points of one step and skip duplicates before drawing (INVERT must hit each once). */
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        int px[8] = { cx + x, cx + y, cx - y, cx - x, cx - x, cx - y, cx + y, cx + x };
        int py[8] = { cy + y, cy + x, cy + x, cy + y, cy - y, cy - x, cy - x, cy - y };
        for (int i = 0; i < 8; i++) {
            bool seen = false;
            for (int j = 0; j < i; j++) {
                seen |= px[j] == px[i] && py[j] == py[i];
            }
            if (!seen) {
                gfx_pixel(fb, px[i], py[i], color);
            }
        }
        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

void gfx_fill_circle(gfx_fb_t *fb, int cx, int cy, int r, gfx_color_t color)
{
    if (r < 0) {
        return;
    }
    for (int dy = -r; dy <= r; dy++) {
        int half = 0;
        while ((half + 1) * (half + 1) + dy * dy <= r * r + r) { /* r*r + r rounds the rim like the outline */
            half++;
        }
        gfx_hline(fb, cx - half, cy + dy, 2 * half + 1, color);
    }
}

static long floor_div(long a, long b) /* b > 0 */
{
    long q = a / b;
    return a % b != 0 && a < 0 ? q - 1 : q;
}

/* Where the edge from (ax, ay) down to (bx, by) crosses row y, to the nearest pixel. */
static int edge_x(int ax, int ay, int bx, int by, int y)
{
    if (by == ay) {
        return ax;
    }
    long dy = by - ay;
    return ax + (int)floor_div(2L * (y - ay) * (bx - ax) + dy, 2 * dy);
}

void gfx_fill_triangle(gfx_fb_t *fb, int x0, int y0, int x1, int y1, int x2, int y2, gfx_color_t color)
{
    int t;
#define SWAP_IF(a, b)                                                                                                  \
    if (y##a > y##b) {                                                                                                 \
        t = x##a, x##a = x##b, x##b = t;                                                                               \
        t = y##a, y##a = y##b, y##b = t;                                                                               \
    }
    SWAP_IF(0, 1)
    SWAP_IF(1, 2)
    SWAP_IF(0, 1)
#undef SWAP_IF
    int top = max_int(y0, fb->clip.y), bottom = min_int(y2, fb->clip.y + fb->clip.h - 1);
    for (int y = top; y <= bottom; y++) {
        int a = edge_x(x0, y0, x2, y2, y);                                          /* the long edge */
        int b = y < y1 ? edge_x(x0, y0, x1, y1, y) : edge_x(x1, y1, x2, y2, y);   /* the two short ones */
        if (y0 == y2) { /* flat: from the leftmost vertex to the rightmost */
            a = min_int(x0, min_int(x1, x2));
            b = max_int(x0, max_int(x1, x2));
        }
        gfx_hline(fb, min_int(a, b), y, abs(b - a) + 1, color);
    }
}

void gfx_bitmap(gfx_fb_t *fb, int x, int y, const gfx_bitmap_t *bm, gfx_color_t color)
{
    if (bm == NULL || bm->bits == NULL) {
        return;
    }
    int row_bytes = (bm->width + 7) / 8;
    for (int r = 0; r < bm->height; r++) {
        for (int c = 0; c < bm->width; c++) {
            if (bm->bits[r * row_bytes + (c >> 3)] & (0x80u >> (c & 7))) {
                gfx_pixel(fb, x + c, y + r, color);
            }
        }
    }
}
