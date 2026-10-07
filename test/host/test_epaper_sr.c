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

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_each_signal_has_its_bit);
    RUN_TEST(test_off_leaves_only_the_logic_supply_disabled);
    return UNITY_END();
}
