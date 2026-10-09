#include "gfx.h"
#include "gfx_fonts.h"
#include "gfx_icons_t5.h"
#include "unity.h"

static const gfx_font_t *const s_fonts[] = {
    &gfx_font_sans_12, &gfx_font_sans_16, &gfx_font_sans_20, &gfx_font_sans_bold_20, &gfx_font_sans_bold_28,
};
#define FONT_COUNT (sizeof(s_fonts) / sizeof(s_fonts[0]))

void setUp(void) {}
void tearDown(void) {}

static void test_fonts_cover_czech_and_symbols(void)
{
    for (size_t f = 0; f < FONT_COUNT; f++) {
        const char *p = "ÁáČčĎďÉéĚěÍíŇňÓóŘřŠšŤťÚúŮůÝýŽž°µ²€–…→";
        uint32_t cp;
        while ((cp = gfx_utf8_next(&p)) != 0) {
            TEST_ASSERT_TRUE_MESSAGE(gfx_font_has_glyph(s_fonts[f], cp), "glyph missing");
        }
    }
}

static void test_glyphs_are_sorted_for_binary_search(void)
{
    for (size_t f = 0; f < FONT_COUNT; f++) {
        for (uint16_t i = 1; i < s_fonts[f]->glyph_count; i++) {
            TEST_ASSERT_TRUE(s_fonts[f]->glyphs[i - 1].codepoint < s_fonts[f]->glyphs[i].codepoint);
        }
    }
}

static void test_metrics_grow_with_size(void)
{
    TEST_ASSERT_TRUE(gfx_font_sans_12.line_height < gfx_font_sans_16.line_height);
    TEST_ASSERT_TRUE(gfx_font_sans_16.line_height < gfx_font_sans_20.line_height);
    for (size_t f = 0; f < FONT_COUNT; f++) {
        TEST_ASSERT_TRUE(s_fonts[f]->ascent > 0);
        TEST_ASSERT_TRUE(s_fonts[f]->ascent < s_fonts[f]->line_height);
    }
}

/* T5 spec §6.3: the RLCD's fonts are 1 bpp, the T5's 4-bit, with the same coverage of Czech. */
static void test_each_font_records_its_depth(void)
{
    for (size_t f = 0; f < FONT_COUNT; f++) {
        TEST_ASSERT_EQUAL_UINT8(1, s_fonts[f]->bpp);
    }
    TEST_ASSERT_EQUAL_UINT8(4, gfx_font_t5_sans_26.bpp);
    TEST_ASSERT_TRUE(gfx_font_t5_sans_26.ascent < gfx_font_t5_sans_26.line_height);
    const char *p = "ÁáČčĎďÉéĚěÍíŇňÓóŘřŠšŤťÚúŮůÝýŽž°€–…";
    uint32_t cp;
    while ((cp = gfx_utf8_next(&p)) != 0) {
        TEST_ASSERT_TRUE_MESSAGE(gfx_font_has_glyph(&gfx_font_t5_sans_26, cp), "glyph missing");
    }
}

/* T5 spec §7.2: the T5's 4-bit set, one font per RLCD role at about 1.7× its size. */
static const gfx_font_t *const s_t5_text[] = { &gfx_font_t5_sans_20, &gfx_font_t5_sans_26, &gfx_font_t5_sans_34,
                                               &gfx_font_t5_bold_26, &gfx_font_t5_bold_34, &gfx_font_t5_bold_46 };
static const gfx_font_t *const s_t5_num[] = { &gfx_font_t5_num_80, &gfx_font_t5_num_120, &gfx_font_t5_num_180,
                                              &gfx_font_t5_num_220 };

static void test_the_t5_set_is_4bit_and_covers_its_charsets(void)
{
    for (size_t f = 0; f < sizeof(s_t5_text) / sizeof(s_t5_text[0]); f++) {
        TEST_ASSERT_EQUAL_UINT8(4, s_t5_text[f]->bpp);
        const char *p = "ÁáČčĎďÉéĚěÍíŇňÓóŘřŠšŤťÚúŮůÝýŽž°µ²€–…→";
        uint32_t cp;
        while ((cp = gfx_utf8_next(&p)) != 0) {
            TEST_ASSERT_TRUE_MESSAGE(gfx_font_has_glyph(s_t5_text[f], cp), "glyph missing");
        }
    }
    for (size_t f = 0; f < sizeof(s_t5_num) / sizeof(s_t5_num[0]); f++) {
        TEST_ASSERT_EQUAL_UINT8(4, s_t5_num[f]->bpp);
        TEST_ASSERT_TRUE(gfx_font_has_glyph(s_t5_num[f], '0'));
        TEST_ASSERT_TRUE(gfx_font_has_glyph(s_t5_num[f], 0x2212)); /* the minus sign */
        if (f > 0) {
            TEST_ASSERT_TRUE(s_t5_num[f - 1]->line_height < s_t5_num[f]->line_height);
        }
    }
    TEST_ASSERT_TRUE(gfx_font_t5_sans_20.line_height < gfx_font_t5_sans_26.line_height);
    TEST_ASSERT_TRUE(gfx_font_t5_sans_26.line_height < gfx_font_t5_sans_34.line_height);
}

static void test_the_t5_icons_come_in_three_4bit_sizes(void)
{
    const gfx_bitmap_t *const icons[] = { &gfx_icon_t5_thermometer_26, &gfx_icon_t5_wx_rain_40, &gfx_icon_t5_wifi_80,
                                          &gfx_icon_t5_stale_26, &gfx_icon_t5_sunset_80 };
    const int sizes[] = { 26, 40, 80, 26, 80 };
    for (size_t i = 0; i < sizeof(icons) / sizeof(icons[0]); i++) {
        TEST_ASSERT_EQUAL_UINT8(4, icons[i]->bpp);
        TEST_ASSERT_EQUAL_INT(sizes[i], icons[i]->width);
        TEST_ASSERT_EQUAL_INT(sizes[i], icons[i]->height);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fonts_cover_czech_and_symbols);
    RUN_TEST(test_glyphs_are_sorted_for_binary_search);
    RUN_TEST(test_metrics_grow_with_size);
    RUN_TEST(test_each_font_records_its_depth);
    RUN_TEST(test_the_t5_set_is_4bit_and_covers_its_charsets);
    RUN_TEST(test_the_t5_icons_come_in_three_4bit_sizes);
    return UNITY_END();
}
