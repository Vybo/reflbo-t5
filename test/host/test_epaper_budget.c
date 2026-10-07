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

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_config_mode_on_the_board_is_enough);
    RUN_TEST(test_too_little_free_skips_the_update);
    RUN_TEST(test_a_fragmented_heap_skips_the_update);
    RUN_TEST(test_the_budget_itself_is_enough);
    return UNITY_END();
}
