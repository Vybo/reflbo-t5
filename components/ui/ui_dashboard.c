#include "ui_dashboard.h"

#include "ui_internal.h"
#include "ui_radar.h"
#include "ui_solar.h"
#include "ui_split.h"

/* The fixed layout's lines, from the board's profile (T3a). */
static void draw_separators(gfx_fb_t *fb, const ui_layout_t *layout)
{
    for (int i = 0; i < layout->sep_count; i++) {
        const ui_sep_t *sep = &layout->seps[i];
        if (sep->vertical) {
            gfx_vline(fb, sep->x, sep->y, sep->len, GFX_BLACK);
        } else {
            gfx_hline(fb, sep->x, sep->y, sep->len, GFX_BLACK);
        }
    }
}

bool ui_draw_cell(gfx_fb_t *fb, gfx_rect_t cell, const ui_context_t *ctx, ui_field_id_t field,
                  ui_stale_policy_t policy)
{
    const ui_field_info_t *info = ui_field_info(field);
    int size = info != NULL ? ui_split_field_size(info->kind, cell.w, cell.h) : -1;
    if (size < 0) {
        return false;
    }
    ui_value_t v;
    ui_resolve(ctx, field, &v);
    ui_widget_draw(fb, cell, (ui_size_t)size, &v, policy, ctx->lang);
    return v.state == UI_VALUE_STALE;
}

/* The split layout (spec §5.2): each split's separator unless it is hidden, then each cell's field.
 * Returns whether any value shown is stale. */
static bool draw_split(gfx_fb_t *fb, const ui_context_t *ctx, const ui_preset_t *preset)
{
    ui_split_geometry_t g;
    if (!ui_split_layout(preset->split, ui_split_area(), &g)) {
        return false; /* presets.json checks its trees: nothing to draw for one cut short */
    }
    for (int i = 0; i < g.lines; i++) {
        const ui_split_line_t *l = &g.line[i];
        if (l->node & UI_SPLIT_NO_LINE) {
            continue;
        }
        if (l->node & UI_SPLIT_COLUMNS) {
            gfx_vline(fb, l->at, l->rect.y + UI_SPLIT_INSET, l->rect.h - 2 * UI_SPLIT_INSET, GFX_BLACK);
        } else {
            gfx_hline(fb, l->rect.x + UI_SPLIT_INSET, l->at, l->rect.w - 2 * UI_SPLIT_INSET, GFX_BLACK);
        }
    }
    bool any_stale = false;
    for (int i = 0; i < g.cells; i++) {
        any_stale |= ui_draw_cell(fb, g.cell[i], ctx, (ui_field_id_t)preset->slots[i],
                                  (ui_stale_policy_t)preset->stale_policy);
    }
    return any_stale;
}

void ui_draw_dashboard(gfx_fb_t *fb, const ui_context_t *ctx, const ui_preset_t *preset)
{
    ui_context_t c = *ctx;
    if (preset->clock != UI_CLOCK_DEFAULT) {
        c.clock_24h = preset->clock == UI_CLOCK_24H;
    }
    c.seconds = preset->seconds;

    gfx_reset_clip(fb);
    gfx_clear(fb, GFX_WHITE);
    const ui_layout_t *layout = ui_layout((ui_layout_id_t)preset->layout);
    bool any_stale = false;
    gfx_rect_t below = { 0, UI_STATUS_H + 1, fb->width, (int16_t)(fb->height - UI_STATUS_H - 1) };
    if (preset->layout == UI_LAYOUT_RADAR) { /* spec §5.2: the map under the status bar */
        ui_draw_radar_view(fb, below, &c);
    } else if (preset->layout == UI_LAYOUT_FLIGHTS) {
        ui_draw_flights_view(fb, below, &c);
    } else if (preset->layout == UI_LAYOUT_SPLIT) {
        any_stale = draw_split(fb, &c, preset);
    } else if (preset->layout == UI_LAYOUT_SOLAR) { /* M6d */
        any_stale = ui_draw_solar_layout(fb, below, &c);
    } else if (preset->layout == UI_LAYOUT_ENERGY) {
        any_stale = ui_draw_energy_layout(fb, below, &c);
    } else if (layout != NULL) {
        draw_separators(fb, layout);
        for (int i = 0; i < layout->slot_count; i++) {
            ui_value_t v;
            ui_resolve(&c, (ui_field_id_t)preset->slots[i], &v);
            any_stale |= v.state == UI_VALUE_STALE;
            ui_widget_draw(fb, layout->slots[i].rect, layout->slots[i].size, &v,
                           (ui_stale_policy_t)preset->stale_policy, c.lang);
        }
    }
    ui_status_draw(fb, &c, preset, any_stale);
    if (preset->invert) {
        gfx_clear(fb, GFX_INVERT);
    }
}
