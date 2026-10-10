#define _POSIX_C_SOURCE 200809L /* setenv() in fixture_zone() */

#include <math.h>
#include <string.h>

#include "context_fixtures.h"
#include "radar_fixtures.h"
#include "ui_profile.h"
#include "ui_radar.h"
#include "ui_split.h"
#include "unity.h"

/* The Flights panel's words (spec §11.3) in each state; the goldens show where they go. */

static ui_context_t s_ctx;
static char s_line1[96], s_line2[96], s_credit[32];

void setUp(void)
{
    s_ctx = fixture_context();
}

void tearDown(void)
{
    ui_profile_use(NULL);
}

static void panel(void)
{
    ui_flights_panel_text(&s_ctx, s_line1, s_line2, s_credit, sizeof(s_line1));
}

static void test_the_nearest_aircraft_with_its_route(void)
{
    s_ctx.radar = fixture_flights(50, fixture_aircraft(50), fixture_route(), s_ctx.now);
    panel();
    TEST_ASSERT_EQUAL_STRING("TVS7UZ \xC2\xB7 B38M \xC2\xB7 3675 ft \xC2\xB7 459 km/h", s_line1); /* 247.8 kt */
    TEST_ASSERT_EQUAL_STRING("22 km SE \xC2\xB7 BRQ Brno \xE2\x86\x92 AYT Antalya", s_line2);
    TEST_ASSERT_EQUAL_STRING("adsb.fi \xC2\xB7 adsb.lol", s_credit);
    s_ctx.lang = lang_get("cs");
    panel();
    TEST_ASSERT_EQUAL_STRING("22 km JV \xC2\xB7 BRQ Brno \xE2\x86\x92 AYT Antalya", s_line2);
}

static void test_flight_levels_from_ten_thousand_feet(void)
{
    char text[16];
    ui_flight_altitude(33800, text, sizeof(text));
    TEST_ASSERT_EQUAL_STRING("FL338", text);
    ui_flight_altitude(36975, text, sizeof(text));
    TEST_ASSERT_EQUAL_STRING("FL370", text); /* to the nearest hundred feet */
    ui_flight_altitude(10000, text, sizeof(text));
    TEST_ASSERT_EQUAL_STRING("FL100", text);
    ui_flight_altitude(9975, text, sizeof(text));
    TEST_ASSERT_EQUAL_STRING("9975 ft", text);
    ui_flight_altitude(ADSB_ALT_GROUND, text, sizeof(text));
    TEST_ASSERT_EQUAL_STRING("GND", text);
    ui_flight_altitude(ADSB_ALT_UNKNOWN, text, sizeof(text));
    TEST_ASSERT_EQUAL_STRING("", text);
}

static void test_an_aircraft_without_a_callsign_type_or_speed(void)
{
    static adsb_list_t list;
    list = (adsb_list_t){ .count = 1 };
    list.ac[0] = (adsb_aircraft_t){ .hex = "~bbbbb4", .alt_ft = ADSB_ALT_UNKNOWN, .speed_kt = -1, .track = -1,
                                    .dist_m = 1400, .bearing = 359 };
    s_ctx.radar = fixture_flights(50, &list, NULL, s_ctx.now);
    panel();
    TEST_ASSERT_EQUAL_STRING("~BBBBB4", s_line1); /* its address, as nothing else is known */
    TEST_ASSERT_EQUAL_STRING("1 km N", s_line2);
    TEST_ASSERT_EQUAL_STRING("adsb.fi", s_credit);
}

static void test_the_messages_when_there_is_nothing_to_show(void)
{
    static const adsb_list_t k_empty;
    ui_radar_t *r = fixture_flights(50, &k_empty, NULL, s_ctx.now);
    s_ctx.radar = r;
    panel();
    TEST_ASSERT_EQUAL_STRING("No aircraft within 50 km", s_line1);
    TEST_ASSERT_EQUAL_STRING("", s_line2);

    r->aircraft = fixture_aircraft(50);
    r->fl_failed = true; /* a failure after a poll 4 s ago: its aircraft still show */
    panel();
    TEST_ASSERT_EQUAL_STRING("TVS7UZ", strtok(s_line1, " "));
    r->fl_updated = s_ctx.now - 8 * 60; /* the last good poll at 20:40 */
    panel();
    TEST_ASSERT_EQUAL_STRING("No aircraft data (20:40)", s_line1);
    r->fl_failed = false; /* no failure, but nothing new for 2 min: the same */
    r->fl_updated = s_ctx.now - UI_FLIGHTS_OLD_S - 1;
    panel();
    TEST_ASSERT_EQUAL_STRING("No aircraft data (20:45)", s_line1);
    r->fl_updated = 0; /* not polled yet */
    panel();
    TEST_ASSERT_EQUAL_STRING("No aircraft data", s_line1);
    r->fl_always = false;
    panel();
    TEST_ASSERT_EQUAL_STRING("Flights need sync mode Always on", s_line1);
    s_ctx.radar = NULL;
    panel();
    TEST_ASSERT_EQUAL_STRING("Flights need sync mode Always on", s_line1);
    s_ctx.lang = lang_get("cs");
    panel();
    TEST_ASSERT_EQUAL_STRING("Lety jen v synchronizaci Stále", s_line1);
}

/* T3b: the Flights map over its panel, on each board, and the one view the map and the app's filter share. */
static void test_the_flights_map_sits_over_its_panel(void)
{
    gfx_rect_t m = ui_flights_map_rect(ui_split_area());
    TEST_ASSERT_EQUAL_INT(0, m.x);
    TEST_ASSERT_EQUAL_INT(21, m.y);
    TEST_ASSERT_EQUAL_INT(400, m.w);
    TEST_ASSERT_EQUAL_INT(238, m.h);
    ui_profile_use(&ui_profile_t547);
    m = ui_flights_map_rect(ui_split_area());
    TEST_ASSERT_EQUAL_INT(35, m.y);
    TEST_ASSERT_EQUAL_INT(960, m.w);
    TEST_ASSERT_EQUAL_INT(436, m.h);
}

static void test_the_flights_view_spans_the_map_its_range_up_to_the_top(void)
{
    ui_profile_use(&ui_profile_t547);
    gfx_rect_t m = ui_flights_map_rect(ui_split_area());
    map_view_t v;
    ui_flights_view(491951, 166068, 50, m, &v);
    TEST_ASSERT_EQUAL_INT(960, v.w);
    TEST_ASSERT_EQUAL_INT(436, v.h);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, map_zoom_for_range(491951, 50000.0, 436 / 2), v.zoom);
    double x, y; /* a point near the map's right edge, east of Brno: inside the wide view */
    map_bounds_t b;
    map_view_bounds(&v, &b);
    map_project(&v, 49.1951, b.lon_max - 0.01, &x, &y);
    TEST_ASSERT_TRUE(x > 900 && x < 960);
}

/* T3b review: on the T5 nothing 1 px wide reads (owner, board check), so the ring that marks the panel's aircraft
 * and the line over the panel are 2 px wide. */
static uint8_t s_buf4[960 * 540 / 2];

static void draw_t5_flights(gfx_fb_t *fb)
{
    ui_profile_use(&ui_profile_t547);
    s_ctx.radar = fixture_flights(50, fixture_aircraft(50), fixture_route(), s_ctx.now);
    gfx_fb_init_fmt(fb, s_buf4, 960, 540, GFX_FMT_4BPP);
    gfx_clear(fb, GFX_WHITE);
    ui_draw_flights_view(fb, ui_split_area(), &s_ctx);
}

static void test_the_t5_rings_the_nearest_aircraft_2_px_wide(void)
{
    gfx_fb_t fb;
    draw_t5_flights(&fb);
    gfx_rect_t m = ui_flights_map_rect(ui_split_area());
    map_view_t v;
    ui_flights_view(s_ctx.radar->fl_lat_e4, s_ctx.radar->fl_lon_e4, 50, m, &v);
    double px, py;
    map_project(&v, s_ctx.radar->aircraft->ac[0].lat, s_ctx.radar->aircraft->ac[0].lon, &px, &py);
    int x = m.x + (int)lround(px), y = m.y + (int)lround(py), r = UI_PX(10);
    static const int k_dirs[4][2] = { { 0, -1 }, { 0, 1 }, { -1, 0 }, { 1, 0 } };
    for (int i = 0; i < 4; i++) {
        int dx = k_dirs[i][0], dy = k_dirs[i][1];
        TEST_ASSERT_TRUE_MESSAGE(gfx_get_level(&fb, x + dx * r, y + dy * r) < 8, "the ring's inner pixel");
        TEST_ASSERT_TRUE_MESSAGE(gfx_get_level(&fb, x + dx * (r + 1), y + dy * (r + 1)) < 8, "the ring's outer pixel");
    }
}

static void test_the_t5_line_over_the_flights_panel_is_2_px_wide(void)
{
    gfx_fb_t fb;
    draw_t5_flights(&fb);
    gfx_rect_t m = ui_flights_map_rect(ui_split_area());
    int y = m.y + m.h; /* the row between the map and the panel */
    for (int x = 0; x < 960; x += 10) {
        TEST_ASSERT_TRUE(gfx_get_level(&fb, x, y) < 8);
        TEST_ASSERT_TRUE(gfx_get_level(&fb, x, y + 1) < 8);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_t5_rings_the_nearest_aircraft_2_px_wide);
    RUN_TEST(test_the_t5_line_over_the_flights_panel_is_2_px_wide);
    RUN_TEST(test_the_nearest_aircraft_with_its_route);
    RUN_TEST(test_flight_levels_from_ten_thousand_feet);
    RUN_TEST(test_an_aircraft_without_a_callsign_type_or_speed);
    RUN_TEST(test_the_messages_when_there_is_nothing_to_show);
    RUN_TEST(test_the_flights_map_sits_over_its_panel);
    RUN_TEST(test_the_flights_view_spans_the_map_its_range_up_to_the_top);
    return UNITY_END();
}
