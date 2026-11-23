#include "epaper_budget.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

/* T1 bring-up: config mode with the AP up, before epdiy came up (`heap`: 140943 free, largest 77824). */
static void test_config_mode_on_the_board_is_enough(void)
{
    TEST_ASSERT_TRUE(epaper_internal_ram_ok(140943, 77824));
}

/* Final review I1: epdiy aborts when an internal allocation fails; below the budget the update waits. */
static void test_too_little_free_skips_the_update(void)
{
    TEST_ASSERT_FALSE(epaper_internal_ram_ok(EPAPER_MIN_FREE_INTERNAL - 1, 32 * 1024));
}

static void test_a_fragmented_heap_skips_the_update(void)
{
    TEST_ASSERT_FALSE(epaper_internal_ram_ok(100 * 1024, EPAPER_MIN_BLOCK_INTERNAL - 1));
}

static void test_the_budget_itself_is_enough(void)
{
    TEST_ASSERT_TRUE(epaper_internal_ram_ok(EPAPER_MIN_FREE_INTERNAL, EPAPER_MIN_BLOCK_INTERNAL));
}

/* T3b review: with internal RAM short the LUT goes to PSRAM (P5), and epdiy aborts if PSRAM can't hold it
 * either (RainViewer's hour of 960 px frames in sync mode `always`): the update waits instead. */
static void test_the_lut_fits_in_internal_ram_or_psram(void)
{
    TEST_ASSERT_TRUE(epaper_lut_ram_ok(EPAPER_LUT_BYTES, 0));     /* internal */
    TEST_ASSERT_TRUE(epaper_lut_ram_ok(32 * 1024, 1600 * 1024));  /* PSRAM, as Wi-Fi leaves it at a sync */
    TEST_ASSERT_TRUE(epaper_lut_ram_ok(32 * 1024, EPAPER_LUT_BYTES));
}

static void test_no_room_for_the_lut_skips_the_update(void)
{
    TEST_ASSERT_FALSE(epaper_lut_ram_ok(EPAPER_LUT_BYTES - 1, EPAPER_LUT_BYTES - 1));
    TEST_ASSERT_FALSE(epaper_lut_ram_ok(32 * 1024, 0));
}

/* T5b's board check (T5 spec DT18): in a PC session internal RAM sat at 129 KB free (77 KB largest), the LUT went
 * there, and epdiy's update left 7.7 KB, at least 844 B, for Wi-Fi. With PSRAM free the LUT goes there now. */
static void test_the_lut_goes_to_psram_unless_internal_ram_has_plenty_to_spare(void)
{
    TEST_ASSERT_FALSE(epaper_lut_internal(129031, 77824, 319488));                          /* the session */
    TEST_ASSERT_TRUE(epaper_lut_internal(EPAPER_LUT_BYTES + EPAPER_LUT_SPARE_INTERNAL, 100 * 1024, 1600 * 1024));
    TEST_ASSERT_FALSE(epaper_lut_internal(EPAPER_LUT_BYTES + EPAPER_LUT_SPARE_INTERNAL - 1, 100 * 1024, 1600 * 1024));
    TEST_ASSERT_FALSE(epaper_lut_internal(200 * 1024, EPAPER_LUT_BYTES - 1, 1600 * 1024)); /* no block for it */
}

static void test_with_psram_full_the_lut_takes_internal_ram_rather_than_skip_the_update(void)
{
    TEST_ASSERT_TRUE(epaper_lut_internal(129031, 77824, 32 * 1024));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_config_mode_on_the_board_is_enough);
    RUN_TEST(test_too_little_free_skips_the_update);
    RUN_TEST(test_a_fragmented_heap_skips_the_update);
    RUN_TEST(test_the_budget_itself_is_enough);
    RUN_TEST(test_the_lut_fits_in_internal_ram_or_psram);
    RUN_TEST(test_no_room_for_the_lut_skips_the_update);
    RUN_TEST(test_the_lut_goes_to_psram_unless_internal_ram_has_plenty_to_spare);
    RUN_TEST(test_with_psram_full_the_lut_takes_internal_ram_rather_than_skip_the_update);
    return UNITY_END();
}
