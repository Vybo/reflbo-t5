#include "ui_visual.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "gfx_fonts.h"
#include "ui_profile.h"
#include "ui_internal.h"

/* Show as (spec §5.3, D44): the four visuals as the spike drew them (approved 2026-10-19), from the resolved value
 * and the chart's history. */

#define CHUNK 50 /* a chart's columns taken at a time, on the stack */

static const ui_font_id_t k_faces[] = { UI_F_NUM_48, UI_F_BOLD_28, UI_F_BOLD_20, UI_F_BOLD_16, UI_F_SANS_12 };

bool ui_visual_fits(ui_vis_as_t as, int w, int h)
{
    switch (as) {
    case UI_VIS_VALUE:
        return true;
    case UI_VIS_CHART:
        return w >= UI_PX(80) && h >= UI_PX(40);
    case UI_VIS_GAUGE:
        return w >= UI_PX(60) && h >= UI_PX(40);
    case UI_VIS_RING:
        return w >= UI_PX(40) && h >= UI_PX(40);
    case UI_VIS_BAR:
        return w >= UI_PX(60) && h >= UI_PX(16);
    default:
        return false;
    }
}

bool ui_visual_span_ok(uint32_t span_s)
{
    return (span_s >= UI_SPAN_MIN_S && span_s <= UI_SPAN_VALUES_MAX_S) ||
           (span_s >= UI_SPAN_HOURS_MIN_S && span_s <= UI_SPAN_MAX_S && span_s % 3600 == 0);
}

void ui_visual_span_text(uint32_t span_s, char *out, size_t size)
{
    if (span_s >= 3600 && span_s % 3600 == 0) {
        snprintf(out, size, "%lu h", (unsigned long)(span_s / 3600));
    } else if (span_s >= 120 && span_s % 60 == 0) {
        snprintf(out, size, "%lu min", (unsigned long)(span_s / 60));
    } else {
        snprintf(out, size, "%lu s", (unsigned long)span_s);
    }
}

/* The round number (1, 2 or 5 times a power of ten) at or past `x` > 0. */
static float nice_up(float x)
{
    float p = powf(10.0f, floorf(log10f(x)));
    float m = x / p;
    return (m <= 1.0f ? 1.0f : m <= 2.0f ? 2.0f : m <= 5.0f ? 5.0f : 10.0f) * p;
}

/* A chart's automatic bounds: the values in view with a tenth's room, on a tenth of a round step. */
static void auto_view(float lo, float hi, float *out_lo, float *out_hi)
{
    float pad = hi > lo ? (hi - lo) * 0.1f : fabsf(hi) > 0.0f ? fabsf(hi) * 0.1f : 1.0f;
    lo -= pad;
    hi += pad;
    float step = nice_up(hi - lo) / 10.0f;
    *out_lo = floorf(lo / step) * step;
    *out_hi = ceilf(hi / step) * step;
}

void ui_visual_range(const ui_visual_t *vis, const ui_value_t *v, bool have_view, float view_lo, float view_hi,
                     float *lo, float *hi)
{
    float alo = 0.0f, ahi = 1.0f; /* automatic */
    if (vis->as == UI_VIS_CHART) {
        if (have_view) {
            auto_view(view_lo, view_hi, &alo, &ahi);
        } else if (v->has_num) {
            auto_view(v->num, v->num, &alo, &ahi);
        }
    } else if (v->has_num && v->num != 0.0f) {
        alo = v->num < 0.0f ? -nice_up(-v->num) : 0.0f;
        ahi = v->num > 0.0f ? nice_up(v->num) : 0.0f;
    }
    *lo = vis->min_mode == UI_LIMIT_VALUE                    ? vis->min
          : vis->min_mode == UI_LIMIT_FIELD && v->has_range ? v->range_min
                                                            : alo;
    *hi = vis->max_mode == UI_LIMIT_VALUE                    ? vis->max
          : vis->max_mode == UI_LIMIT_FIELD && v->has_range ? v->range_max
                                                            : ahi;
    if (!(*hi > *lo)) {
        *hi = *lo + 1.0f;
    }
}

/* ---- text ---- */

static int ascent(const gfx_font_t *f)
{
    return f->ascent;
}

static bool face_has(const gfx_font_t *f, const char *s)
{
    for (const char *p = s; *p;) {
        uint32_t c = gfx_utf8_next(&p);
        if (c != ' ' && !gfx_font_has_glyph(f, c)) {
            return false;
        }
    }
    return true;
}

/* Text with a white halo, UI_PX(1) wide, so it reads over a chart. Returns where it ends. */
static int text_halo(gfx_fb_t *fb, const gfx_font_t *f, int x, int base, const char *s, bool halo)
{
    if (halo) {
        int k = UI_PX(1);
        for (int dy = -k; dy <= k; dy++) {
            for (int dx = -k; dx <= k; dx++) {
                if (dx != 0 || dy != 0) {
                    gfx_text(fb, f, x + dx, base + dy, s, GFX_WHITE);
                }
            }
        }
    }
    return gfx_text(fb, f, x, base, s, GFX_BLACK);
}

/* What a visual writes for its value: the text, its shorter form, and the unit; "—" without a value. */
typedef struct {
    const char *text, *short_text, *unit;
} number_t;

static number_t number_of(const ui_value_t *v)
{
    if (v->state == UI_VALUE_MISSING || !v->text[0]) {
        return (number_t){ "—", "—", "" };
    }
    return (number_t){ v->text, v->short_text[0] ? v->short_text : v->text, v->unit };
}

/* The value and its unit, as large as fits w×h (the unit never below bold 16), then without the unit, then without
 * its decimals, as the spike drew them; the width it takes, drawn when `draw`. 0: nothing fits. */
static int number_fit(gfx_fb_t *fb, int x, int y, int w, int h, const number_t *n, bool draw, gfx_align_t align,
                      bool halo)
{
    for (int pass = 0; pass < 3; pass++) {
        const char *num = pass < 2 ? n->text : n->short_text;
        const char *u = pass == 0 ? n->unit : "";
        if (pass == 2 && strcmp(n->text, n->short_text) == 0) {
            continue;
        }
        for (size_t i = 0; i < sizeof(k_faces) / sizeof(k_faces[0]); i++) {
            const gfx_font_t *f = UI_FONT(k_faces[i]);
            if (!face_has(f, num) || ascent(f) > h || (u[0] && f == UI_FONT(UI_F_SANS_12))) {
                continue;
            }
            const gfx_font_t *uf = ascent(f) >= UI_PX(28) ? UI_FONT(UI_F_SANS_16) : UI_FONT(UI_F_SANS_12);
            int nw = gfx_text_width(f, num), uw = u[0] ? gfx_text_width(uf, u) + UI_PX(3) : 0;
            if (nw + uw > w) {
                continue;
            }
            if (draw) {
                int x0 = align == GFX_ALIGN_LEFT    ? x
                         : align == GFX_ALIGN_RIGHT ? x + w - nw - uw
                                                    : x + (w - nw - uw) / 2;
                int base = y + (h + ascent(f)) / 2;
                int end = text_halo(fb, f, x0, base, num, halo);
                if (u[0]) {
                    text_halo(fb, uf, end + UI_PX(3), base, u, halo);
                }
            }
            return nw + uw;
        }
    }
    return 0;
}

/* The label (the field's, or the slot's own title) at the top left, cut to `max_w`. */
static void label(gfx_fb_t *fb, gfx_rect_t r, const ui_value_t *v, const gfx_font_t *f, int max_w)
{
    const char *text = v->label;
    char fit[64];
    if (text == NULL || !text[0] || max_w <= 0 || gfx_text_ellipsize(f, text, max_w, fit, sizeof(fit)) <= 0) {
        return;
    }
    ui_label(v);
    gfx_text(fb, f, r.x + UI_PX(4), r.y + UI_PX(2) + ascent(f), fit, GFX_BLACK);
}

/* A bound as the visuals write it: whole, else with the decimals it needs (up to two). */
static void bound_text(const lang_t *lang, float x, char *out, size_t size)
{
    int decimals = fabsf(x - roundf(x)) < 1e-3f ? 0 : fabsf(x * 10.0f - roundf(x * 10.0f)) < 1e-2f ? 1 : 2;
    float scale = decimals == 0 ? 1.0f : decimals == 1 ? 10.0f : 100.0f;
    float scaled = x * scale;
    if (fabsf(scaled) < 2e9f) {
        lang_format_decimal(lang, lroundf(scaled), decimals, out, size);
    } else {
        snprintf(out, size, "%.0f", (double)x);
    }
}

static float frac(const ui_value_t *v, float lo, float hi)
{
    if (!v->has_num || v->state == UI_VALUE_MISSING) {
        return 0.0f;
    }
    float f = (v->num - lo) / (hi - lo);
    return f < 0.0f ? 0.0f : f > 1.0f ? 1.0f : f;
}

/* ---- the chart ---- */

static int y_of(gfx_rect_t p, float v, float lo, float hi)
{
    int y = p.y + p.h - 1 - (int)lroundf((v - lo) / (hi - lo) * (float)(p.h - 1));
    return y > p.y + p.h - 1 ? p.y + p.h - 1 : y < p.y ? p.y : y;
}

/* The light fill under a column's line at `y`: a sparse dither on the RLCD, gray 6 on the T5 (T5 spec §6.4). */
static void fill_under(gfx_fb_t *fb, gfx_rect_t p, int x, int y)
{
    if (fb->format == GFX_FMT_4BPP) {
        gfx_vline(fb, x, y + 1, p.y + p.h - (y + 1), GFX_GRAY(6));
        return;
    }
    for (int fy = y + 1; fy < p.y + p.h; fy++) {
        if (x % 2 == 0 && fy % 2 == 0) {
            gfx_pixel(fb, x, fy, GFX_BLACK);
        }
    }
}

/* A column's spread from y0 to y1: half its pixels on the RLCD, gray 3 on the T5. */
static void spread(gfx_fb_t *fb, int x, int y0, int y1)
{
    if (fb->format == GFX_FMT_4BPP) {
        if (y1 >= y0) {
            gfx_vline(fb, x, y0, y1 - y0 + 1, GFX_GRAY(3));
        }
        return;
    }
    for (int y = y0; y <= y1; y++) {
        if ((x + y) % 2 == 0) {
            gfx_pixel(fb, x, y, GFX_BLACK);
        }
    }
}

/* One column of the line at `y`, joined to the column before (`line_y`, -1: none), UI_PX(1) wide and tall and
 * ending at `x`, so the next column's fill doesn't cover it. */
static void plot(gfx_fb_t *fb, int x, int y, int *line_y)
{
    int a = *line_y >= 0 && *line_y < y ? *line_y : y, b = *line_y > y ? *line_y : y;
    ui_vline(fb, x - UI_PX(1) + 1, a, b - a + UI_PX(1), GFX_BLACK);
    *line_y = y;
}

static void chart(gfx_fb_t *fb, gfx_rect_t r, const ui_value_t *v, const ui_visual_t *vis, const ui_series_t *series,
                  uint32_t now_ms, const lang_t *lang)
{
    const gfx_font_t *lf = r.h >= UI_PX(80) ? UI_FONT(UI_F_SANS_16) : UI_FONT(UI_F_SANS_12);
    const gfx_font_t *sf = UI_FONT(UI_F_SANS_12);
    number_t n = number_of(v);
    int top = r.y + UI_PX(2) + ascent(lf) + UI_PX(6); /* the label's line stays clear of the chart */
    gfx_rect_t p = { (int16_t)(r.x + UI_PX(4)), (int16_t)top, (int16_t)(r.w - UI_PX(8)),
                     (int16_t)(r.y + r.h - UI_PX(3) - top) };
    uint32_t span_ms = (ui_visual_span_ok(vis->span_s) ? vis->span_s : UI_SPAN_DEFAULT_S) * 1000u;
    ui_hist_column_t cols[CHUNK];
    bool have = false;
    float vlo = 0.0f, vhi = 0.0f;
    for (int first = 0; series != NULL && first < p.w; first += CHUNK) { /* the values in view, for an auto range */
        int count = p.w - first < CHUNK ? p.w - first : CHUNK;
        ui_series_columns(series, now_ms, span_ms, p.w, first, count, cols);
        for (int i = 0; i < count; i++) {
            if (cols[i].n > 0) {
                vlo = !have || cols[i].min < vlo ? cols[i].min : vlo;
                vhi = !have || cols[i].max > vhi ? cols[i].max : vhi;
                have = true;
            }
        }
    }
    if (!have && (v->state == UI_VALUE_MISSING || !v->text[0])) { /* nothing to show yet: the label and the baseline */
        label(fb, r, v, lf, r.w - UI_PX(8));
        ui_hline(fb, p.x, p.y + p.h, p.w, GFX_BLACK);
        return;
    }
    float lo, hi;
    ui_visual_range(vis, v, have, vlo, vhi, &lo, &hi);
    char lo_s[16], hi_s[16];
    bound_text(lang, lo, lo_s, sizeof(lo_s));
    bound_text(lang, hi, hi_s, sizeof(hi_s));
    int right = vis->numbers ? gfx_text_width(sf, hi_s) + UI_PX(8) /* the range's top */
                             : number_fit(fb, 0, 0, r.w / 2, ascent(lf) + UI_PX(4), &n, false, GFX_ALIGN_RIGHT, false) +
                                   UI_PX(8);
    label(fb, r, v, lf, r.w - UI_PX(8) - right);
    if (vis->numbers) {
        gfx_text(fb, sf, r.x + r.w - UI_PX(4) - gfx_text_width(sf, hi_s), r.y + UI_PX(2) + ascent(lf), hi_s, GFX_BLACK);
    } else { /* the value on the label's line, at the right */
        int hw = right - UI_PX(8);
        number_fit(fb, r.x + r.w - UI_PX(4) - hw, r.y + UI_PX(1), hw, ascent(lf) + UI_PX(4), &n, true, GFX_ALIGN_RIGHT,
                   false);
    }
    int prev_x = -1, prev_y = 0, line_y = -1;
    for (int first = 0; series != NULL && first < p.w; first += CHUNK) {
        int count = p.w - first < CHUNK ? p.w - first : CHUNK;
        ui_series_columns(series, now_ms, span_ms, p.w, first, count, cols);
        for (int i = 0; i < count; i++) {
            const ui_hist_column_t *c = &cols[i];
            if (c->n == 0) {
                continue;
            }
            int px = p.x + first + i;
            int ym = y_of(p, c->mean, lo, hi), ylo = y_of(p, c->min, lo, hi), yhi = y_of(p, c->max, lo, hi);
            if (c->brk) {
                line_y = -1; /* the line starts again */
            } else if (prev_x >= 0) {
                for (int x = prev_x + 1; x < px; x++) { /* across the columns without a value, straight */
                    int y = prev_y + (ym - prev_y) * (x - prev_x) / (px - prev_x);
                    fill_under(fb, p, x, y);
                    plot(fb, x, y, &line_y);
                }
            }
            fill_under(fb, p, px, ym);
            spread(fb, px, yhi, ym - 1); /* the column's range above the mean, half grey */
            spread(fb, px, ym + 1, ylo); /* and below it */
            plot(fb, px, ym, &line_y);
            prev_x = px;
            prev_y = ym;
        }
    }
    ui_hline(fb, p.x, p.y + p.h, p.w, GFX_BLACK); /* the baseline */
    if (vis->numbers) { /* the latest value over the chart's top left, the range's bottom at the right */
        int nh = p.h >= UI_PX(130) ? UI_PX(52) : p.h >= UI_PX(60) ? UI_PX(30) : p.h >= UI_PX(40) ? UI_PX(22) : UI_PX(18);
        number_fit(fb, p.x + UI_PX(1), p.y + UI_PX(1), p.w * 2 / 3, nh, &n, true, GFX_ALIGN_LEFT, true);
        text_halo(fb, sf, p.x + p.w - gfx_text_width(sf, lo_s) - UI_PX(1), p.y + p.h - UI_PX(2), lo_s, true);
    }
    if (p.h >= UI_PX(40)) { /* the span, small, at the bottom left */
        char span[16];
        ui_visual_span_text(span_ms / 1000u, span, sizeof(span));
        text_halo(fb, sf, p.x + UI_PX(1), p.y + p.h - UI_PX(2), span, true);
    }
}

/* ---- the bar ---- */

static void bar(gfx_fb_t *fb, gfx_rect_t r, const ui_value_t *v, float lo, float hi)
{
    number_t n = number_of(v);
    float f = frac(v, lo, hi);
    int k = UI_PX(1); /* the outline's width */
    if (r.h > r.w * 3 / 2) { /* upright in a tall cell: the value under it */
        label(fb, r, v, UI_FONT(UI_F_SANS_12), r.w - UI_PX(8));
        int nh = UI_PX(22), top = r.y + UI_PX(4) + ascent(UI_FONT(UI_F_SANS_12)) + UI_PX(2);
        gfx_rect_t b = { (int16_t)(r.x + r.w / 2 - UI_PX(10)), (int16_t)top, (int16_t)UI_PX(20),
                         (int16_t)(r.y + r.h - nh - UI_PX(4) - top) };
        ui_frame(fb, b, GFX_BLACK);
        int fill = (int)lroundf(f * (float)(b.h - 2 * k));
        gfx_fill_rect(fb, (gfx_rect_t){ (int16_t)(b.x + k), (int16_t)(b.y + b.h - k - fill), (int16_t)(b.w - 2 * k),
                                        (int16_t)fill },
                      GFX_BLACK);
        number_fit(fb, r.x + UI_PX(2), r.y + r.h - nh - UI_PX(2), r.w - UI_PX(4), nh, &n, true, GFX_ALIGN_CENTER, false);
        return;
    }
    gfx_rect_t b;
    if (r.h < UI_PX(44)) { /* one line: the bar, then the value */
        int nw = number_fit(fb, 0, 0, r.w / 2, r.h - UI_PX(4), &n, false, GFX_ALIGN_LEFT, false);
        int bh = r.h - UI_PX(8) > UI_PX(12) ? UI_PX(12) : r.h - UI_PX(8);
        b = (gfx_rect_t){ (int16_t)(r.x + UI_PX(4)), (int16_t)(r.y + (r.h - bh) / 2), (int16_t)(r.w - UI_PX(12) - nw),
                          (int16_t)bh };
        number_fit(fb, b.x + b.w + UI_PX(4), r.y + UI_PX(2), nw, r.h - UI_PX(4), &n, true, GFX_ALIGN_LEFT, false);
    } else { /* the label and the value on a line, the bar under them */
        const gfx_font_t *lf = r.h >= UI_PX(70) ? UI_FONT(UI_F_SANS_16) : UI_FONT(UI_F_SANS_12);
        int hh = r.h >= UI_PX(70) ? UI_PX(32) : UI_PX(22);
        int lw = v->label != NULL ? gfx_text_width(lf, v->label) : 0; /* the label first, the value in the rest */
        lw = lw > (r.w - UI_PX(8)) * 3 / 4 ? (r.w - UI_PX(8)) * 3 / 4 : lw;
        label(fb, (gfx_rect_t){ r.x, (int16_t)(r.y + hh - ascent(lf) - UI_PX(4)), r.w, r.h }, v, lf, lw);
        lw += lw > 0 ? UI_PX(12) : 0;
        number_fit(fb, r.x + lw, r.y + UI_PX(2), r.w - UI_PX(4) - lw, hh, &n, true, GFX_ALIGN_RIGHT, false);
        int room = r.y + r.h - UI_PX(6) - (r.y + hh + UI_PX(4));
        int bh = room > UI_PX(18) ? UI_PX(18) : room;
        b = (gfx_rect_t){ (int16_t)(r.x + UI_PX(4)), (int16_t)(r.y + hh + UI_PX(4) + (room - bh) / 2),
                          (int16_t)(r.w - UI_PX(8)), (int16_t)bh };
    }
    ui_frame(fb, b, GFX_BLACK);
    int fill = (int)lroundf(f * (float)(b.w - 2 * k));
    gfx_fill_rect(fb, (gfx_rect_t){ (int16_t)(b.x + k), (int16_t)(b.y + k), (int16_t)fill, (int16_t)(b.h - 2 * k) },
                  GFX_BLACK);
}

/* ---- the ring and the gauge ---- */

/* Angles: 0 at 12 o'clock, clockwise, in degrees; a0 below -180 for a full ring. */
static void arc_band(gfx_fb_t *fb, int cx, int cy, float ro, float ri, float a0, float a1, float filled_to)
{
    for (int y = (int)((float)cy - ro - 1.0f); y <= (int)((float)cy + ro + 1.0f); y++) {
        for (int x = (int)((float)cx - ro - 1.0f); x <= (int)((float)cx + ro + 1.0f); x++) {
            float dx = (float)x + 0.5f - (float)cx, dy = (float)y + 0.5f - (float)cy;
            float d = sqrtf(dx * dx + dy * dy);
            if (d > ro || d < ri) {
                continue;
            }
            float a = atan2f(dx, -dy) * 57.29578f; /* -180..180 */
            if (a0 >= -180.0f) {
                if (a < a0 || a > a1) {
                    continue;
                }
            } else if (a < 0.0f) {
                a += 360.0f;
            }
            bool edge = d > ro - (float)UI_PX(1) || d < ri + (float)UI_PX(1); /* the outline, UI_PX(1) wide */
            if (a <= filled_to || edge) {
                gfx_pixel(fb, x, y, GFX_BLACK);
            }
        }
    }
}

static void ring(gfx_fb_t *fb, gfx_rect_t r, const ui_value_t *v, float lo, float hi)
{
    number_t n = number_of(v);
    float f = frac(v, lo, hi);
    const gfx_font_t *lf = r.h >= UI_PX(80) ? UI_FONT(UI_F_SANS_16) : UI_FONT(UI_F_SANS_12);
    int lh = ascent(lf) + UI_PX(4);
    label(fb, r, v, lf, r.w - UI_PX(8));
    int ah = r.h - lh - UI_PX(4), aw = r.w - UI_PX(8), ay = r.y + lh + UI_PX(2);
    int d = ah < aw ? ah : aw;
    float t_min = (float)UI_PX(4);
    float ro = (float)d / 2.0f, t = ro / 5.0f < t_min ? t_min : ro / 5.0f;
    int inner = (int)((ro - t) * 1.35f);
    if (number_fit(fb, 0, 0, inner, inner * 2 / 3, &n, false, GFX_ALIGN_CENTER, false) > 0) {
        int cx = r.x + r.w / 2, cy = ay + ah / 2;
        arc_band(fb, cx, cy, ro, ro - t, -999.0f, 999.0f, f * 360.0f);
        number_fit(fb, cx - inner / 2, cy - inner / 3, inner, inner * 2 / 3, &n, true, GFX_ALIGN_CENTER, false);
        return;
    }
    /* beside: a smaller ring at the left (a pie under 30 px), the value right of it */
    d = ah < aw / 2 ? ah : aw / 2;
    ro = (float)d / 2.0f;
    int cx = r.x + UI_PX(4) + d / 2, cy = ay + ah / 2;
    if (d < UI_PX(30)) {
        ui_ring(fb, cx, cy, d / 2, GFX_BLACK);
        arc_band(fb, cx, cy, ro, 0.0f, -999.0f, 999.0f, f * 360.0f);
    } else {
        t = ro / 4.0f < t_min ? t_min : ro / 4.0f;
        arc_band(fb, cx, cy, ro, ro - t, -999.0f, 999.0f, f * 360.0f);
    }
    int nx = cx + d / 2 + UI_PX(4);
    number_fit(fb, nx, cy - UI_PX(15), r.x + r.w - nx - UI_PX(2), UI_PX(30), &n, true, GFX_ALIGN_LEFT, false);
}

static void gauge(gfx_fb_t *fb, gfx_rect_t r, const ui_value_t *v, float lo, float hi, const lang_t *lang)
{
    number_t n = number_of(v);
    float f = frac(v, lo, hi);
    const gfx_font_t *lf = r.h >= UI_PX(80) ? UI_FONT(UI_F_SANS_16) : UI_FONT(UI_F_SANS_12);
    const gfx_font_t *sf = UI_FONT(UI_F_SANS_12);
    int lh = ascent(lf) + UI_PX(4);
    label(fb, r, v, lf, r.w - UI_PX(8));
    bool ends = r.h >= UI_PX(70);
    int ah = r.h - lh - UI_PX(4) - (ends ? UI_PX(4) : 0), aw = r.w - UI_PX(8);
    float ro = (float)aw / 2.0f;
    ro = ro * 1.5f > (float)ah ? (float)ah / 1.5f : ro;
    float t_min = (float)UI_PX(4);
    float t = ro / 5.0f < t_min ? t_min : ro / 5.0f;
    int spare = ah - (int)(ro * 1.5f); /* a tall cell: the arc in its middle */
    int cx = r.x + r.w / 2, cy = r.y + lh + UI_PX(2) + (spare > 0 ? spare / 2 : 0) + (int)ro + UI_PX(1);
    arc_band(fb, cx, cy, ro, ro - t, -120.0f, 120.0f, -120.0f + f * 240.0f);
    int iw = (int)((ro - t) * 1.6f), ih = (int)(ro * 0.85f);
    if (number_fit(fb, cx - iw / 2, cy - ih / 2, iw, ih, &n, true, GFX_ALIGN_CENTER, false) == 0) {
        number_fit(fb, cx - (int)ro, cy + (int)(ro * 0.5f) - UI_PX(18), (int)(2.0f * ro), UI_PX(18), &n, true,
                   GFX_ALIGN_CENTER, false);
    }
    if (ends) { /* the range's ends under the arc's ends */
        char lo_s[16], hi_s[16];
        bound_text(lang, lo, lo_s, sizeof(lo_s));
        bound_text(lang, hi, hi_s, sizeof(hi_s));
        int ey = cy + (int)(ro * 0.5f) + ascent(sf) + UI_PX(1);
        ey = ey > r.y + r.h - UI_PX(2) ? r.y + r.h - UI_PX(2) : ey;
        int lw = gfx_text_width(sf, lo_s), hw = gfx_text_width(sf, hi_s);
        int lx = cx - (int)(ro * 0.87f) - lw / 2, hx = cx + (int)(ro * 0.87f) - hw / 2;
        lx = lx < r.x + UI_PX(1) ? r.x + UI_PX(1) : lx; /* inside the cell, beside the arc's ends */
        hx = hx + hw > r.x + r.w - UI_PX(1) ? r.x + r.w - UI_PX(1) - hw : hx;
        gfx_text(fb, sf, lx, ey, lo_s, GFX_BLACK);
        gfx_text(fb, sf, hx, ey, hi_s, GFX_BLACK);
    }
}

void ui_visual_draw(gfx_fb_t *fb, gfx_rect_t r, const ui_value_t *v, const ui_visual_t *vis, const ui_series_t *series,
                    uint32_t now_ms, const lang_t *lang)
{
    if (!ui_visual_fits((ui_vis_as_t)vis->as, r.w, r.h) || vis->as == UI_VIS_VALUE) {
        return;
    }
    gfx_set_clip(fb, r);
    float lo, hi;
    ui_visual_range(vis, v, false, 0, 0, &lo, &hi);
    switch (vis->as) {
    case UI_VIS_CHART:
        chart(fb, r, v, vis, series, now_ms, lang);
        break;
    case UI_VIS_BAR:
        bar(fb, r, v, lo, hi);
        break;
    case UI_VIS_RING:
        ring(fb, r, v, lo, hi);
        break;
    case UI_VIS_GAUGE:
        gauge(fb, r, v, lo, hi, lang);
        break;
    default:
        break;
    }
    gfx_reset_clip(fb);
}
