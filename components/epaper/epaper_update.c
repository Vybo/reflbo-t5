#include "epaper_update.h"

#include <string.h>

/* A pixel's level: the even pixel in the low nibble. */
static uint8_t px(const uint8_t *buf, int w, int x, int y)
{
    return (uint8_t)((buf[y * ((w + 1) / 2) + x / 2] >> ((x & 1) ? 4 : 0)) & 0x0F);
}

void epaper_change(const uint8_t *before, const uint8_t *after, int w, int h, epaper_change_t *out)
{
    memset(out, 0, sizeof(*out));
    out->bw_only = true;
    int stride = (w + 1) / 2, x0 = w, y0 = h, x1 = -1, y1 = -1;
    for (int y = 0; y < h; y++) {
        const uint8_t *b = before + y * stride, *a = after + y * stride;
        if (memcmp(b, a, (size_t)stride) == 0) {
            continue; /* most rows don't change */
        }
        for (int x = 0; x < w; x++) {
            uint8_t from = px(before, w, x, y), to = px(after, w, x, y);
            if (from == to) {
                continue;
            }
            if ((from != 0 && from != 15) || (to != 0 && to != 15)) {
                out->bw_only = false;
            }
            x0 = x < x0 ? x : x0;
            x1 = x > x1 ? x : x1;
            y0 = y < y0 ? y : y0;
            y1 = y;
        }
    }
    if (x1 < 0) {
        out->bw_only = false;
        return;
    }
    out->any = true;
    out->x = (int16_t)x0;
    out->y = (int16_t)y0;
    out->w = (int16_t)(x1 - x0 + 1);
    out->h = (int16_t)(y1 - y0 + 1);
}

epaper_mode_t epaper_choose(const epaper_policy_t *p, const epaper_change_t *c)
{
    if (!p->known || p->clean_next) {
        return EPAPER_CLEAN;
    }
    if (!c->any) {
        return EPAPER_NONE;
    }
    if (!p->fast && p->since_clean + 1 >= (p->clean_after > 0 ? p->clean_after : 1)) {
        return EPAPER_CLEAN; /* every clean_after-th counted update */
    }
    return c->bw_only ? EPAPER_DU : EPAPER_GL16;
}

void epaper_after(epaper_policy_t *p, epaper_mode_t mode, bool ok)
{
    if (mode == EPAPER_NONE) {
        return;
    }
    if (!ok) {
        p->known = false; /* the panel may show anything between the two frames: the next is clean */
        return;
    }
    if (mode == EPAPER_CLEAN) {
        p->known = true;
        p->clean_next = false;
        p->since_clean = 0;
    } else if (!p->fast && p->since_clean < UINT16_MAX) {
        p->since_clean++;
    }
}
