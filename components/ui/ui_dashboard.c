#include "ui_dashboard.h"

#include "ui_internal.h"
#include "ui_profile.h"
#include "ui_radar.h"
#include "ui_solar.h"
#include "ui_split.h"

/* The fixed layout's lines, from the board's profile (T3a). */
static void draw_separators(gfx_fb_t *fb, const ui_layout_t *layout)
{
    for (int i = 0; i < layout->sep_count; i++) {
        const ui_sep_t *sep = &layout->seps[i];
        for (int k = 0; k < UI_PX(1); k++) { /* the T5's 2 px (owner, board check) */
            if (sep->vertical) {
                gfx_vline(fb, sep->x + k, sep->y, sep->len, GFX_BLACK);
            } else {
                gfx_hline(fb, sep->x, sep->y + k, sep->len, GFX_BLACK);
            }
        }
    }
}

bool ui_draw_cell(gfx_fb_t *fb, gfx_rect_t cell, const ui_context_t *ctx, ui_field_id_t field,
                  ui_stale_policy_t policy)
{
    return ui_draw_titled_cell(fb, cell, ctx, field, policy, NULL, false);
}

/* Draws `text` in `f` with its baseline at y, cut to max_w, clipped to the cell. */
static void title_text(gfx_fb_t *fb, gfx_rect_t cell, const gfx_font_t *f, int x, int y, const char *text)
{
    gfx_rect_t saved = fb->clip;
    gfx_set_clip(fb, gfx_rect_intersect(saved, cell));
    gfx_text(fb, f, x, y, text, GFX_BLACK);
    fb->clip = saved;
}

/* A forced title where the field shows no label (T5 spec §7.5): a line above the field where the rest of the cell
 * still fits it, else beside it, cut to half the cell. Draws the title and returns the field's part with its size in
 * *size; the whole cell, with nothing drawn, where neither has room. */
static gfx_rect_t place_title(gfx_fb_t *fb, gfx_rect_t cell, const ui_value_t *v, ui_stale_policy_t policy,
                              const lang_t *lang, int *size)
{
    char fit[48];
    const gfx_font_t *lf = UI_FONT(UI_F_SANS_12); /* the M size's label */
    int line = UI_PX(6) + lf->line_height;
    gfx_rect_t below = { cell.x, (int16_t)(cell.y + line), cell.w, (int16_t)(cell.h - line) };
    int s = below.h > 0 ? ui_split_field_size(v->kind, below.w, below.h) : -1;
    if (s >= 0 && !ui_widget_labelled(fb, below, (ui_size_t)s, v, policy, lang)) {
        gfx_text_ellipsize(lf, v->label, cell.w - UI_PX(12), fit, sizeof(fit));
        title_text(fb, cell, lf, cell.x + UI_PX(6), cell.y + UI_PX(6) + lf->ascent, fit);
        *size = s;
        return below;
    }
    const gfx_font_t *bf = UI_FONT(UI_F_SANS_16)->line_height <= cell.h ? UI_FONT(UI_F_SANS_16) : lf;
    int max_w = cell.w / 2 - UI_PX(12);
    if (max_w > 0 && gfx_text_ellipsize(bf, v->label, max_w, fit, sizeof(fit)) > 0) {
        int column = UI_PX(6) + gfx_text_width(bf, fit) + UI_PX(6);
        gfx_rect_t beside = { (int16_t)(cell.x + column), cell.y, (int16_t)(cell.w - column), cell.h };
        s = ui_split_field_size(v->kind, beside.w, beside.h);
        if (s >= 0 && !ui_widget_labelled(fb, beside, (ui_size_t)s, v, policy, lang)) {
            title_text(fb, cell, bf, cell.x + UI_PX(6), cell.y + (cell.h + ui_ink_above(bf, fit) - ui_ink_below(bf, fit)) / 2,
                       fit);
            *size = s;
            return beside;
        }
    }
    return cell;
}

bool ui_draw_titled_cell(gfx_fb_t *fb, gfx_rect_t cell, const ui_context_t *ctx, ui_field_id_t field,
                         ui_stale_policy_t policy, const char *title, bool force)
{
    const ui_field_info_t *info = ui_field_info(field);
    int size = info != NULL ? ui_split_field_size(info->kind, cell.w, cell.h) : -1;
    if (size < 0) {
        return false;
    }
    ui_value_t v;
    ui_resolve(ctx, field, &v);
    if (title != NULL && title[0]) {
        v.label = title;
    }
    gfx_rect_t body = cell;
    if (force && v.label != NULL && v.label[0] && !ui_widget_hidden(&v, policy) &&
        !ui_widget_labelled(fb, cell, (ui_size_t)size, &v, policy, ctx->lang)) {
        body = place_title(fb, cell, &v, policy, ctx->lang, &size);
    }
    ui_widget_draw(fb, body, (ui_size_t)size, &v, policy, ctx->lang);
    return v.state == UI_VALUE_STALE;
}

/* The split layout (spec §5.2): each split's separator unless it is hidden, then each cell's field.
 * Returns whether any value shown is stale. */
static bool draw_split(gfx_fb_t *fb, const ui_context_t *ctx, const ui_preset_t *preset, const ui_titles_t *titles)
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
        for (int k = 0; k < UI_PX(1); k++) { /* the T5's 2 px (owner, board check) */
            if (l->node & UI_SPLIT_COLUMNS) {
                gfx_vline(fb, l->at + k, l->rect.y + UI_SPLIT_INSET, l->rect.h - 2 * UI_SPLIT_INSET, GFX_BLACK);
            } else {
                gfx_hline(fb, l->rect.x + UI_SPLIT_INSET, l->at + k, l->rect.w - 2 * UI_SPLIT_INSET, GFX_BLACK);
            }
        }
    }
    bool any_stale = false;
    for (int i = 0; i < g.cells; i++) {
        any_stale |= ui_draw_titled_cell(fb, g.cell[i], ctx, (ui_field_id_t)preset->slots[i],
                                         (ui_stale_policy_t)preset->stale_policy, titles ? titles->text[i] : NULL,
                                         preset->force_title & (1u << i));
    }
    return any_stale;
}

void ui_draw_dashboard(gfx_fb_t *fb, const ui_context_t *ctx, const ui_preset_t *preset, const ui_titles_t *titles)
{
    ui_context_t c = *ctx;
    if (preset->clock != UI_CLOCK_DEFAULT) {
        c.clock_24h = preset->clock == UI_CLOCK_24H;
    }
    c.seconds = ui_preset_every_second(preset); /* e-paper: hours and minutes (T5 spec §5.3) */

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
        any_stale = draw_split(fb, &c, preset, titles);
    } else if (preset->layout == UI_LAYOUT_SOLAR) { /* M6d */
        any_stale = ui_draw_solar_layout(fb, below, &c);
    } else if (preset->layout == UI_LAYOUT_ENERGY) {
        any_stale = ui_draw_energy_layout(fb, below, &c);
    } else if (layout != NULL) {
        draw_separators(fb, layout);
        for (int i = 0; i < layout->slot_count; i++) {
            ui_value_t v;
            ui_resolve(&c, (ui_field_id_t)preset->slots[i], &v);
            if (titles != NULL && titles->text[i] != NULL && titles->text[i][0]) {
                v.label = titles->text[i]; /* the slot's own title (T5 spec §7.5) */
            }
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

bool ui_preset_every_second(const ui_preset_t *preset)
{
    return preset->seconds && !(ui_profile()->caps & UI_CAP_EPAPER);
}
