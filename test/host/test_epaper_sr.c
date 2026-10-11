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

/* Review Focus 3: the rest word is epdiy's lilygo_t5_47 power-off state: power disable set, PWR_EN, the
 * rails, STV, OE, MODE and LE clear. */
static void test_rest_matches_epdiys_power_off(void)
{
    epaper_sr_t off = epaper_sr_off();
    TEST_ASSERT_TRUE(off.power_disable);
    TEST_ASSERT_FALSE(off.power_enable);
    TEST_ASSERT_FALSE(off.pos_power);
    TEST_ASSERT_FALSE(off.neg_power);
    TEST_ASSERT_FALSE(off.stv);
    TEST_ASSERT_FALSE(off.output_enable);
    TEST_ASSERT_FALSE(off.mode);
    TEST_ASSERT_FALSE(off.latch_enable);
    TEST_ASSERT_EQUAL_HEX8(0x02, epaper_sr_word(&off));
}

/* The battery's divider hangs behind PWR_EN's switch (T5 spec §2.5 gotcha 24): the sense word turns PWR_EN on and leaves the
 * converter disabled, the rails, STV, OE, MODE and LE off. */
static void test_sense_powers_only_the_switched_supply(void)
{
    epaper_sr_t sense = epaper_sr_sense();
    TEST_ASSERT_TRUE(sense.power_enable);
    TEST_ASSERT_TRUE(sense.power_disable);
    TEST_ASSERT_FALSE(sense.pos_power);
    TEST_ASSERT_FALSE(sense.neg_power);
    TEST_ASSERT_FALSE(sense.stv);
    TEST_ASSERT_FALSE(sense.output_enable);
    TEST_ASSERT_FALSE(sense.mode);
    TEST_ASSERT_FALSE(sense.latch_enable);
    TEST_ASSERT_EQUAL_HEX8(0x22, epaper_sr_word(&sense));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_each_signal_has_its_bit);
    RUN_TEST(test_off_leaves_only_the_logic_supply_disabled);
    RUN_TEST(test_rest_matches_epdiys_power_off);
    RUN_TEST(test_sense_powers_only_the_switched_supply);
    return UNITY_END();
}
