#include "ui_layout.h"
#include "ui_profile.h"
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

/* T0 (T5 spec §11): the T5 keeps the RLCD's geometry until T3 and differs only in what it has. */
static void test_the_t5_has_the_rlcd_geometry_and_none_of_its_capabilities(void)
{
    TEST_ASSERT_EQUAL_INT(400, ui_profile_t547.width);
    TEST_ASSERT_EQUAL_INT(300, ui_profile_t547.height);
    TEST_ASSERT_EQUAL_INT(20, ui_profile_t547.status_h);
    TEST_ASSERT_EQUAL_HEX32(0, ui_profile_t547.caps);
    TEST_ASSERT_EQUAL_HEX32(UI_CAP_ENV_SENSOR | UI_CAP_AUDIO | UI_CAP_RTC_TRIM | UI_CAP_RTC_ALARM_WAKE | UI_CAP_LPM_RATE,
                            ui_profile_rlcd42.caps);
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
    static const ui_profile_t wide = { "test", 960, 540, 34, 0 };
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

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_rlcd_is_the_default_profile);
    RUN_TEST(test_use_switches_the_profile_and_null_restores_the_rlcd);
    RUN_TEST(test_the_t5_has_the_rlcd_geometry_and_none_of_its_capabilities);
    RUN_TEST(test_the_rlcd_split_area_is_unchanged);
    RUN_TEST(test_the_split_area_and_status_bar_follow_the_profile);
    RUN_TEST(test_each_capability_has_a_name);
    return UNITY_END();
}
