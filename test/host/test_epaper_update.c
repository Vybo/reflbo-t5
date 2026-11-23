#include <stdint.h>
#include <string.h>

#include "epaper_update.h"
#include "unity.h"

#define W 16
#define H 4

static uint8_t s_before[W / 2 * H], s_after[W / 2 * H];

void setUp(void)
{
    memset(s_before, 0xFF, sizeof(s_before)); /* white */
    memset(s_after, 0xFF, sizeof(s_after));
}

void tearDown(void) {}

static void set_px(uint8_t *buf, int x, int y, uint8_t level)
{
    uint8_t *b = &buf[y * (W / 2) + x / 2];
    *b = (x & 1) ? (uint8_t)((*b & 0x0F) | (level << 4)) : (uint8_t)((*b & 0xF0) | level);
}

static epaper_change_t change(void)
{
    epaper_change_t c;
    epaper_change(s_before, s_after, W, H, &c);
    return c;
}

static void test_no_change_is_none_and_a_box_bounds_the_change(void)
{
    epaper_change_t c = change();
    TEST_ASSERT_FALSE(c.any);

    set_px(s_after, 5, 2, 6); /* white to a gray */
    c = change();
    TEST_ASSERT_TRUE(c.any);
    TEST_ASSERT_FALSE(c.bw_only);
    TEST_ASSERT_EQUAL_INT(5, c.x);
    TEST_ASSERT_EQUAL_INT(2, c.y);
    TEST_ASSERT_EQUAL_INT(1, c.w);
    TEST_ASSERT_EQUAL_INT(1, c.h);

    memcpy(s_after, s_before, sizeof(s_after));
    set_px(s_after, 1, 0, 0); /* white to black, at both corners of the box */
    set_px(s_after, 14, 3, 0);
    c = change();
    TEST_ASSERT_TRUE(c.bw_only);
    TEST_ASSERT_EQUAL_INT(1, c.x);
    TEST_ASSERT_EQUAL_INT(0, c.y);
    TEST_ASSERT_EQUAL_INT(14, c.w);
    TEST_ASSERT_EQUAL_INT(4, c.h);
}

static void test_black_and_white_changes_are_du_others_gl16(void)
{
    epaper_policy_t p = { .known = true, .clean_after = 60 };
    set_px(s_after, 3, 1, 0);
    epaper_change_t c = change();
    TEST_ASSERT_EQUAL_INT(EPAPER_DU, epaper_choose(&p, &c));
    set_px(s_before, 3, 1, 8); /* a gray before: not black and white */
    c = change();
    TEST_ASSERT_EQUAL_INT(EPAPER_GL16, epaper_choose(&p, &c));
}

/* Review Focus 1 and 4: after a wake whose frame didn't match, and when the menu or the loop ends. */
static void test_an_unknown_frame_or_a_clean_request_is_clean_even_unchanged(void)
{
    epaper_change_t none = change();
    epaper_policy_t p = { .known = false, .clean_after = 60 };
    TEST_ASSERT_EQUAL_INT(EPAPER_CLEAN, epaper_choose(&p, &none));
    p = (epaper_policy_t){ .known = true, .clean_next = true, .clean_after = 60 };
    TEST_ASSERT_EQUAL_INT(EPAPER_CLEAN, epaper_choose(&p, &none));
    epaper_after(&p, EPAPER_CLEAN, EPAPER_DONE);
    TEST_ASSERT_FALSE(p.clean_next);
    TEST_ASSERT_TRUE(p.known);
    TEST_ASSERT_EQUAL_INT(EPAPER_NONE, epaper_choose(&p, &none));
}

static void test_every_nth_counted_update_is_clean_and_fast_ones_dont_count(void)
{
    set_px(s_after, 0, 0, 0);
    epaper_change_t c = change();
    epaper_policy_t p = { .known = true, .clean_after = 4 };
    for (int i = 0; i < 3; i++) { /* the 1st to 3rd updates: fast */
        epaper_mode_t m = epaper_choose(&p, &c);
        TEST_ASSERT_EQUAL_INT(EPAPER_DU, m);
        epaper_after(&p, m, EPAPER_DONE);
    }
    TEST_ASSERT_EQUAL_UINT16(3, p.since_clean);
    p.fast = true; /* the menu: fast, not counted */
    epaper_after(&p, epaper_choose(&p, &c), EPAPER_DONE);
    TEST_ASSERT_EQUAL_UINT16(3, p.since_clean);
    p.fast = false;
    TEST_ASSERT_EQUAL_INT(EPAPER_CLEAN, epaper_choose(&p, &c)); /* the 4th counted */
    epaper_after(&p, EPAPER_CLEAN, EPAPER_DONE);
    TEST_ASSERT_EQUAL_UINT16(0, p.since_clean);

    p.clean_after = 1; /* every update clean */
    TEST_ASSERT_EQUAL_INT(EPAPER_CLEAN, epaper_choose(&p, &c));
}

/* T5b (T5 spec DT18): a PC session's redraws are fast and left out of the count; its clean comes by the clock. */
static void test_a_sessions_redraws_dont_count_and_its_clean_comes_by_the_clock(void)
{
    set_px(s_after, 0, 0, 0);
    epaper_change_t c = change();
    epaper_policy_t p = { .known = true, .clean_after = 2, .clean_ms = 1000 };
    epaper_session(&p, true, 600000); /* every 10 minutes */
    for (int i = 1; i <= 10; i++) { /* a reading every 5 s */
        p.now_ms = 1000 + (uint32_t)i * 5000;
        epaper_mode_t m = epaper_choose(&p, &c);
        TEST_ASSERT_EQUAL_INT(EPAPER_DU, m);
        epaper_after(&p, m, EPAPER_DONE);
    }
    TEST_ASSERT_EQUAL_UINT16(0, p.since_clean);
    p.now_ms = 1000 + 600000;
    TEST_ASSERT_EQUAL_INT(EPAPER_CLEAN, epaper_choose(&p, &c));
    epaper_after(&p, EPAPER_CLEAN, EPAPER_DONE);
    TEST_ASSERT_EQUAL_UINT32(601000, p.clean_ms);
    p.now_ms += 5000;
    TEST_ASSERT_EQUAL_INT(EPAPER_DU, epaper_choose(&p, &c));
}

static void test_a_sessions_clean_waits_for_a_change_and_the_menu_and_survives_the_clock_wrapping(void)
{
    epaper_change_t none = change();
    set_px(s_after, 3, 1, 0);
    epaper_change_t c = change();
    epaper_policy_t p = { .known = true, .clean_after = 60, .clean_ms = UINT32_MAX - 999 };
    epaper_session(&p, true, 60000);
    p.now_ms = 59000; /* the clock wrapped: 60 001 ms since the last clean */
    TEST_ASSERT_EQUAL_INT(EPAPER_NONE, epaper_choose(&p, &none));
    p.fast = true; /* the menu opened in the session: fast, no clean */
    TEST_ASSERT_EQUAL_INT(EPAPER_DU, epaper_choose(&p, &c));
    p.fast = false;
    TEST_ASSERT_EQUAL_INT(EPAPER_CLEAN, epaper_choose(&p, &c));
}

static void test_a_sessions_end_flashes_clean_and_counting_goes_on(void)
{
    set_px(s_after, 0, 0, 0);
    epaper_change_t c = change();
    epaper_policy_t p = { .known = true, .since_clean = 5, .clean_after = 60 };
    epaper_session(&p, true, 600000);
    TEST_ASSERT_FALSE(p.clean_next);
    epaper_session(&p, true, 600000); /* already on: nothing new */
    TEST_ASSERT_FALSE(p.clean_next);
    epaper_session(&p, false, 600000);
    TEST_ASSERT_FALSE(p.session);
    TEST_ASSERT_TRUE(p.clean_next);
    epaper_after(&p, epaper_choose(&p, &c), EPAPER_DONE);
    TEST_ASSERT_EQUAL_UINT16(0, p.since_clean);
    epaper_after(&p, epaper_choose(&p, &c), EPAPER_DONE);
    TEST_ASSERT_EQUAL_UINT16(1, p.since_clean); /* counted again */
    epaper_session(&p, false, 600000); /* already off: no clean */
    TEST_ASSERT_FALSE(p.clean_next);
}

/* Review Focus 3: no flash for an unchanged frame. */
static void test_the_count_reached_without_a_change_runs_nothing(void)
{
    epaper_change_t none = change();
    epaper_policy_t p = { .known = true, .since_clean = 59, .clean_after = 60 };
    TEST_ASSERT_EQUAL_INT(EPAPER_NONE, epaper_choose(&p, &none));
    set_px(s_after, 9, 3, 0);
    epaper_change_t c = change();
    TEST_ASSERT_EQUAL_INT(EPAPER_CLEAN, epaper_choose(&p, &c));
}

/* Review Focus 2: epdiy's RAM budget refused it, or a draw error came after the clear. */
static void test_a_failed_update_forgets_the_frame_and_keeps_the_count(void)
{
    epaper_policy_t p = { .known = true, .since_clean = 7, .clean_after = 60 };
    epaper_after(&p, EPAPER_GL16, EPAPER_FAILED);
    TEST_ASSERT_FALSE(p.known);
    TEST_ASSERT_EQUAL_UINT16(7, p.since_clean);
    epaper_change_t none = change();
    TEST_ASSERT_EQUAL_INT(EPAPER_CLEAN, epaper_choose(&p, &none));
    epaper_after(&p, EPAPER_CLEAN, EPAPER_FAILED);
    TEST_ASSERT_FALSE(p.known);
    epaper_after(&p, EPAPER_CLEAN, EPAPER_DONE);
    TEST_ASSERT_TRUE(p.known);
    TEST_ASSERT_EQUAL_UINT16(0, p.since_clean);
}

/* T4's review: in a sync, internal RAM sits under epdiy's budget and the update is refused before it touches the
 * panel; the frame there is still known, so the next update needn't flash. */
static void test_an_update_refused_before_it_drew_keeps_the_frame(void)
{
    epaper_policy_t p = { .known = true, .since_clean = 7, .clean_after = 60 };
    epaper_after(&p, EPAPER_GL16, EPAPER_REFUSED);
    TEST_ASSERT_TRUE(p.known);
    TEST_ASSERT_EQUAL_UINT16(7, p.since_clean);
    p.clean_next = true;
    epaper_after(&p, EPAPER_CLEAN, EPAPER_REFUSED);
    TEST_ASSERT_TRUE(p.clean_next); /* still owed */
    set_px(s_after, 2, 2, 0);
    epaper_change_t c = change();
    p.clean_next = false;
    TEST_ASSERT_EQUAL_INT(EPAPER_DU, epaper_choose(&p, &c));
}

/* T4b review: Back held through a clean update asked for a repair that the clean update's own bookkeeping wiped. */
static void test_a_clean_asked_for_during_an_update_survives_it(void)
{
    epaper_policy_t p = { .known = true, .clean_next = true, .clean_again = true, .clean_after = 60 };
    epaper_after(&p, EPAPER_CLEAN, EPAPER_DONE);
    TEST_ASSERT_TRUE(p.clean_next);
    TEST_ASSERT_FALSE(p.clean_again);
    p = (epaper_policy_t){ .known = true, .clean_again = true, .clean_after = 60 };
    epaper_after(&p, EPAPER_GL16, EPAPER_DONE);
    TEST_ASSERT_TRUE(p.clean_next);
    TEST_ASSERT_FALSE(p.clean_again);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_no_change_is_none_and_a_box_bounds_the_change);
    RUN_TEST(test_black_and_white_changes_are_du_others_gl16);
    RUN_TEST(test_an_unknown_frame_or_a_clean_request_is_clean_even_unchanged);
    RUN_TEST(test_every_nth_counted_update_is_clean_and_fast_ones_dont_count);
    RUN_TEST(test_a_sessions_redraws_dont_count_and_its_clean_comes_by_the_clock);
    RUN_TEST(test_a_sessions_clean_waits_for_a_change_and_the_menu_and_survives_the_clock_wrapping);
    RUN_TEST(test_a_sessions_end_flashes_clean_and_counting_goes_on);
    RUN_TEST(test_the_count_reached_without_a_change_runs_nothing);
    RUN_TEST(test_a_failed_update_forgets_the_frame_and_keeps_the_count);
    RUN_TEST(test_an_update_refused_before_it_drew_keeps_the_frame);
    RUN_TEST(test_a_clean_asked_for_during_an_update_survives_it);
    return UNITY_END();
}
