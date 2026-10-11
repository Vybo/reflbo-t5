#define _POSIX_C_SOURCE 200809L /* setenv() in fixture_zone() */

#include <string.h>

#include "dashboard_fixtures.h"
#include "gfx.h"
#include "ui_internal.h"
#include "ui_profile.h"
#include "unity.h"

/* T4: what the UI does on an e-paper board (T5 spec §5.3, §5.6), against the RLCD's. */

static uint8_t s_a[960 * 540 / 2], s_b[960 * 540 / 2];

void setUp(void) {}

void tearDown(void)
{
    ui_profile_use(NULL);
}

/* `name` drawn on the profile in use into `buf`, with seconds and the night's end as given. */
static void render(const char *name, bool seconds, time_t night_until, uint8_t *buf, gfx_fb_t *fb)
{
    ui_context_t ctx;
    ui_preset_t preset;
    TEST_ASSERT_TRUE_MESSAGE(fixture_dashboard(name, &ctx, &preset), name);
    preset.seconds = seconds;
    ctx.night_until = night_until;
    const ui_profile_t *p = ui_profile();
    gfx_fb_init_fmt(fb, buf, p->width, p->height, (gfx_format_t)p->format);
    ui_draw_dashboard(fb, &ctx, &preset);
}

static void test_epaper_is_a_t5_capability(void)
{
    TEST_ASSERT_EQUAL_STRING("epaper", ui_cap_name(UI_CAP_EPAPER));
    TEST_ASSERT_TRUE((ui_profile_t547.caps & UI_CAP_EPAPER) != 0);
    TEST_ASSERT_TRUE((ui_profile()->caps & UI_CAP_EPAPER) == 0); /* the RLCD's */
}

static void test_the_t5_draws_no_seconds(void)
{
    gfx_fb_t a, b;
    render("focus_seconds", true, 0, s_a, &a);
    render("focus_seconds", false, 0, s_b, &b);
    TEST_ASSERT_TRUE_MESSAGE(memcmp(s_a, s_b, gfx_fb_size(400, 300)) != 0, "the RLCD shows the seconds");
    ui_profile_use(&ui_profile_t547);
    render("focus_seconds", true, 0, s_a, &a);
    render("focus_seconds", false, 0, s_b, &b);
    TEST_ASSERT_EQUAL_MEMORY(s_b, s_a, sizeof(s_a));
}

static void test_a_night_puts_a_moon_and_its_end_in_the_t5_status_bar(void)
{
    gfx_fb_t a, b;
    const time_t until = FIX_NOW + 8 * 3600;
    render("home", false, until, s_a, &a);
    render("home", false, 0, s_b, &b);
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(s_b, s_a, gfx_fb_size(400, 300), "the RLCD's panel sleeps: no mark");

    ui_profile_use(&ui_profile_t547);
    render("home", false, until, s_a, &a);
    render("home", false, 0, s_b, &b);
    int changed = 0, below = 0;
    for (int y = 0; y < a.height; y++) {
        for (int x = 0; x < a.width; x++) {
            if (gfx_get_level(&a, x, y) != gfx_get_level(&b, x, y)) {
                changed++;
                below += y >= ui_profile()->status_h;
            }
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(changed > 200, "a moon and a time");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, below, "only the status bar changes");
}

/* T4's render review: the Moon's outline was a 1 px circle on the T5, where nothing 1 px wide reads (T5 spec §6.4);
 * its ring is UI_PX(1) wide, drawn inwards so the disc keeps its size. */
static void test_the_t5_moon_has_a_2_px_outline(void)
{
    gfx_fb_t fb;
    ui_profile_use(&ui_profile_t547);
    gfx_fb_init_fmt(&fb, s_a, 200, 100, GFX_FMT_4BPP);
    gfx_clear(&fb, GFX_WHITE);
    ui_draw_moon(&fb, 50, 50, 30, 0.5); /* full (age is the fraction of the cycle): white inside its ring */
    TEST_ASSERT_EQUAL_UINT8(0, gfx_get_level(&fb, 50 - 30, 50));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, gfx_get_level(&fb, 50 - 29, 50), "the ring's second pixel");
    TEST_ASSERT_EQUAL_UINT8(15, gfx_get_level(&fb, 50 - 31, 50)); /* not grown outwards */
    TEST_ASSERT_EQUAL_UINT8(15, gfx_get_level(&fb, 50, 50));

    ui_profile_use(NULL); /* the RLCD keeps its 1 px ring */
    gfx_fb_init(&fb, s_a, 200, 100);
    gfx_clear(&fb, GFX_WHITE);
    ui_draw_moon(&fb, 50, 50, 30, 0.5);
    TEST_ASSERT_TRUE(gfx_get_pixel(&fb, 50 - 30, 50));
    TEST_ASSERT_FALSE(gfx_get_pixel(&fb, 50 - 29, 50));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_epaper_is_a_t5_capability);
    RUN_TEST(test_the_t5_draws_no_seconds);
    RUN_TEST(test_a_night_puts_a_moon_and_its_end_in_the_t5_status_bar);
    RUN_TEST(test_the_t5_moon_has_a_2_px_outline);
    return UNITY_END();
}
