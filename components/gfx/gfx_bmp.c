#include <string.h>

#include "gfx.h"

/* A BMP (spec §4.3, T5 spec §6.5) for the web UI's preview and screenshot: BITMAPFILEHEADER,
 * BITMAPINFOHEADER, a palette, then the rows bottom-up, each padded to 4 bytes. At 1 bpp a two-colour
 * palette with index 1 black lets the canonical rows (1 = black, MSB first) go in unchanged. At 4 bpp a
 * 16-gray palette (index i is gray i × 17) takes the levels as they are, each byte's nibbles swapped: BMP
 * puts the left pixel in the high nibble, epdiy in the low. */

static bool gray(const gfx_fb_t *fb)
{
    return fb->format == GFX_FMT_4BPP;
}

static size_t headers_size(const gfx_fb_t *fb)
{
    return 14 + 40 + (gray(fb) ? 16 * 4 : 2 * 4);
}

static size_t row_size(const gfx_fb_t *fb)
{
    return ((size_t)fb->stride + 3u) & ~(size_t)3u;
}

size_t gfx_bmp_size(const gfx_fb_t *fb)
{
    return headers_size(fb) + row_size(fb) * (size_t)fb->height;
}

static void put16(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *p, uint32_t v)
{
    put16(p, v & 0xFFFFu);
    put16(p + 2, v >> 16);
}

size_t gfx_bmp_encode(const gfx_fb_t *fb, uint8_t *out, size_t out_size)
{
    size_t total = gfx_bmp_size(fb), row = row_size(fb), headers = headers_size(fb);
    if (out_size < total) {
        return 0;
    }
    memset(out, 0, headers);
    out[0] = 'B';
    out[1] = 'M';
    put32(out + 2, (uint32_t)total);
    put32(out + 10, (uint32_t)headers); /* where the pixels start */
    put32(out + 14, 40);                /* BITMAPINFOHEADER */
    put32(out + 18, (uint32_t)fb->width);
    put32(out + 22, (uint32_t)fb->height); /* positive: bottom-up rows */
    put16(out + 26, 1);                    /* planes */
    put16(out + 28, gray(fb) ? 4 : 1);     /* bits per pixel */
    put32(out + 34, (uint32_t)(row * (size_t)fb->height));
    put32(out + 38, 2835); /* 72 dpi */
    put32(out + 42, 2835);
    put32(out + 46, gray(fb) ? 16 : 2); /* colours in the palette */
    if (gray(fb)) {
        for (int i = 0; i < 16; i++) {
            uint8_t v = (uint8_t)(i * 17);
            uint8_t *entry = out + 54 + i * 4;
            entry[0] = entry[1] = entry[2] = v;
        }
    } else {
        memcpy(out + 54, "\xFF\xFF\xFF\x00\x00\x00\x00\x00", 8); /* 0 white, 1 black */
    }
    for (int y = 0; y < fb->height; y++) {
        uint8_t *dst = out + headers + row * (size_t)(fb->height - 1 - y);
        const uint8_t *src = fb->buf + (size_t)y * (size_t)fb->stride;
        if (gray(fb)) {
            for (int i = 0; i < fb->stride; i++) {
                dst[i] = (uint8_t)((src[i] << 4) | (src[i] >> 4));
            }
        } else {
            memcpy(dst, src, (size_t)fb->stride);
        }
        memset(dst + fb->stride, 0, row - (size_t)fb->stride);
    }
    return total;
}
