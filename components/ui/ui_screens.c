#include "ui_screens.h"

#include <stdio.h>

#include "gfx_fonts.h"
#include "ui_internal.h"
#include "ui_profile.h"

void ui_draw_toast(gfx_fb_t *fb, const char *text)
{
    const gfx_font_t *f = UI_FONT(UI_F_BOLD_20);
    char fit[96];
    int w = gfx_text_ellipsize(f, text, fb->width - UI_PX(60), fit, sizeof(fit)) + UI_PX(40);
    gfx_rect_t box = { (int16_t)((fb->width - w) / 2), (int16_t)(fb->height - UI_PX(62)), (int16_t)w,
                       (int16_t)UI_PX(44) };
    gfx_reset_clip(fb);
    /* a white rim keeps the box visible over black content, such as an inverted preset */
    int rim = UI_PX(3);
    gfx_fill_rect(fb, (gfx_rect_t){ (int16_t)(box.x - rim), (int16_t)(box.y - rim), (int16_t)(box.w + 2 * rim),
                                    (int16_t)(box.h + 2 * rim) },
                  GFX_WHITE);
    gfx_fill_rect(fb, box, GFX_BLACK);
    gfx_text_in_rect(fb, f, box, GFX_ALIGN_CENTER, fit, GFX_WHITE);
}

void ui_draw_critical(gfx_fb_t *fb, const ui_context_t *ctx)
{
    gfx_reset_clip(fb);
    gfx_clear(fb, GFX_WHITE);
    int top = (fb->height - UI_PX(300)) / 2; /* the RLCD's screen, centred on a larger one */
    ui_draw_battery(fb, (fb->width - UI_PX(140)) / 2, top + UI_PX(44), UI_PX(140), UI_PX(64), 0);
    gfx_text_in_rect(fb, UI_FONT(UI_F_BOLD_28),
                     (gfx_rect_t){ 0, (int16_t)(top + UI_PX(132)), fb->width, (int16_t)UI_PX(36) }, GFX_ALIGN_CENTER,
                     lang_str(ctx->lang, LS_BATTERY_EMPTY), GFX_BLACK);
    gfx_text_in_rect(fb, UI_FONT(UI_F_SANS_20),
                     (gfx_rect_t){ 0, (int16_t)(top + UI_PX(172)), fb->width, (int16_t)UI_PX(28) }, GFX_ALIGN_CENTER,
                     lang_str(ctx->lang, LS_CHARGE_ME), GFX_BLACK);
    if (ctx->time_valid) { /* when the screen was drawn: it stays up while the battery recovers */
        ui_value_t t, d;
        ui_resolve(ctx, UI_FIELD_TIME_CLOCK, &t);
        ui_resolve(ctx, UI_FIELD_DATE_DAY, &d);
        char line[sizeof(t.text) + sizeof(t.unit) + sizeof(d.extra) + 8];
        snprintf(line, sizeof(line), "%s%s%s \xC2\xB7 %s", t.text, t.unit[0] ? " " : "", t.unit, d.extra);
        gfx_text_in_rect(fb, UI_FONT(UI_F_SANS_16),
                         (gfx_rect_t){ 0, (int16_t)(top + UI_PX(244)), fb->width, (int16_t)UI_PX(24) },
                         GFX_ALIGN_CENTER, line, GFX_BLACK);
    }
}

void ui_draw_first_run(gfx_fb_t *fb, const ui_context_t *ctx)
{
    gfx_reset_clip(fb);
    gfx_clear(fb, GFX_WHITE);
    int header = UI_PX(30), margin = UI_PX(12);
    gfx_fill_rect(fb, (gfx_rect_t){ 0, 0, fb->width, (int16_t)header }, GFX_BLACK);
    gfx_text_in_rect(fb, UI_FONT(UI_F_BOLD_20),
                     (gfx_rect_t){ (int16_t)margin, 0, (int16_t)(fb->width - 2 * margin), (int16_t)header },
                     GFX_ALIGN_LEFT, lang_str(ctx->lang, LS_F_TITLE), GFX_WHITE);
    int y = UI_PX(76);
    if (ctx->time_valid) { /* spec §5.5: the clock, when there is a time to show */
        ui_value_t t;
        ui_resolve(ctx, UI_FIELD_TIME_CLOCK, &t);
        const gfx_font_t *f = UI_FONT(UI_F_NUM_72);
        int w = gfx_text_width(f, t.text);
        int suffix_w = t.unit[0] ? gfx_text_width(UI_FONT(UI_F_BOLD_20), t.unit) + UI_PX(6) : 0;
        int x = gfx_text(fb, f, (fb->width - w - suffix_w) / 2, UI_PX(110), t.text, GFX_BLACK);
        if (suffix_w) {
            gfx_text(fb, UI_FONT(UI_F_BOLD_20), x + UI_PX(6), UI_PX(110), t.unit, GFX_BLACK);
        }
        y = UI_PX(138);
    }
    const lang_str_t hints[] = { LS_F_WIFI, LS_F_MENU, LS_F_CONTINUE };
    for (int i = 0; i < 3; i++) {
        const gfx_font_t *f = i == 2 ? UI_FONT(UI_F_SANS_20) : UI_FONT(UI_F_BOLD_20);
        char line1[96], line2[96];
        ui_split_two_lines(f, lang_str(ctx->lang, hints[i]), fb->width - 2 * margin, line1, line2, sizeof(line1));
        gfx_text_in_rect(fb, f, (gfx_rect_t){ 0, (int16_t)y, fb->width, (int16_t)UI_PX(28) }, GFX_ALIGN_CENTER, line1,
                         GFX_BLACK);
        y += UI_PX(26);
        if (line2[0] != '\0') {
            gfx_text_in_rect(fb, f, (gfx_rect_t){ 0, (int16_t)y, fb->width, (int16_t)UI_PX(28) }, GFX_ALIGN_CENTER,
                             line2, GFX_BLACK);
            y += UI_PX(26);
        }
        y += UI_PX(12);
    }
}
