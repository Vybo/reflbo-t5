#include "pcf8563_regs.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define FRI_2026_09_25_204805 ((time_t)1790369285)

static void test_encodes_a_known_time(void)
{
    uint8_t r[PCF8563_TIME_LEN];
    pcf8563_encode_time(FRI_2026_09_25_204805, r);
    const uint8_t expected[PCF8563_TIME_LEN] = { 0x05, 0x48, 0x20, 0x25, 5 /* Friday */, 0x09, 0x26 };
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, r, PCF8563_TIME_LEN);
}

static void test_decodes_a_known_time(void)
{
    const uint8_t r[PCF8563_TIME_LEN] = { 0x05, 0x48, 0x20, 0x25, 5, 0x09, 0x26 };
    time_t t = 0;
    bool low = true;
    TEST_ASSERT_TRUE(pcf8563_decode_time(r, &t, &low));
    TEST_ASSERT_EQUAL_INT64(FRI_2026_09_25_204805, t);
    TEST_ASSERT_FALSE(low);
}

static void test_reports_the_voltage_low_flag(void)
{
    const uint8_t r[PCF8563_TIME_LEN] = { 0x80, 0x00, 0x00, 0x01, 6, 0x01, 0x00 }; /* VL set, 2000-01-01 */
    time_t t = 0;
    bool low = false;
    TEST_ASSERT_TRUE(pcf8563_decode_time(r, &t, &low));
    TEST_ASSERT_TRUE(low);
    TEST_ASSERT_EQUAL_INT64(PCF8563_MIN_TIME, t);
}

/* The century bit toggles when Years overflows; the RTC holds 2000-2099 only, so it means nothing here. */
static void test_ignores_the_century_bit(void)
{
    const uint8_t r[PCF8563_TIME_LEN] = { 0x05, 0x48, 0x20, 0x25, 5, 0x89 /* C set, September */, 0x26 };
    time_t t = 0;
    bool low = true;
    TEST_ASSERT_TRUE(pcf8563_decode_time(r, &t, &low));
    TEST_ASSERT_EQUAL_INT64(FRI_2026_09_25_204805, t);
}

static void test_round_trips_leap_day_and_range_ends(void)
{
    const time_t cases[] = { 1835438400 /* 2028-02-29T12:00Z */, PCF8563_MIN_TIME, PCF8563_MAX_TIME };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        uint8_t r[PCF8563_TIME_LEN];
        time_t back = 0;
        bool low = true;
        pcf8563_encode_time(cases[i], r);
        TEST_ASSERT_TRUE(pcf8563_decode_time(r, &back, &low));
        TEST_ASSERT_EQUAL_INT64(cases[i], back);
        TEST_ASSERT_FALSE(low);
    }
}

static void test_rejects_impossible_register_values(void)
{
    const uint8_t bad_bcd[PCF8563_TIME_LEN] = { 0x5A, 0x00, 0x00, 0x01, 6, 0x01, 0x00 };
    const uint8_t feb_30[PCF8563_TIME_LEN] = { 0x00, 0x00, 0x00, 0x30, 0, 0x02, 0x26 };
    const uint8_t month_13[PCF8563_TIME_LEN] = { 0x00, 0x00, 0x00, 0x01, 0, 0x13, 0x26 };
    time_t t;
    bool low;
    TEST_ASSERT_FALSE(pcf8563_decode_time(bad_bcd, &t, &low));
    TEST_ASSERT_FALSE(pcf8563_decode_time(feb_30, &t, &low));
    TEST_ASSERT_FALSE(pcf8563_decode_time(month_13, &t, &low));
}

static void test_clamps_times_outside_the_rtc_range(void)
{
    uint8_t early[PCF8563_TIME_LEN], min[PCF8563_TIME_LEN], late[PCF8563_TIME_LEN], max[PCF8563_TIME_LEN];
    pcf8563_encode_time(0, early);
    pcf8563_encode_time(PCF8563_MIN_TIME, min);
    pcf8563_encode_time(PCF8563_MAX_TIME + 86400, late);
    pcf8563_encode_time(PCF8563_MAX_TIME, max);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(min, early, PCF8563_TIME_LEN);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(max, late, PCF8563_TIME_LEN);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_encodes_a_known_time);
    RUN_TEST(test_decodes_a_known_time);
    RUN_TEST(test_reports_the_voltage_low_flag);
    RUN_TEST(test_ignores_the_century_bit);
    RUN_TEST(test_round_trips_leap_day_and_range_ends);
    RUN_TEST(test_rejects_impossible_register_values);
    RUN_TEST(test_clamps_times_outside_the_rtc_range);
    return UNITY_END();
}
