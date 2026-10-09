#include "gfx_fonts.h"
#include "gfx_icons_t5.h"
#include "ui_layout.h"
#include "ui_profile.h"

/* The LilyGo T5-4.7 (T5 spec §7): 960×540 in 4 bpp gray, its 4-bit fonts and icons at about 1.7× the RLCD's, the
 * fixed layouts of §7.3 below a 34 px status bar. The starting points of the owner's render review (DT2). */

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

static const gfx_font_t *const k_fonts[UI_F_COUNT] = {
    [UI_F_SANS_12] = &gfx_font_t5_sans_20,
    [UI_F_SANS_16] = &gfx_font_t5_sans_26,
    [UI_F_SANS_20] = &gfx_font_t5_sans_34,
    [UI_F_BOLD_16] = &gfx_font_t5_bold_26,
    [UI_F_BOLD_20] = &gfx_font_t5_bold_34,
    [UI_F_BOLD_28] = &gfx_font_t5_bold_46,
    [UI_F_NUM_48] = &gfx_font_t5_num_80,
    [UI_F_NUM_72] = &gfx_font_t5_num_120,
    [UI_F_NUM_110] = &gfx_font_t5_num_180,
    [UI_F_NUM_130] = &gfx_font_t5_num_220,
};

#define T5(n) [UI_ICON_##n] = { &gfx_icon_t5_##n##_26, &gfx_icon_t5_##n##_40, &gfx_icon_t5_##n##_80 },
static const gfx_bitmap_t *const k_icons[UI_ICON_COUNT][UI_IC_CLASSES] = { UI_ICON_LIST(T5) };

/* Slot rectangles for 960×540 below the 34 px status bar: {0, 35, 960, 505}. */
static const ui_slot_t k_classic[] = {
    { "main", { 0, 35, 960, 212 }, UI_SIZE_XL, UI_KINDS_XL },
    { "sub", { 0, 247, 960, 68 }, UI_SIZE_M, UI_KIND(UI_FK_DATE) | UI_KIND(UI_FK_TEXT) },
    { "s1", { 0, 318, 160, 222 }, UI_SIZE_S, UI_KINDS_S },
    { "s2", { 160, 318, 160, 222 }, UI_SIZE_S, UI_KINDS_S },
    { "s3", { 320, 318, 160, 222 }, UI_SIZE_S, UI_KINDS_S },
    { "s4", { 480, 318, 160, 222 }, UI_SIZE_S, UI_KINDS_S },
    { "s5", { 640, 318, 160, 222 }, UI_SIZE_S, UI_KINDS_S },
    { "s6", { 800, 318, 160, 222 }, UI_SIZE_S, UI_KINDS_S },
};
static const ui_sep_t k_classic_seps[] = {
    { 20, 316, 920, 0 },
    { 160, 335, 186, 1 },
    { 320, 335, 186, 1 },
    { 480, 335, 186, 1 },
    { 640, 335, 186, 1 },
    { 800, 335, 186, 1 },
};

static const ui_slot_t k_weather[] = {
    { "now", { 0, 35, 480, 288 }, UI_SIZE_L, UI_KINDS_L },
    { "today", { 480, 35, 480, 144 }, UI_SIZE_M, UI_KINDS_M },
    { "hourly", { 480, 179, 480, 144 }, UI_SIZE_M, UI_KINDS_M },
    { "s1", { 0, 324, 320, 216 }, UI_SIZE_S, UI_KINDS_S },
    { "s2", { 320, 324, 320, 216 }, UI_SIZE_S, UI_KINDS_S },
    { "s3", { 640, 324, 320, 216 }, UI_SIZE_S, UI_KINDS_S },
};
static const ui_sep_t k_weather_seps[] = {
    { 480, 49, 260, 1 },
    { 494, 179, 452, 0 },
    { 20, 323, 920, 0 },
    { 320, 340, 186, 1 },
    { 640, 340, 186, 1 },
};

static const ui_slot_t k_grid[] = {
    { "g1", { 0, 35, 240, 252 }, UI_SIZE_M, UI_KINDS_M },
    { "g2", { 240, 35, 240, 252 }, UI_SIZE_M, UI_KINDS_M },
    { "g3", { 480, 35, 240, 252 }, UI_SIZE_M, UI_KINDS_M },
    { "g4", { 720, 35, 240, 252 }, UI_SIZE_M, UI_KINDS_M },
    { "g5", { 0, 287, 240, 253 }, UI_SIZE_M, UI_KINDS_M },
    { "g6", { 240, 287, 240, 253 }, UI_SIZE_M, UI_KINDS_M },
    { "g7", { 480, 287, 240, 253 }, UI_SIZE_M, UI_KINDS_M },
    { "g8", { 720, 287, 240, 253 }, UI_SIZE_M, UI_KINDS_M },
};
static const ui_sep_t k_grid_seps[] = {
    { 240, 49, 477, 1 },
    { 480, 49, 477, 1 },
    { 720, 49, 477, 1 },
    { 14, 287, 932, 0 },
};

static const ui_slot_t k_focus[] = {
    { "main", { 0, 35, 960, 344 }, UI_SIZE_XL, UI_KINDS_XL },
    { "s1", { 0, 380, 320, 160 }, UI_SIZE_M, UI_KINDS_M },
    { "s2", { 320, 380, 320, 160 }, UI_SIZE_M, UI_KINDS_M },
    { "s3", { 640, 380, 320, 160 }, UI_SIZE_M, UI_KINDS_M },
};
static const ui_sep_t k_focus_seps[] = {
    { 20, 379, 920, 0 },
    { 320, 395, 130, 1 },
    { 640, 395, 130, 1 },
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

const ui_profile_t ui_profile_t547 = {
    .board = "t547",
    .width = 960,
    .height = 540,
    .status_h = 34,
    .caps = UI_CAPS_T547,
    .format = GFX_FMT_4BPP,
    .px_num = 17,
    .px_den = 10,
    .fonts = k_fonts,
    .icons = k_icons,
    .icon_px = { 26, 40, 80 },
    .layouts = k_layouts,
    .menu = { .header_h = 51, .row_y0 = 61, .row_h = 55, .rows = 8 },
    .split = { .min_w = 68, .min_h = 34, .narrow_w = 255, .inset = 14 },
};
