#include <stdio.h>
#include <string.h>

#include "gfx.h"

static int pbm_header(const gfx_fb_t *fb, char *out, size_t size)
{
    return snprintf(out, size, "P4\n%d %d\n", fb->width, fb->height);
}

size_t gfx_pbm_size(const gfx_fb_t *fb)
{
    if (fb->format == GFX_FMT_4BPP) {
        return 0;
    }
    char header[32];
    return (size_t)pbm_header(fb, header, sizeof(header)) + gfx_fb_size(fb->width, fb->height);
}

size_t gfx_pbm_encode(const gfx_fb_t *fb, uint8_t *out, size_t out_size)
{
    if (fb->format == GFX_FMT_4BPP) {
        return 0;
    }
    char header[32];
    int n = pbm_header(fb, header, sizeof(header));
    size_t raster = gfx_fb_size(fb->width, fb->height);
    if (n <= 0 || out_size < (size_t)n + raster) {
        return 0;
    }
    memcpy(out, header, (size_t)n);
    memcpy(out + n, fb->buf, raster);
    return (size_t)n + raster;
}

static int pgm_header(const gfx_fb_t *fb, char *out, size_t size)
{
    return snprintf(out, size, "P5\n%d %d\n255\n", fb->width, fb->height);
}

size_t gfx_pgm_size(const gfx_fb_t *fb)
{
    char header[32];
    return (size_t)pgm_header(fb, header, sizeof(header)) + (size_t)fb->width * (size_t)fb->height;
}

size_t gfx_pgm_encode(const gfx_fb_t *fb, uint8_t *out, size_t out_size)
{
    char header[32];
    int n = pgm_header(fb, header, sizeof(header));
    size_t total = gfx_pgm_size(fb);
    if (n <= 0 || out_size < total) {
        return 0;
    }
    memcpy(out, header, (size_t)n);
    uint8_t *p = out + n;
    for (int y = 0; y < fb->height; y++) {
        for (int x = 0; x < fb->width; x++) {
            *p++ = (uint8_t)(gfx_get_level(fb, x, y) * 17);
        }
    }
    return total;
}
