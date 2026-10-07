#include "rtc_sysclock.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

/* Review Focus 5: a power-on or EN reset starts the ESP32's clock at 1970; that is never a valid time. */
static void test_a_clock_from_power_on_is_invalid(void)
{
    TEST_ASSERT_FALSE(rtc_sysclock_valid(0));
    TEST_ASSERT_FALSE(rtc_sysclock_valid(86400 * 365)); /* a year after power-on, still 1971 */
}

static void test_the_boundary_is_2026(void)
{
    TEST_ASSERT_FALSE(rtc_sysclock_valid(RTC_SYSCLOCK_MIN_VALID - 1));
    TEST_ASSERT_TRUE(rtc_sysclock_valid(RTC_SYSCLOCK_MIN_VALID));
}

static void test_a_set_clock_is_valid(void)
{
    TEST_ASSERT_TRUE(rtc_sysclock_valid((time_t)1791379365)); /* 2026-10-07 */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_clock_from_power_on_is_invalid);
    RUN_TEST(test_the_boundary_is_2026);
    RUN_TEST(test_a_set_clock_is_valid);
    return UNITY_END();
}
