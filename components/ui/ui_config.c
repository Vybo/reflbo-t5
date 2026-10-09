#include <stdio.h>
#include <string.h>

#include "gfx_fonts.h"
#include "ui_profile.h"
#include "ui_screens.h"

/* The config-mode screen (spec §5.5, §10.2): a QR code on the left, what to do on the right. */

#define HEADER_H UI_PX(30)
#define FOOTER_Y (fb->height - UI_PX(18))
#define QR_MAX   UI_PX(210) /* leaves the text column about 185 px on the RLCD */
#define AP_IP    "192.168.4.1"

/* Appends `text` to out[*n], with the backslash escapes of the Wi-Fi QR format. */
static void put_escaped(char *out, size_t size, size_t *n, const char *text)
{
    for (const char *c = text; *c != '\0' && *n + 2 < size; c++) {
        if (strchr("\\;,:\"", *c) != NULL) {
            out[(*n)++] = '\\';
        }
        out[(*n)++] = *c;
    }
    out[*n] = '\0';
}

static bool url_available(const ui_config_view_t *v)
{
    return (v->state == UI_NET_STATION && v->ip[0] != '\0') || v->state == UI_NET_AP;
}

bool ui_config_can_switch(const ui_config_view_t *v)
{
    return v->ap_on && url_available(v);
}

ui_qr_kind_t ui_config_qr(const ui_config_view_t *v, char *out, size_t size)
{
    out[0] = '\0';
    if (url_available(v) && (v->qr_url || !v->ap_on)) {
        snprintf(out, size, "http://%s/", v->state == UI_NET_STATION ? v->ip : AP_IP);
        return UI_QR_OPEN;
    }
    if (!v->ap_on) {
        return UI_QR_NONE;
    }
    size_t n = (size_t)snprintf(out, size, "WIFI:T:WPA;S:");
    put_escaped(out, size, &n, v->ap_ssid);
    n += (size_t)snprintf(out + n, size - n, ";P:");
    put_escaped(out, size, &n, v->ap_pass);
    snprintf(out + n, size - n, ";;");
    return UI_QR_JOIN;
}

/* A label over its value, cut to the column; returns the y below them. */
static int draw_pair(gfx_fb_t *fb, int x, int y, int w, const char *label, const char *value)
{
    char fit[96];
    if (label != NULL) {
        gfx_text_ellipsize(UI_FONT(UI_F_SANS_16), label, w, fit, sizeof(fit));
        gfx_text_in_rect(fb, UI_FONT(UI_F_SANS_16), (gfx_rect_t){ (int16_t)x, (int16_t)y, (int16_t)w, (int16_t)UI_PX(20) },
                         GFX_ALIGN_LEFT, fit, GFX_BLACK);
        y += UI_PX(20);
    }
    const gfx_font_t *f = gfx_text_width(UI_FONT(UI_F_BOLD_20), value) <= w ? UI_FONT(UI_F_BOLD_20)
                                                                             : UI_FONT(UI_F_BOLD_16);
    gfx_text_ellipsize(f, value, w, fit, sizeof(fit));
    gfx_text_in_rect(fb, f, (gfx_rect_t){ (int16_t)x, (int16_t)y, (int16_t)w, (int16_t)UI_PX(26) }, GFX_ALIGN_LEFT, fit,
                     GFX_BLACK);
    return y + UI_PX(34);
}

static void draw_header(gfx_fb_t *fb, const ui_config_view_t *v, const lang_t *lang)
{
    gfx_fill_rect(fb, (gfx_rect_t){ 0, 0, fb->width, (int16_t)HEADER_H }, GFX_BLACK);
    char left[40];
    snprintf(left, sizeof(left), "%s %d %s", lang_str(lang, LS_C_CLOSES_IN), v->minutes_left,
             lang_str(lang, LS_MINUTES_UNIT));
    int left_w = gfx_text_width(UI_FONT(UI_F_SANS_16), left);
    gfx_text_in_rect(fb, UI_FONT(UI_F_SANS_16), (gfx_rect_t){ (int16_t)(fb->width - UI_PX(12) - left_w), 0, (int16_t)left_w,
                                                          (int16_t)HEADER_H },
                     GFX_ALIGN_LEFT, left, GFX_WHITE);
    char title[64];
    gfx_text_ellipsize(UI_FONT(UI_F_BOLD_20), lang_str(lang, LS_C_TITLE), fb->width - UI_PX(36) - left_w, title,
                       sizeof(title));
    gfx_text_in_rect(fb, UI_FONT(UI_F_BOLD_20),
                     (gfx_rect_t){ (int16_t)UI_PX(12), 0, (int16_t)(fb->width - UI_PX(36) - left_w), (int16_t)HEADER_H },
                     GFX_ALIGN_LEFT, title, GFX_WHITE);
}

static void draw_hints(gfx_fb_t *fb, const char *hints)
{
    gfx_hline(fb, 0, FOOTER_Y - UI_PX(4), fb->width, GFX_BLACK);
    char fit[96];
    gfx_text_ellipsize(UI_FONT(UI_F_SANS_12), hints, fb->width - UI_PX(12), fit, sizeof(fit));
    gfx_text_in_rect(fb, UI_FONT(UI_F_SANS_12), (gfx_rect_t){ 0, (int16_t)FOOTER_Y, fb->width, (int16_t)(fb->height - FOOTER_Y) },
                     GFX_ALIGN_CENTER, fit, GFX_BLACK);
}

void ui_draw_config(gfx_fb_t *fb, const ui_config_view_t *v, const lang_t *lang)
{
    gfx_reset_clip(fb);
    gfx_clear(fb, GFX_WHITE);
    draw_header(fb, v, lang);
    draw_hints(fb, lang_str(lang, v->back                   ? LS_HINT_CONFIG_BACK
                                  : ui_config_can_switch(v) ? LS_HINT_CONFIG_SWITCH
                                                            : LS_HINT_CONFIG));

    char qr[160], line[64];
    ui_qr_kind_t kind = ui_config_qr(v, qr, sizeof(qr));
    if (kind == UI_QR_NONE) { /* starting, or joining without the AP: nothing to scan yet */
        int mid = (fb->height - UI_PX(300)) / 2; /* the RLCD's screen, centred on a larger one */
        if (v->state == UI_NET_STARTING) {
            gfx_text_in_rect(fb, UI_FONT(UI_F_BOLD_20), (gfx_rect_t){ 0, (int16_t)(mid + UI_PX(130)), fb->width, (int16_t)UI_PX(28) },
                             GFX_ALIGN_CENTER, lang_str(lang, LS_C_STARTING), GFX_BLACK);
            return;
        }
        gfx_text_in_rect(fb, UI_FONT(UI_F_SANS_20), (gfx_rect_t){ 0, (int16_t)(mid + UI_PX(110)), fb->width, (int16_t)UI_PX(28) },
                         GFX_ALIGN_CENTER, lang_str(lang, LS_C_CONNECTING), GFX_BLACK);
        char fit[64];
        snprintf(line, sizeof(line), "%s…", v->ssid);
        gfx_text_ellipsize(UI_FONT(UI_F_BOLD_28), line, fb->width - UI_PX(24), fit, sizeof(fit));
        gfx_text_in_rect(fb, UI_FONT(UI_F_BOLD_28), (gfx_rect_t){ 0, (int16_t)(mid + UI_PX(142)), fb->width, (int16_t)UI_PX(36) },
                         GFX_ALIGN_CENTER, fit, GFX_BLACK);
        return;
    }

    int scale = UI_PX(7); /* a module's pixels, largest first */
    while (scale > 1 && gfx_qr_side(qr, scale) > QR_MAX) {
        scale--;
    }
    int top = HEADER_H + UI_PX(2);
    int side = gfx_qr(fb, 0, top, scale, qr);
    gfx_text_in_rect(fb, UI_FONT(UI_F_BOLD_16), (gfx_rect_t){ 0, (int16_t)(top + side - UI_PX(2)), (int16_t)side, (int16_t)UI_PX(22) },
                     GFX_ALIGN_CENTER, lang_str(lang, kind == UI_QR_JOIN ? LS_C_SCAN_JOIN : LS_C_SCAN_OPEN),
                     GFX_BLACK);

    int x = side + UI_PX(4), w = fb->width - x - UI_PX(6), y = HEADER_H + UI_PX(10);
    if (v->state == UI_NET_JOINING) {
        snprintf(line, sizeof(line), "%s…", v->ssid);
        y = draw_pair(fb, x, y, w, lang_str(lang, LS_C_CONNECTING), line);
    } else if (v->state == UI_NET_STATION) {
        y = draw_pair(fb, x, y, w, lang_str(lang, LS_C_CONNECTED), v->ssid);
    }
    if (kind == UI_QR_JOIN) {
        y = draw_pair(fb, x, y, w, lang_str(lang, LS_C_NETWORK), v->ap_ssid);
        y = draw_pair(fb, x, y, w, lang_str(lang, LS_C_PASSWORD), v->ap_pass);
        draw_pair(fb, x, y, w, lang_str(lang, LS_C_THEN_OPEN), AP_IP);
    } else if (v->state == UI_NET_STATION) {
        snprintf(line, sizeof(line), "%s.local", v->host);
        y = draw_pair(fb, x, y, w, lang_str(lang, LS_C_ADDRESS), line);
        draw_pair(fb, x, y - UI_PX(8), w, NULL, v->ip);
    } else {
        y = draw_pair(fb, x, y, w, lang_str(lang, LS_C_NETWORK), v->ap_ssid);
        draw_pair(fb, x, y, w, lang_str(lang, LS_C_ADDRESS), AP_IP);
    }
}
