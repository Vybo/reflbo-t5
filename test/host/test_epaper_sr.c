#include "epaper_sr.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

/* T5 spec §2.3: the bits go out OE first and LE last, so OE ends at QP7 and LE at QP0. */
static void test_each_signal_has_its_bit(void)
{
    const struct {
        epaper_sr_t s;
        uint8_t word;
    } cases[] = {
        { { .output_enable = true }, 0x80 }, { { .mode = true }, 0x40 },          { { .power_enable = true }, 0x20 },
        { { .stv = true }, 0x10 },           { { .neg_power = true }, 0x08 },     { { .pos_power = true }, 0x04 },
        { { .power_disable = true }, 0x02 }, { { .latch_enable = true }, 0x01 },
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        TEST_ASSERT_EQUAL_HEX8(cases[i].word, epaper_sr_word(&cases[i].s));
    }
}

static void test_off_leaves_only_the_logic_supply_disabled(void)
{
    epaper_sr_t off = epaper_sr_off();
    TEST_ASSERT_EQUAL_HEX8(0x02, epaper_sr_word(&off));
}

/* Review Focus 1: the negative rails come up before the positive ones. */
static void test_power_on_brings_the_negative_rails_up_first(void)
{
    epaper_sr_step_t s[EPAPER_SR_POWER_STEPS];
    TEST_ASSERT_EQUAL_INT(4, epaper_sr_poweron(epaper_sr_off(), s));
    TEST_ASSERT_TRUE(s[0].state.power_enable);
    TEST_ASSERT_FALSE(s[0].state.power_disable);
    TEST_ASSERT_FALSE(s[0].state.neg_power);
    TEST_ASSERT_FALSE(s[0].state.pos_power);
    TEST_ASSERT_EQUAL_UINT16(100, s[0].wait_us);
    TEST_ASSERT_TRUE(s[1].state.neg_power);
    TEST_ASSERT_FALSE(s[1].state.pos_power);
    TEST_ASSERT_EQUAL_UINT16(500, s[1].wait_us);
    TEST_ASSERT_TRUE(s[2].state.neg_power);
    TEST_ASSERT_TRUE(s[2].state.pos_power);
    TEST_ASSERT_EQUAL_UINT16(100, s[2].wait_us);
    TEST_ASSERT_TRUE(s[3].state.stv);
    TEST_ASSERT_EQUAL_UINT16(0, s[3].wait_us);
}

/* Review Focus 1: the positive rails drop first, and everything ends off. */
static void test_power_off_drops_the_positive_rails_first_and_ends_off(void)
{
    epaper_sr_step_t on[EPAPER_SR_POWER_STEPS], off[EPAPER_SR_POWER_STEPS];
    int n_on = epaper_sr_poweron(epaper_sr_off(), on);
    epaper_sr_t running = on[n_on - 1].state;
    running.output_enable = true; /* stopped mid-frame */
    running.mode = true;
    TEST_ASSERT_EQUAL_INT(3, epaper_sr_poweroff(running, off));
    TEST_ASSERT_FALSE(off[0].state.pos_power);
    TEST_ASSERT_TRUE(off[0].state.neg_power);
    TEST_ASSERT_EQUAL_UINT16(10, off[0].wait_us);
    TEST_ASSERT_FALSE(off[1].state.neg_power);
    TEST_ASSERT_EQUAL_UINT16(100, off[1].wait_us);
    epaper_sr_t want = epaper_sr_off();
    TEST_ASSERT_EQUAL_HEX8(epaper_sr_word(&want), epaper_sr_word(&off[2].state));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_each_signal_has_its_bit);
    RUN_TEST(test_off_leaves_only_the_logic_supply_disabled);
    RUN_TEST(test_power_on_brings_the_negative_rails_up_first);
    RUN_TEST(test_power_off_drops_the_positive_rails_first_and_ends_off);
    return UNITY_END();
}
