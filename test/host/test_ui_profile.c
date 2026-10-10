#define _POSIX_C_SOURCE 200809L /* setenv() in fixture_zone() */

#include <stdio.h>
#include <string.h>

#include "dashboard_fixtures.h"
#include "gfx_fonts.h"
#include "gfx_icons.h"
#include "map_draw.h"
#include "ui_layout.h"
#include "ui_profile.h"
#include "ui_radar.h"
#include "screen_fixtures.h"
#include "ui_split.h"
#include "unity.h"

void setUp(void)
{
}

void tearDown(void)
{
    ui_profile_use(NULL);
}

static void test_the_rlcd_is_the_default_profile(void)
{
    TEST_ASSERT_EQUAL_PTR(&ui_profile_rlcd42, ui_profile());
    TEST_ASSERT_EQUAL_STRING("rlcd42", ui_profile()->board);
    TEST_ASSERT_EQUAL_INT(400, ui_profile()->width);
    TEST_ASSERT_EQUAL_INT(300, ui_profile()->height);
    TEST_ASSERT_EQUAL_INT(20, UI_STATUS_H);
}

static void test_use_switches_the_profile_and_null_restores_the_rlcd(void)
{
    ui_profile_use(&ui_profile_t547);
    TEST_ASSERT_EQUAL_STRING("t547", ui_profile()->board);
    ui_profile_use(NULL);
    TEST_ASSERT_EQUAL_PTR(&ui_profile_rlcd42, ui_profile());
}

/* T3a (T5 spec §7.1-7.3): the T5's own geometry, and none of the RLCD's capabilities. */
static void test_the_t5_has_its_own_geometry_and_none_of_the_rlcds_capabilities(void)
{
    TEST_ASSERT_EQUAL_INT(960, ui_profile_t547.width);
    TEST_ASSERT_EQUAL_INT(540, ui_profile_t547.height);
    TEST_ASSERT_EQUAL_INT(34, ui_profile_t547.status_h);
    TEST_ASSERT_EQUAL_INT(GFX_FMT_4BPP, ui_profile_t547.format);
    TEST_ASSERT_EQUAL_HEX32(0, ui_profile_t547.caps);
    TEST_ASSERT_EQUAL_HEX32(UI_CAP_ENV_SENSOR | UI_CAP_AUDIO | UI_CAP_RTC_TRIM | UI_CAP_RTC_ALARM_WAKE |
                                UI_CAP_LPM_RATE,
                            ui_profile_rlcd42.caps);
    TEST_ASSERT_EQUAL_STRING("lpm_rate", ui_cap_name(UI_CAP_LPM_RATE));
    TEST_ASSERT_NULL(ui_cap_name(1u << 31));
    TEST_ASSERT_NULL(ui_cap_name(UI_CAP_AUDIO | UI_CAP_RTC_TRIM)); /* one bit at a time */
}

static void test_the_rlcd_split_area_is_unchanged(void)
{
    gfx_rect_t a = ui_split_area();
    TEST_ASSERT_EQUAL_INT(0, a.x);
    TEST_ASSERT_EQUAL_INT(21, a.y);
    TEST_ASSERT_EQUAL_INT(400, a.w);
    TEST_ASSERT_EQUAL_INT(279, a.h);
}

static void test_the_split_area_and_status_bar_follow_the_profile(void)
{
    ui_profile_t wide = ui_profile_t547; /* a copy: positional initializers break as the profile grows */
    wide.status_h = 34;
    ui_profile_use(&wide);
    TEST_ASSERT_EQUAL_INT(34, UI_STATUS_H);
    gfx_rect_t a = ui_split_area();
    TEST_ASSERT_EQUAL_INT(0, a.x);
    TEST_ASSERT_EQUAL_INT(35, a.y);
    TEST_ASSERT_EQUAL_INT(960, a.w);
    TEST_ASSERT_EQUAL_INT(505, a.h);
}

static void test_each_capability_has_a_name(void)
{
    TEST_ASSERT_EQUAL_STRING("env_sensor", ui_cap_name(UI_CAP_ENV_SENSOR));
    TEST_ASSERT_EQUAL_STRING("audio", ui_cap_name(UI_CAP_AUDIO));
    TEST_ASSERT_EQUAL_STRING("rtc_trim", ui_cap_name(UI_CAP_RTC_TRIM));
    TEST_ASSERT_EQUAL_STRING("rtc_alarm_wake", ui_cap_name(UI_CAP_RTC_ALARM_WAKE));
    TEST_ASSERT_EQUAL_STRING("lpm_rate", ui_cap_name(UI_CAP_LPM_RATE));
    TEST_ASSERT_NULL(ui_cap_name(1u << 31));
    TEST_ASSERT_NULL(ui_cap_name(UI_CAP_AUDIO | UI_CAP_RTC_TRIM)); /* one bit at a time */
}


/* UI_PX() (T5 spec §7.1): the identity on the RLCD, × 1.7 rounded half away from zero on the T5. */
static void test_ui_px_is_the_identity_on_the_rlcd_and_1_7_on_the_t5(void)
{
    for (int n = -5; n <= 400; n++) {
        TEST_ASSERT_EQUAL_INT(n, ui_px(n));
    }
    ui_profile_use(&ui_profile_t547);
    TEST_ASSERT_EQUAL_INT(0, UI_PX(0));
    TEST_ASSERT_EQUAL_INT(2, UI_PX(1));
    TEST_ASSERT_EQUAL_INT(7, UI_PX(4));
    TEST_ASSERT_EQUAL_INT(17, UI_PX(10));
    TEST_ASSERT_EQUAL_INT(34, UI_PX(20));
    TEST_ASSERT_EQUAL_INT(-10, UI_PX(-6));
}

/* Review Focus 1: the RLCD's tables are today's literals. */
static void test_the_rlcd_fonts_and_icons_are_todays(void)
{
    const gfx_font_t *const want[UI_F_COUNT] = { &gfx_font_sans_12, &gfx_font_sans_16, &gfx_font_sans_20,
                                                 &gfx_font_sans_bold_16, &gfx_font_sans_bold_20, &gfx_font_sans_bold_28,
                                                 &gfx_font_num_cb_48, &gfx_font_num_cb_72, &gfx_font_num_cb_110,
                                                 &gfx_font_num_cb_130 };
    for (int i = 0; i < UI_F_COUNT; i++) {
        TEST_ASSERT_EQUAL_PTR(want[i], UI_FONT(i));
    }
    TEST_ASSERT_EQUAL_PTR(&gfx_icon_thermometer_48, ui_icon(UI_ICON_thermometer, UI_IC48));
    TEST_ASSERT_EQUAL_PTR(&gfx_icon_wx_rain_24, ui_icon(UI_ICON_wx_rain, UI_IC24));
    TEST_ASSERT_EQUAL_PTR(&gfx_icon_bolt_24, ui_icon(UI_ICON_bolt, UI_IC48));
    TEST_ASSERT_EQUAL_PTR(&gfx_icon_web_16, ui_icon(UI_ICON_web, UI_IC24));
    for (int id = 0; id < UI_ICON_COUNT; id++) {
        for (int c = 0; c < UI_IC_CLASSES; c++) {
            TEST_ASSERT_NOT_NULL(ui_icon((ui_icon_id_t)id, (ui_icon_class_t)c));
        }
    }
    TEST_ASSERT_EQUAL_INT(16, ui_icon_px(16));
    TEST_ASSERT_EQUAL_INT(24, ui_icon_px(24));
    TEST_ASSERT_EQUAL_INT(48, ui_icon_px(48));
    TEST_ASSERT_EQUAL_INT(UI_IC24, ui_icon_class(47));
    TEST_ASSERT_EQUAL_INT(UI_IC16, ui_icon_class(23));
    TEST_ASSERT_NULL(ui_icon(UI_ICON_COUNT, UI_IC16));
}

static void assert_slots(const ui_layout_t *l, const ui_slot_t *want, int n)
{
    TEST_ASSERT_EQUAL_INT(n, l->slot_count);
    for (int i = 0; i < n; i++) {
        TEST_ASSERT_EQUAL_STRING(want[i].name, l->slots[i].name);
        TEST_ASSERT_EQUAL_MEMORY(&want[i].rect, &l->slots[i].rect, sizeof(gfx_rect_t));
        TEST_ASSERT_EQUAL_INT(want[i].size, l->slots[i].size);
        TEST_ASSERT_EQUAL_HEX32(want[i].kinds, l->slots[i].kinds);
    }
}

static void assert_seps(const ui_layout_t *l, const ui_sep_t *want, int n)
{
    TEST_ASSERT_EQUAL_INT(n, l->sep_count);
    for (int i = 0; i < n; i++) {
        TEST_ASSERT_EQUAL_INT(want[i].x, l->seps[i].x);
        TEST_ASSERT_EQUAL_INT(want[i].y, l->seps[i].y);
        TEST_ASSERT_EQUAL_INT(want[i].len, l->seps[i].len);
        TEST_ASSERT_EQUAL_INT(want[i].vertical, l->seps[i].vertical);
    }
}

/* The RLCD's layouts as ui_layout.c and ui_dashboard.c had them before T3a. */
static void test_the_rlcd_layouts_and_separators_are_todays(void)
{
    const ui_slot_t classic[] = {
        { "main", { 0, 21, 400, 125 }, UI_SIZE_XL, UI_KINDS_XL },
        { "sub", { 0, 146, 400, 40 }, UI_SIZE_M, UI_KIND(UI_FK_DATE) | UI_KIND(UI_FK_TEXT) },
        { "s1", { 0, 189, 100, 111 }, UI_SIZE_S, UI_KINDS_S },
        { "s2", { 100, 189, 100, 111 }, UI_SIZE_S, UI_KINDS_S },
        { "s3", { 200, 189, 100, 111 }, UI_SIZE_S, UI_KINDS_S },
        { "s4", { 300, 189, 100, 111 }, UI_SIZE_S, UI_KINDS_S },
    };
    const ui_slot_t weather[] = {
        { "now", { 0, 21, 200, 160 }, UI_SIZE_L, UI_KINDS_L },
        { "today", { 200, 21, 200, 80 }, UI_SIZE_M, UI_KINDS_M },
        { "hourly", { 200, 101, 200, 80 }, UI_SIZE_M, UI_KINDS_M },
        { "s1", { 0, 182, 200, 118 }, UI_SIZE_S, UI_KINDS_S },
        { "s2", { 200, 182, 200, 118 }, UI_SIZE_S, UI_KINDS_S },
    };
    const ui_slot_t grid[] = {
        { "g1", { 0, 21, 133, 139 }, UI_SIZE_M, UI_KINDS_M },
        { "g2", { 133, 21, 134, 139 }, UI_SIZE_M, UI_KINDS_M },
        { "g3", { 267, 21, 133, 139 }, UI_SIZE_M, UI_KINDS_M },
        { "g4", { 0, 160, 133, 140 }, UI_SIZE_M, UI_KINDS_M },
        { "g5", { 133, 160, 134, 140 }, UI_SIZE_M, UI_KINDS_M },
        { "g6", { 267, 160, 133, 140 }, UI_SIZE_M, UI_KINDS_M },
    };
    const ui_slot_t focus[] = {
        { "main", { 0, 21, 400, 190 }, UI_SIZE_XL, UI_KINDS_XL },
        { "s1", { 0, 212, 200, 88 }, UI_SIZE_M, UI_KINDS_M },
        { "s2", { 200, 212, 200, 88 }, UI_SIZE_M, UI_KINDS_M },
    };
    assert_slots(ui_layout(UI_LAYOUT_CLASSIC), classic, 6);
    assert_slots(ui_layout(UI_LAYOUT_WEATHER), weather, 5);
    assert_slots(ui_layout(UI_LAYOUT_GRID), grid, 6);
    assert_slots(ui_layout(UI_LAYOUT_FOCUS), focus, 3);
    const ui_sep_t c_sep[] = { { 12, 188, 376, 0 }, { 100, 199, 90, 1 }, { 200, 199, 90, 1 }, { 300, 199, 90, 1 } };
    const ui_sep_t w_sep[] = { { 200, 29, 144, 1 }, { 208, 101, 184, 0 }, { 12, 181, 376, 0 }, { 200, 190, 102, 1 } };
    const ui_sep_t g_sep[] = { { 133, 29, 263, 1 }, { 267, 29, 263, 1 }, { 8, 160, 384, 0 } };
    const ui_sep_t f_sep[] = { { 12, 211, 376, 0 }, { 200, 220, 72, 1 } };
    assert_seps(ui_layout(UI_LAYOUT_CLASSIC), c_sep, 4);
    assert_seps(ui_layout(UI_LAYOUT_WEATHER), w_sep, 4);
    assert_seps(ui_layout(UI_LAYOUT_GRID), g_sep, 3);
    assert_seps(ui_layout(UI_LAYOUT_FOCUS), f_sep, 2);
    for (int id = UI_LAYOUT_RADAR; id < UI_LAYOUT_COUNT; id++) {
        TEST_ASSERT_EQUAL_INT(0, ui_layout((ui_layout_id_t)id)->slot_count);
        TEST_ASSERT_EQUAL_INT(0, ui_layout((ui_layout_id_t)id)->sep_count);
    }
    const ui_profile_t *p = ui_profile();
    TEST_ASSERT_EQUAL_INT(GFX_FMT_1BPP, p->format);
    TEST_ASSERT_EQUAL_INT(30, p->menu.header_h);
    TEST_ASSERT_EQUAL_INT(36, p->menu.row_y0);
    TEST_ASSERT_EQUAL_INT(34, p->menu.row_h);
    TEST_ASSERT_EQUAL_INT(7, p->menu.rows);
    TEST_ASSERT_EQUAL_INT(40, p->split.min_w);
    TEST_ASSERT_EQUAL_INT(20, p->split.min_h);
    TEST_ASSERT_EQUAL_INT(150, p->split.narrow_w);
    TEST_ASSERT_EQUAL_INT(8, p->split.inset);
}

/* T5 spec §7.2-7.3: the T5's 4-bit assets, its menu, split limits and fixed layouts. */
static void test_the_t5_tables(void)
{
    ui_profile_use(&ui_profile_t547);
    const ui_profile_t *p = ui_profile();
    for (int i = 0; i < UI_F_COUNT; i++) {
        TEST_ASSERT_EQUAL_UINT8(4, UI_FONT(i)->bpp);
    }
    for (int id = 0; id < UI_ICON_COUNT; id++) {
        for (int c = 0; c < UI_IC_CLASSES; c++) {
            const gfx_bitmap_t *b = ui_icon((ui_icon_id_t)id, (ui_icon_class_t)c);
            TEST_ASSERT_NOT_NULL(b);
            TEST_ASSERT_EQUAL_UINT8(4, b->bpp);
            TEST_ASSERT_EQUAL_INT(p->icon_px[c], b->width);
        }
    }
    TEST_ASSERT_EQUAL_INT(26, ui_icon_px(16));
    TEST_ASSERT_EQUAL_INT(40, ui_icon_px(24));
    TEST_ASSERT_EQUAL_INT(80, ui_icon_px(48));
    TEST_ASSERT_EQUAL_INT(51, p->menu.header_h);
    TEST_ASSERT_EQUAL_INT(61, p->menu.row_y0);
    TEST_ASSERT_EQUAL_INT(55, p->menu.row_h);
    TEST_ASSERT_EQUAL_INT(8, p->menu.rows);
    TEST_ASSERT_EQUAL_INT(68, p->split.min_w);
    TEST_ASSERT_EQUAL_INT(34, p->split.min_h);
    TEST_ASSERT_EQUAL_INT(255, p->split.narrow_w);
    TEST_ASSERT_EQUAL_INT(14, p->split.inset);
    TEST_ASSERT_EQUAL_INT(8, ui_layout(UI_LAYOUT_CLASSIC)->slot_count);
    TEST_ASSERT_EQUAL_INT(6, ui_layout(UI_LAYOUT_WEATHER)->slot_count);
    TEST_ASSERT_EQUAL_INT(8, ui_layout(UI_LAYOUT_GRID)->slot_count);
    TEST_ASSERT_EQUAL_INT(4, ui_layout(UI_LAYOUT_FOCUS)->slot_count);
    TEST_ASSERT_EQUAL_INT(UI_LAYOUT_CLASSIC, ui_layout_by_name("classic"));
    TEST_ASSERT_EQUAL_INT(7, ui_slot_by_name(ui_layout(UI_LAYOUT_CLASSIC), "s6"));
}

/* T3a: UI_PX() with equal numerator and denominator is the identity by another path, so every RLCD golden renders
 * unchanged under such a copy of the RLCD's profile; a literal converted by mistake, or a rewrite that moves the
 * RLCD's number, shows here beside the golden tests. */
static uint8_t s_fb_buf[400 * 300 / 8];
static uint8_t s_img[16000];
static uint8_t s_golden[16000];

static void expect_golden(gfx_fb_t *fb, const char *path)
{
    size_t n = gfx_pbm_encode(fb, s_img, sizeof(s_img));
    FILE *f = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, path);
    size_t golden = fread(s_golden, 1, sizeof(s_golden), f);
    fclose(f);
    TEST_ASSERT_EQUAL_INT_MESSAGE((int)golden, (int)n, path);
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(s_golden, s_img, n, path);
}

static void test_a_scale_of_three_thirds_draws_every_rlcd_golden(void)
{
    ui_profile_t thirds = ui_profile_rlcd42;
    thirds.px_num = 3;
    thirds.px_den = 3;
    ui_profile_use(&thirds);
    char path[256];
    gfx_fb_t fb;
    for (size_t i = 0; i < sizeof(k_dashboard_fixtures) / sizeof(k_dashboard_fixtures[0]); i++) {
        ui_context_t ctx;
        ui_preset_t preset;
        TEST_ASSERT_TRUE_MESSAGE(fixture_dashboard(k_dashboard_fixtures[i], &ctx, &preset), k_dashboard_fixtures[i]);
        gfx_fb_init(&fb, s_fb_buf, 400, 300);
        ui_draw_dashboard(&fb, &ctx, &preset);
        snprintf(path, sizeof(path), "%s/dash_%s.pbm", GOLDEN_DIR, k_dashboard_fixtures[i]);
        expect_golden(&fb, path);
    }
    for (size_t i = 0; i < sizeof(k_screen_fixtures) / sizeof(k_screen_fixtures[0]); i++) {
        gfx_fb_init(&fb, s_fb_buf, 400, 300);
        TEST_ASSERT_TRUE_MESSAGE(fixture_screen(k_screen_fixtures[i], &fb), k_screen_fixtures[i]);
        snprintf(path, sizeof(path), "%s/screen_%s.pbm", GOLDEN_DIR, k_screen_fixtures[i]);
        expect_golden(&fb, path);
    }
}

/* T3a (Review Focus 3): under the T5's profile each fixed layout's slots and lines lie under the status bar, the
 * slots don't overlap, and each is at least its size's least cell. */
static void test_the_t5_layouts_fit_the_screen(void)
{
    ui_profile_use(&ui_profile_t547);
    const gfx_rect_t below = { 0, 35, 960, 505 };
    for (int id = UI_LAYOUT_CLASSIC; id <= UI_LAYOUT_FOCUS; id++) {
        const ui_layout_t *l = ui_layout((ui_layout_id_t)id);
        TEST_ASSERT_TRUE(l->slot_count > 0);
        for (int i = 0; i < l->slot_count; i++) {
            gfx_rect_t a = l->slots[i].rect;
            TEST_ASSERT_TRUE_MESSAGE(a.x >= below.x && a.y >= below.y && a.x + a.w <= below.x + below.w &&
                                         a.y + a.h <= below.y + below.h,
                                     l->slots[i].name);
            if (a.w < ui_split_min_w(l->slots[i].size) ||
                a.h < ui_split_min_h(l->slots[i].size, a.w < UI_SPLIT_NARROW_W)) {
                /* only where the RLCD's own slot is as short (Classic's date row, 40 px at M), and no shorter
                 * than it scaled */
                ui_profile_use(NULL);
                const ui_layout_t *r = ui_layout((ui_layout_id_t)id);
                int k = ui_slot_by_name(r, l->slots[i].name);
                TEST_ASSERT_TRUE_MESSAGE(k >= 0, l->slots[i].name);
                gfx_rect_t rr = r->slots[k].rect;
                bool rlcd_short = rr.w < ui_split_min_w(r->slots[k].size) ||
                                  rr.h < ui_split_min_h(r->slots[k].size, rr.w < UI_SPLIT_NARROW_W);
                ui_profile_use(&ui_profile_t547);
                TEST_ASSERT_TRUE_MESSAGE(rlcd_short, l->slots[i].name);
                TEST_ASSERT_TRUE_MESSAGE(a.h >= ui_px(rr.h), l->slots[i].name);
            }
            for (int k = i + 1; k < l->slot_count; k++) {
                gfx_rect_t b = l->slots[k].rect;
                bool apart = a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y || b.y + b.h <= a.y;
                TEST_ASSERT_TRUE_MESSAGE(apart, l->id);
            }
        }
        for (int i = 0; i < l->sep_count; i++) {
            const ui_sep_t *sep = &l->seps[i];
            int x1 = sep->x + (sep->vertical ? 1 : sep->len), y1 = sep->y + (sep->vertical ? sep->len : 1);
            TEST_ASSERT_TRUE_MESSAGE(sep->x >= 0 && sep->y >= below.y && x1 <= 960 && y1 <= 540, l->id);
        }
    }
}

/* T3a (Review Focus 5): the display's frame must be the profile's, size and format, or the UI draws off it. */
static void test_a_frame_matches_the_profile_only_in_size_and_format(void)
{
    static uint8_t buf[960 * 540 / 2];
    gfx_fb_t fb;
    gfx_fb_init(&fb, buf, 400, 300);
    TEST_ASSERT_TRUE(ui_profile_matches(&fb));
    gfx_fb_init(&fb, buf, 401, 300);
    TEST_ASSERT_FALSE(ui_profile_matches(&fb));
    gfx_fb_init(&fb, buf, 400, 299);
    TEST_ASSERT_FALSE(ui_profile_matches(&fb));
    gfx_fb_init_fmt(&fb, buf, 400, 300, GFX_FMT_4BPP);
    TEST_ASSERT_FALSE(ui_profile_matches(&fb));
    TEST_ASSERT_FALSE(ui_profile_matches(NULL));

    ui_profile_use(&ui_profile_t547);
    gfx_fb_init_fmt(&fb, buf, 960, 540, GFX_FMT_4BPP);
    TEST_ASSERT_TRUE(ui_profile_matches(&fb));
    gfx_fb_init_fmt(&fb, buf, 960, 540, GFX_FMT_1BPP);
    TEST_ASSERT_FALSE(ui_profile_matches(&fb));
    gfx_fb_init_fmt(&fb, buf, 400, 300, GFX_FMT_4BPP);
    TEST_ASSERT_FALSE(ui_profile_matches(&fb));
    gfx_fb_init_fmt(&fb, buf, 960, 539, GFX_FMT_4BPP);
    TEST_ASSERT_FALSE(ui_profile_matches(&fb));
}

/* Owner, T3a render review (2026-10-09): on the T5 the sun's times in a narrow S cell start at the top, level with
 * the other stacked S widgets' symbols (UI_PX(12) down), beside 40 px icons, in a face larger than the RLCD's
 * scaled. */
static void test_the_t5_sun_in_a_narrow_cell_stacks_from_the_top_in_a_larger_face(void)
{
    static uint8_t buf[960 * 540 / 2];
    ui_profile_use(&ui_profile_t547);
    ui_context_t ctx;
    ui_preset_t preset;
    TEST_ASSERT_TRUE(fixture_dashboard("home", &ctx, &preset));
    TEST_ASSERT_EQUAL(UI_FIELD_SUN_TIMES, preset.slots[4]); /* Classic's s3 */
    gfx_rect_t r = ui_layout(UI_LAYOUT_CLASSIC)->slots[4].rect;
    gfx_fb_t fb;
    gfx_fb_init_fmt(&fb, buf, 960, 540, GFX_FMT_4BPP);
    ui_draw_dashboard(&fb, &ctx, &preset);
    int top = -1, bottom = -1, left = r.x + r.w, right = -1;
    for (int y = r.y; y < r.y + r.h; y++) {
        for (int x = r.x + 2; x < r.x + r.w - 2; x++) { /* clear of the separators at the cell's edges */
            if (gfx_get_level(&fb, x, y) < 15) {
                top = top < 0 ? y : top;
                bottom = y;
                left = x < left ? x : left;
                right = x > right ? x : right;
            }
        }
    }
    TEST_ASSERT_TRUE(top >= 0);
    TEST_ASSERT_INT_WITHIN(8, r.y + ui_px(12) + 4, top);
    /* two rows of 40 px icons, 4 px apart (their own margins inside), and digits larger than bold 16's role */
    TEST_ASSERT_TRUE(bottom - top + 1 >= 2 * ui_icon_px(24) + ui_px(4) - 16);
    TEST_ASSERT_TRUE(right - left + 1 >= ui_icon_px(24) + ui_px(6) + gfx_text_width(UI_FONT(UI_F_BOLD_16), "06:44") + 8);
}

/* T3b: the map's style from the profile: the RLCD's font, scale and black lines; the T5's 4-bit font, 1.7, gray. */
static void test_the_map_style_follows_the_profile(void)
{
    map_style_t s = { 0 };
    ui_map_style(&s);
    TEST_ASSERT_EQUAL_PTR(UI_FONT(UI_F_SANS_12), s.font);
    TEST_ASSERT_EQUAL_INT(1, s.px_num);
    TEST_ASSERT_EQUAL_INT(1, s.px_den);
    TEST_ASSERT_EQUAL(GFX_BLACK, s.line);
    ui_profile_use(&ui_profile_t547);
    ui_map_style(&s);
    TEST_ASSERT_EQUAL_UINT8(4, s.font->bpp);
    TEST_ASSERT_EQUAL_INT(17, s.px_num);
    TEST_ASSERT_EQUAL_INT(10, s.px_den);
    TEST_ASSERT_TRUE(s.line >= GFX_GRAY(0) && s.line <= GFX_GRAY(8));
}

/* T3b: a radar map's view is the setting's zoom plus the profile's step; the fetch asks for that very view. */
static void test_the_radar_view_and_its_fetch_agree(void)
{
    map_view_t v;
    uint8_t fz;
    uint16_t w, h;
    ui_radar_view(491951, 166068, 26, ui_split_area(), &v);
    ui_radar_fetch_size(26, &fz, &w, &h);
    TEST_ASSERT_EQUAL_INT(400, w);
    TEST_ASSERT_EQUAL_INT(279, h);
    TEST_ASSERT_EQUAL_UINT8(26, fz);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 6.5, v.zoom);
    ui_profile_use(&ui_profile_t547);
    ui_radar_view(491951, 166068, 26, ui_split_area(), &v);
    ui_radar_fetch_size(26, &fz, &w, &h);
    TEST_ASSERT_EQUAL_INT(960, w);
    TEST_ASSERT_EQUAL_INT(505, h);
    TEST_ASSERT_EQUAL_UINT8(29, fz);
    TEST_ASSERT_EQUAL_INT(960, (int)v.w);
    TEST_ASSERT_EQUAL_INT(505, (int)v.h);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, fz / 4.0, v.zoom);
}

/* T5 spec §6.4: the status bar's line in gray on the T5, black on the RLCD. */
static void test_the_status_line_is_gray_on_the_t5(void)
{
    static uint8_t buf[960 * 540 / 2];
    ui_context_t ctx;
    ui_preset_t preset;
    gfx_fb_t fb;
    TEST_ASSERT_TRUE(fixture_dashboard("home", &ctx, &preset));
    gfx_fb_init(&fb, buf, 400, 300);
    ui_draw_dashboard(&fb, &ctx, &preset);
    TEST_ASSERT_TRUE(gfx_get_pixel(&fb, 200, 20));
    ui_profile_use(&ui_profile_t547);
    TEST_ASSERT_TRUE(fixture_dashboard("home", &ctx, &preset));
    gfx_fb_init_fmt(&fb, buf, 960, 540, GFX_FMT_4BPP);
    ui_draw_dashboard(&fb, &ctx, &preset);
    uint8_t level = gfx_get_level(&fb, 480, 34);
    TEST_ASSERT_TRUE_MESSAGE(level > 0 && level <= 8, "a gray from the dark half");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_rlcd_is_the_default_profile);
    RUN_TEST(test_the_status_line_is_gray_on_the_t5);
    RUN_TEST(test_the_radar_view_and_its_fetch_agree);
    RUN_TEST(test_the_map_style_follows_the_profile);
    RUN_TEST(test_the_t5_sun_in_a_narrow_cell_stacks_from_the_top_in_a_larger_face);
    RUN_TEST(test_a_frame_matches_the_profile_only_in_size_and_format);
    RUN_TEST(test_the_t5_layouts_fit_the_screen);
    RUN_TEST(test_a_scale_of_three_thirds_draws_every_rlcd_golden);
    RUN_TEST(test_use_switches_the_profile_and_null_restores_the_rlcd);
    RUN_TEST(test_the_t5_has_its_own_geometry_and_none_of_the_rlcds_capabilities);
    RUN_TEST(test_the_rlcd_split_area_is_unchanged);
    RUN_TEST(test_the_split_area_and_status_bar_follow_the_profile);
    RUN_TEST(test_each_capability_has_a_name);
    RUN_TEST(test_ui_px_is_the_identity_on_the_rlcd_and_1_7_on_the_t5);
    RUN_TEST(test_the_rlcd_fonts_and_icons_are_todays);
    RUN_TEST(test_the_rlcd_layouts_and_separators_are_todays);
    RUN_TEST(test_the_t5_tables);
    return UNITY_END();
}
