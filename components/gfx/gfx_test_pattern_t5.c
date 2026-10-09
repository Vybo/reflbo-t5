#include "gfx_test_pattern.h"

#include <stdio.h>

#include "gfx_fonts.h"
#include "gfx_icons_t5.h"

#define RAMP_X    20
#define RAMP_Y    300
#define RAMP_STEP 57
#define RAMP_H    56

void gfx_draw_test_pattern_t5(gfx_fb_t *fb)
{
    gfx_draw_test_pattern(fb);
    if (fb->format != GFX_FMT_4BPP) {
        return;
    }
    for (int i = 0; i < 16; i++) {
        gfx_color_t c = i == 0 ? GFX_BLACK : i == 15 ? GFX_WHITE : GFX_GRAY(i);
        int16_t x = (int16_t)(RAMP_X + i * RAMP_STEP);
        gfx_fill_rect(fb, (gfx_rect_t){ x, RAMP_Y, RAMP_STEP, RAMP_H }, c);
        char label[4];
        snprintf(label, sizeof(label), "%d", i);
        gfx_text_in_rect(fb, &gfx_font_sans_12, (gfx_rect_t){ x, RAMP_Y + RAMP_H + 2, RAMP_STEP, 16 },
                         GFX_ALIGN_CENTER, label, GFX_BLACK);
    }
    gfx_rect(fb, (gfx_rect_t){ RAMP_X - 1, RAMP_Y - 1, 16 * RAMP_STEP + 2, RAMP_H + 2 }, GFX_BLACK);

    gfx_bitmap(fb, 20, 382, &gfx_icon_t5_thermometer_40, GFX_BLACK);
    gfx_text(fb, &gfx_font_t5_sans_26, 70, 412, "Anti-aliased: Žluťoučký kůň úpěl ďábelské ódy", GFX_BLACK);
    gfx_fill_rect(fb, (gfx_rect_t){ 20, 432, 920, 48 }, GFX_BLACK);
    gfx_bitmap(fb, 20, 436, &gfx_icon_t5_thermometer_40, GFX_WHITE);
    gfx_text(fb, &gfx_font_t5_sans_26, 70, 466, "White on black: 0123456789 °C € …", GFX_WHITE);
}
