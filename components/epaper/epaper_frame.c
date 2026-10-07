#include "epaper_frame.h"

#include <string.h>

void epaper_frame_blit_1bpp(const gfx_fb_t *src, uint8_t *dst, int dst_w, int dst_h, int x0, int y0)
{
    memset(dst, 0xFF, (size_t)(dst_w / 2) * (size_t)dst_h);
    for (int y = 0; y < src->height; y++) {
        int dy = y0 + y;
        if (dy < 0 || dy >= dst_h) {
            continue;
        }
        uint8_t *row = dst + (size_t)dy * (size_t)(dst_w / 2);
        for (int x = 0; x < src->width; x++) {
            int dx = x0 + x;
            if (dx < 0 || dx >= dst_w || !gfx_get_pixel(src, x, y)) {
                continue;
            }
            row[dx / 2] &= (uint8_t)(dx % 2 ? 0x0F : 0xF0); /* black: this pixel's nibble to 0 */
        }
    }
}
