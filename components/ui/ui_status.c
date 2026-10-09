#include <stdio.h>
#include <string.h>

#include "gfx_fonts.h"
#include "gfx_icons.h"
#include "ui_internal.h"
#include "ui_profile.h"

/* Status bar (spec §5.2): "Set time" or a stale warning on the left, then a globe while a phone is
 * logged in to the web UI (D20), the sync state and in sync mode `always` the Wi-Fi state; an
 * optional clock in the middle, the battery on the right with its level, voltage or days left as
 * the preset asks. */
void ui_status_draw(gfx_fb_t *fb, const ui_context_t *ctx, const ui_preset_t *preset, bool any_stale)
{
    ui_value_t bat, days;
    ui_resolve(ctx, UI_FIELD_BAT_LEVEL, &bat);
    ui_resolve(ctx, UI_FIELD_BAT_DAYS, &days);
    any_stale |= bat.state == UI_VALUE_STALE; /* the battery shown here counts too */
    int left = UI_PX(4); /* where the next mark on the left goes */
    int step = ui_icon_px(16) + UI_PX(4); /* a mark and the gap after it */
    int mark_y = (UI_STATUS_H - ui_icon_px(16)) / 2;
    if (!ctx->time_valid) {
        const gfx_font_t *f = UI_FONT(UI_F_BOLD_16);
        const char *text = lang_str(ctx->lang, LS_SET_TIME);
        int w = gfx_text_width(f, text) + UI_PX(12);
        gfx_fill_rect(fb, (gfx_rect_t){ 0, 0, (int16_t)w, UI_STATUS_H }, GFX_BLACK);
        gfx_text_in_rect(fb, f, (gfx_rect_t){ (int16_t)UI_PX(6), 0, (int16_t)(w - UI_PX(6)), UI_STATUS_H }, GFX_ALIGN_LEFT,
                         text, GFX_WHITE);
        left = w + UI_PX(4);
    } else if (any_stale) {
        gfx_bitmap(fb, left, mark_y, ui_icon(UI_ICON_stale, UI_IC16), GFX_BLACK);
        left += step;
    }
    if (ctx->web_session) {
        gfx_bitmap(fb, left, mark_y, ui_icon(UI_ICON_web, UI_IC16), GFX_BLACK);
        left += step;
    }
    if (ctx->sync != UI_SYNC_IDLE) { /* spec §5.2: a sync running, or the last one failed */
        gfx_bitmap(fb, left, mark_y, ctx->sync == UI_SYNC_RUNNING ? ui_icon(UI_ICON_sync, UI_IC16) : ui_icon(UI_ICON_sync_failed, UI_IC16),
                   GFX_BLACK);
        left += step;
    }
    if (ctx->wifi != UI_WIFI_NONE) { /* sync mode `always` (D19) */
        gfx_bitmap(fb, left, mark_y, ctx->wifi == UI_WIFI_ON ? ui_icon(UI_ICON_wifi, UI_IC16) : ui_icon(UI_ICON_wifi_off, UI_IC16), GFX_BLACK);
    }

    if (preset->status_clock && ctx->time_valid) {
        ui_value_t t;
        ui_resolve(ctx, UI_FIELD_TIME_CLOCK, &t);
        char clock[sizeof(t.text) + sizeof(t.unit) + 1];
        snprintf(clock, sizeof(clock), "%s%s%s", t.text, t.unit[0] ? " " : "", t.unit);
        gfx_text_in_rect(fb, UI_FONT(UI_F_BOLD_16), (gfx_rect_t){ (int16_t)((fb->width - UI_PX(160)) / 2), 0, (int16_t)UI_PX(160), UI_STATUS_H }, GFX_ALIGN_CENTER,
                         clock, GFX_BLACK);
    }

    int x = fb->width - UI_PX(6) - UI_PX(26);
    ui_draw_battery(fb, x, UI_PX(5), UI_PX(26), UI_PX(11), bat.state == UI_VALUE_MISSING ? -1 : bat.percent);
    if (bat.battery == DS_BAT_CHARGING) {
        const gfx_bitmap_t *bolt = ui_icon(UI_ICON_bolt, UI_IC16);
        x -= bolt->width;
        gfx_bitmap(fb, x, mark_y, bolt, GFX_BLACK);
    } else if (bat.state != UI_VALUE_MISSING && bat.percent <= UI_BATTERY_LOW_PCT) { /* spec §8: low */
        x -= UI_PX(10);
        gfx_text(fb, UI_FONT(UI_F_BOLD_16), x + UI_PX(2), UI_PX(16), "!", GFX_BLACK);
    }
    char text[sizeof(bat.text) + sizeof(bat.extra) + sizeof(days.text) + sizeof(days.unit) + 12] = "";
    size_t n = 0;
    uint8_t parts = preset->status_battery;
    if (bat.state == UI_VALUE_MISSING) {
        snprintf(text, sizeof(text), "\xE2\x80\x94");
    } else {
        if (parts & UI_STATUS_BAT_PERCENT) {
            n += (size_t)snprintf(text + n, sizeof(text) - n, "%s%%", bat.text);
        }
        if ((parts & UI_STATUS_BAT_VOLTAGE) && n < sizeof(text)) {
            n += (size_t)snprintf(text + n, sizeof(text) - n, "%s%s", n ? "  " : "", bat.extra);
        }
        if ((parts & UI_STATUS_BAT_DAYS) && days.state != UI_VALUE_MISSING && n < sizeof(text)) {
            snprintf(text + n, sizeof(text) - n, "%s%s %s", n ? "  " : "", days.text, days.unit);
        }
    }
    char fit[sizeof(text)];
    gfx_text_ellipsize(UI_FONT(UI_F_SANS_12), text, UI_PX(120), fit, sizeof(fit)); /* cut at the end, not the start */
    gfx_text_in_rect(fb, UI_FONT(UI_F_SANS_12), (gfx_rect_t){ (int16_t)(x - UI_PX(124)), 0, (int16_t)UI_PX(120), UI_STATUS_H },
                     GFX_ALIGN_RIGHT, fit, GFX_BLACK);
    gfx_hline(fb, 0, UI_STATUS_H, fb->width, GFX_BLACK);
}
