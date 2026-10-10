#include "gfx_fonts.h"
#include "gfx_icons.h"
#include "ui_layout.h"
#include "ui_profile.h"

/* The Waveshare RLCD-4.2 (spec §5): 400×300, 1 bpp, the geometry the UI had before T3a, value for value. */

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

static const gfx_font_t *const k_fonts[UI_F_COUNT] = {
    [UI_F_SANS_12] = &gfx_font_sans_12,
    [UI_F_SANS_16] = &gfx_font_sans_16,
    [UI_F_SANS_20] = &gfx_font_sans_20,
    [UI_F_BOLD_16] = &gfx_font_sans_bold_16,
    [UI_F_BOLD_20] = &gfx_font_sans_bold_20,
    [UI_F_BOLD_28] = &gfx_font_sans_bold_28,
    [UI_F_NUM_48] = &gfx_font_num_cb_48,
    [UI_F_NUM_72] = &gfx_font_num_cb_72,
    [UI_F_NUM_110] = &gfx_font_num_cb_110,
    [UI_F_NUM_130] = &gfx_font_num_cb_130,
};

/* Every name at 16, 24 and 48 px; the bolt and the stale mark have no 48, the status bar's marks only 16. */
#define I3(n) [UI_ICON_##n] = { &gfx_icon_##n##_16, &gfx_icon_##n##_24, &gfx_icon_##n##_48 },
#define I2(n) [UI_ICON_##n] = { &gfx_icon_##n##_16, &gfx_icon_##n##_24, &gfx_icon_##n##_24 },
#define I1(n) [UI_ICON_##n] = { &gfx_icon_##n##_16, &gfx_icon_##n##_16, &gfx_icon_##n##_16 },
static const gfx_bitmap_t *const k_icons[UI_ICON_COUNT][UI_IC_CLASSES] = {
    I3(thermometer)
    I3(drop)
    I3(dew)
    I2(bolt)
    I2(stale)
    I3(clock)
    I3(calendar)
    I3(person)
    I3(celebration)
    I3(cloud)
    I1(web)
    I1(sync)
    I1(sync_failed)
    I1(wifi)
    I1(wifi_off)
    I3(air)
    I3(particles)
    I3(uv)
    I3(pollen)
    I3(forecast)
    I3(solar)
    I3(house)
    I3(grid)
    I3(self_use)
    I3(wx_clear_day)
    I3(wx_clear_night)
    I3(wx_mainly_day)
    I3(wx_mainly_night)
    I3(wx_partly_day)
    I3(wx_partly_night)
    I3(wx_overcast)
    I3(wx_fog)
    I3(wx_drizzle)
    I3(wx_rain)
    I3(wx_freezing)
    I3(wx_snow)
    I3(wx_showers_day)
    I3(wx_showers_night)
    I3(wx_snow_showers_day)
    I3(wx_snow_showers_night)
    I3(wx_thunder)
    I3(wx_unknown)
    I3(sunrise)
    I3(sunset)
};

/* Slot rectangles for 400×300 below the 20 px status bar (spec §5.2), tuned on host renders. */
static const ui_slot_t k_classic[] = {
    { "main", { 0, 21, 400, 125 }, UI_SIZE_XL, UI_KINDS_XL },
    { "sub", { 0, 146, 400, 40 }, UI_SIZE_M, UI_KIND(UI_FK_DATE) | UI_KIND(UI_FK_TEXT) },
    { "s1", { 0, 189, 100, 111 }, UI_SIZE_S, UI_KINDS_S },
    { "s2", { 100, 189, 100, 111 }, UI_SIZE_S, UI_KINDS_S },
    { "s3", { 200, 189, 100, 111 }, UI_SIZE_S, UI_KINDS_S },
    { "s4", { 300, 189, 100, 111 }, UI_SIZE_S, UI_KINDS_S },
};
static const ui_sep_t k_classic_seps[] = {
    { 12, 188, 376, 0 },
    { 100, 199, 90, 1 },
    { 200, 199, 90, 1 },
    { 300, 199, 90, 1 },
};

static const ui_slot_t k_weather[] = {
    { "now", { 0, 21, 200, 160 }, UI_SIZE_L, UI_KINDS_L },
    { "today", { 200, 21, 200, 80 }, UI_SIZE_M, UI_KINDS_M },
    { "hourly", { 200, 101, 200, 80 }, UI_SIZE_M, UI_KINDS_M },
    { "s1", { 0, 182, 200, 118 }, UI_SIZE_S, UI_KINDS_S },
    { "s2", { 200, 182, 200, 118 }, UI_SIZE_S, UI_KINDS_S },
};
static const ui_sep_t k_weather_seps[] = {
    { 200, 29, 144, 1 },
    { 208, 101, 184, 0 },
    { 12, 181, 376, 0 },
    { 200, 190, 102, 1 },
};

static const ui_slot_t k_grid[] = {
    { "g1", { 0, 21, 133, 139 }, UI_SIZE_M, UI_KINDS_M },
    { "g2", { 133, 21, 134, 139 }, UI_SIZE_M, UI_KINDS_M },
    { "g3", { 267, 21, 133, 139 }, UI_SIZE_M, UI_KINDS_M },
    { "g4", { 0, 160, 133, 140 }, UI_SIZE_M, UI_KINDS_M },
    { "g5", { 133, 160, 134, 140 }, UI_SIZE_M, UI_KINDS_M },
    { "g6", { 267, 160, 133, 140 }, UI_SIZE_M, UI_KINDS_M },
};
static const ui_sep_t k_grid_seps[] = {
    { 133, 29, 263, 1 },
    { 267, 29, 263, 1 },
    { 8, 160, 384, 0 },
};

static const ui_slot_t k_focus[] = {
    { "main", { 0, 21, 400, 190 }, UI_SIZE_XL, UI_KINDS_XL },
    { "s1", { 0, 212, 200, 88 }, UI_SIZE_M, UI_KINDS_M },
    { "s2", { 200, 212, 200, 88 }, UI_SIZE_M, UI_KINDS_M },
};
static const ui_sep_t k_focus_seps[] = {
    { 12, 211, 376, 0 },
    { 200, 220, 72, 1 },
};

static const ui_layout_t k_layouts[UI_LAYOUT_COUNT] = {
    [UI_LAYOUT_CLASSIC] = { "classic", k_classic, COUNT(k_classic), k_classic_seps, COUNT(k_classic_seps) },
    [UI_LAYOUT_WEATHER] = { "weather", k_weather, COUNT(k_weather), k_weather_seps, COUNT(k_weather_seps) },
    [UI_LAYOUT_GRID] = { "grid", k_grid, COUNT(k_grid), k_grid_seps, COUNT(k_grid_seps) },
    [UI_LAYOUT_FOCUS] = { "focus", k_focus, COUNT(k_focus), k_focus_seps, COUNT(k_focus_seps) },
    [UI_LAYOUT_RADAR] = { "radar", NULL, 0, NULL, 0 },
    [UI_LAYOUT_FLIGHTS] = { "flights", NULL, 0, NULL, 0 },
    [UI_LAYOUT_SPLIT] = { "split", NULL, 0, NULL, 0 },
    [UI_LAYOUT_SOLAR] = { "solar", NULL, 0, NULL, 0 },
    [UI_LAYOUT_ENERGY] = { "energy", NULL, 0, NULL, 0 },
};

const ui_profile_t ui_profile_rlcd42 = {
    .board = "rlcd42",
    .width = 400,
    .height = 300,
    .status_h = 20,
    .caps = UI_CAPS_RLCD42,
    .format = GFX_FMT_1BPP,
    .px_num = 1,
    .px_den = 1,
    .fonts = k_fonts,
    .icons = k_icons,
    .icon_px = { 16, 24, 48 },
    .layouts = k_layouts,
    .menu = { .header_h = 30, .row_y0 = 36, .row_h = 34, .rows = 7 },
    .split = { .min_w = 40, .min_h = 20, .narrow_w = 150, .inset = 8 },
    .sun_s_face = &gfx_font_sans_bold_16,
    .sun_s_top = false,
    .moon_fit = false,
    .map_zoom_q = 0,
};
