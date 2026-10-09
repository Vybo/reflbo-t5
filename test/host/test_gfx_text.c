#include <string.h>

#include "gfx.h"
#include "unity.h"

/* Hand-made font: 'A' is a 3x3 arch, 'Ž' a 2x2 block, space has no ink. */
static const uint8_t s_bitmap[] = {
    0x40, 0xA0, 0xE0, /* 'A': .#. / #.# / ### */
    0xC0, 0xC0,       /* 'Ž': ## / ## */
};
static const gfx_glyph_t s_glyphs[] = {
    { 0x0020, 0, 0, 0, 0, 0, 2 },
    { 0x0041, 0, 3, 3, 0, -3, 4 },
    { 0x017D, 3, 2, 2, 0, -2, 3 },
};
static const gfx_font_t s_font = { s_bitmap, s_glyphs, 3, 3, 4, 1 };

static uint8_t s_buf[16]; /* 16x8 */
static gfx_fb_t s_fb;

void setUp(void)
{
    memset(s_buf, 0, sizeof(s_buf));
    gfx_fb_init(&s_fb, s_buf, 16, 8);
}

void tearDown(void) {}

static int count_black(void)
{
    int n = 0;
    for (int y = 0; y < s_fb.height; y++) {
        for (int x = 0; x < s_fb.width; x++) {
            n += gfx_get_pixel(&s_fb, x, y) ? 1 : 0;
        }
    }
    return n;
}

static void test_utf8_decodes_multibyte_characters(void)
{
    const char *s = "Ž°€A";
    TEST_ASSERT_EQUAL_HEX32(0x017D, gfx_utf8_next(&s));
    TEST_ASSERT_EQUAL_HEX32(0x00B0, gfx_utf8_next(&s));
    TEST_ASSERT_EQUAL_HEX32(0x20AC, gfx_utf8_next(&s));
    TEST_ASSERT_EQUAL_HEX32(0x0041, gfx_utf8_next(&s));
    TEST_ASSERT_EQUAL_HEX32(0, gfx_utf8_next(&s));
}

static void test_malformed_utf8_becomes_replacement_and_stops_at_nul(void)
{
    const char *cases[] = { "\xFF", "\xC5", "\xE2\x82", "\xC0\x80", "\xED\xA0\x80" };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        const char *s = cases[i];
        TEST_ASSERT_EQUAL_HEX32(0xFFFD, gfx_utf8_next(&s));
        while (*s != '\0') {
            TEST_ASSERT_EQUAL_HEX32(0xFFFD, gfx_utf8_next(&s));
        }
        TEST_ASSERT_EQUAL_HEX32(0, gfx_utf8_next(&s));
        TEST_ASSERT_EQUAL_PTR(cases[i] + strlen(cases[i]), s); /* stopped on the NUL, not past it */
    }
}

static void test_text_width_sums_advances(void)
{
    TEST_ASSERT_EQUAL_INT(10, gfx_text_width(&s_font, "A A"));
    TEST_ASSERT_EQUAL_INT(7, gfx_text_width(&s_font, "AŽ"));
    TEST_ASSERT_EQUAL_INT(0, gfx_text_width(&s_font, ""));
}

static void test_missing_glyph_uses_box_advance(void)
{
    TEST_ASSERT_TRUE(gfx_font_has_glyph(&s_font, 'A'));
    TEST_ASSERT_FALSE(gfx_font_has_glyph(&s_font, 'B'));
    TEST_ASSERT_EQUAL_INT(4, gfx_text_width(&s_font, "B"));
}

static void test_glyph_is_placed_relative_to_pen_and_baseline(void)
{
    TEST_ASSERT_EQUAL_INT(5, gfx_text(&s_fb, &s_font, 1, 4, "A", GFX_BLACK));
    TEST_ASSERT_TRUE(gfx_get_pixel(&s_fb, 2, 1));
    TEST_ASSERT_TRUE(gfx_get_pixel(&s_fb, 1, 2));
    TEST_ASSERT_TRUE(gfx_get_pixel(&s_fb, 3, 2));
    TEST_ASSERT_TRUE(gfx_get_pixel(&s_fb, 1, 3));
    TEST_ASSERT_TRUE(gfx_get_pixel(&s_fb, 2, 3));
    TEST_ASSERT_TRUE(gfx_get_pixel(&s_fb, 3, 3));
    TEST_ASSERT_EQUAL_INT(6, count_black());
}

/* No ink at all: a tall line box, so the fallback box is big enough to be hollow (6x6). */
static const gfx_font_t s_tall = { s_bitmap, s_glyphs, 3, 9, 12, 1 };

static void test_missing_glyph_draws_a_hollow_box(void)
{
    TEST_ASSERT_EQUAL_INT(8, gfx_text(&s_fb, &s_tall, 0, 7, "B", GFX_BLACK));
    TEST_ASSERT_TRUE(gfx_get_pixel(&s_fb, 1, 1));
    TEST_ASSERT_TRUE(gfx_get_pixel(&s_fb, 6, 6));
    TEST_ASSERT_FALSE(gfx_get_pixel(&s_fb, 3, 3));
    TEST_ASSERT_EQUAL_INT(20, count_black());
}

/* The review found the box's x was cut to int16_t, so far off-screen text drew phantom boxes. */
static void test_missing_glyph_far_right_draws_nothing(void)
{
    gfx_text(&s_fb, &s_font, 65536, 4, "B", GFX_BLACK);
    TEST_ASSERT_EQUAL_INT(0, count_black());
}

static void test_null_strings_are_empty(void)
{
    TEST_ASSERT_EQUAL_INT(0, gfx_text_width(&s_font, NULL));
    TEST_ASSERT_EQUAL_INT(3, gfx_text(&s_fb, &s_font, 3, 4, NULL, GFX_BLACK));
    gfx_text_in_rect(&s_fb, &s_font, (gfx_rect_t){ 0, 0, 16, 8 }, GFX_ALIGN_CENTER, NULL, GFX_BLACK);
    char out[8];
    TEST_ASSERT_EQUAL_INT(0, gfx_text_ellipsize(&s_font, NULL, 10, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("", out);
    TEST_ASSERT_EQUAL_INT(0, count_black());
}

/* 'A' (advance 4) and the ellipsis (advance 3). */
static const gfx_glyph_t s_dot_glyphs[] = {
    { 0x0041, 0, 3, 3, 0, -3, 4 },
    { 0x2026, 0, 1, 1, 0, -1, 3 },
};
static const gfx_font_t s_dots = { s_bitmap, s_dot_glyphs, 2, 3, 4, 1 };

static void test_ellipsize_keeps_text_that_fits_and_cuts_the_rest(void)
{
    char out[16];
    TEST_ASSERT_EQUAL_INT(8, gfx_text_ellipsize(&s_dots, "AA", 13, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("AA", out);
    TEST_ASSERT_EQUAL_INT(11, gfx_text_ellipsize(&s_dots, "AAAAAA", 13, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("AA\xE2\x80\xA6", out);
    TEST_ASSERT_EQUAL_INT(3, gfx_text_ellipsize(&s_dots, "AAAAAA", 4, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("\xE2\x80\xA6", out);
    TEST_ASSERT_EQUAL_INT(4, gfx_text_ellipsize(&s_dots, "AAAAAA", 100, out, 2)); /* one A fits the buffer */
    TEST_ASSERT_EQUAL_STRING("A", out);
}

static void test_text_in_rect_centres_horizontally_and_vertically(void)
{
    gfx_text_in_rect(&s_fb, &s_font, (gfx_rect_t){ 0, 0, 16, 8 }, GFX_ALIGN_CENTER, "A", GFX_BLACK);
    TEST_ASSERT_TRUE(gfx_get_pixel(&s_fb, 7, 2));
    TEST_ASSERT_TRUE(gfx_get_pixel(&s_fb, 6, 4));
    TEST_ASSERT_TRUE(gfx_get_pixel(&s_fb, 8, 4));
    TEST_ASSERT_EQUAL_INT(6, count_black());
}

static void test_text_in_rect_aligns_right(void)
{
    gfx_text_in_rect(&s_fb, &s_font, (gfx_rect_t){ 0, 0, 16, 8 }, GFX_ALIGN_RIGHT, "A", GFX_BLACK);
    TEST_ASSERT_TRUE(gfx_get_pixel(&s_fb, 13, 2));
    TEST_ASSERT_EQUAL_INT(6, count_black());
}

static void test_text_in_rect_clips_to_the_rect_and_restores_the_clip(void)
{
    gfx_text_in_rect(&s_fb, &s_font, (gfx_rect_t){ 0, 0, 5, 8 }, GFX_ALIGN_LEFT, "AA", GFX_BLACK);
    for (int y = 0; y < 8; y++) {
        for (int x = 5; x < 16; x++) {
            TEST_ASSERT_FALSE(gfx_get_pixel(&s_fb, x, y));
        }
    }
    TEST_ASSERT_TRUE(gfx_get_pixel(&s_fb, 4, 4));
    TEST_ASSERT_EQUAL_INT(16, s_fb.clip.w);
    TEST_ASSERT_EQUAL_INT(8, s_fb.clip.h);
}

/* T5 spec §6.3: a 4-bit font, rows of two pixels a byte (the first in the high nibble), coverage 0-15. */
static const uint8_t s_bitmap4[] = {
    0xF8,       /* 'A': 2×1, coverage 15 and 8 */
    0x70,       /* 'B': 1×1, coverage 7 */
};
static const gfx_glyph_t s_glyphs4[] = {
    { 0x0041, 0, 2, 1, 0, -1, 3 },
    { 0x0042, 1, 1, 1, 0, -1, 2 },
};
static const gfx_font_t s_font4 = { s_bitmap4, s_glyphs4, 2, 1, 2, 4 };

static void test_a_4bit_glyph_blends_its_coverage_on_4bpp(void)
{
    static uint8_t buf4[8];
    memset(buf4, 0xFF, sizeof(buf4));
    gfx_fb_t fb4;
    gfx_fb_init_fmt(&fb4, buf4, 8, 2, GFX_FMT_4BPP);
    int pen = gfx_text(&fb4, &s_font4, 0, 1, "A", GFX_BLACK);
    TEST_ASSERT_EQUAL_INT(3, pen);
    TEST_ASSERT_EQUAL_UINT8(0, gfx_get_level(&fb4, 0, 0));
    TEST_ASSERT_EQUAL_UINT8(7, gfx_get_level(&fb4, 1, 0));
    TEST_ASSERT_EQUAL_UINT8(15, gfx_get_level(&fb4, 2, 0));
}

/* Review Focus 5: on 1 bpp a 4-bit glyph inks from coverage 8, so a T5 font stays legible on the RLCD. */
static void test_a_4bit_glyph_on_1bpp_inks_from_half_coverage(void)
{
    gfx_text(&s_fb, &s_font4, 0, 1, "AB", GFX_BLACK);
    TEST_ASSERT_TRUE(gfx_get_pixel(&s_fb, 0, 0));
    TEST_ASSERT_TRUE(gfx_get_pixel(&s_fb, 1, 0));  /* coverage 8 */
    TEST_ASSERT_FALSE(gfx_get_pixel(&s_fb, 3, 0)); /* 'B', coverage 7 */
}

/* T5 spec §6.3: glyphs over 255 px (the T5's 220 px numerals and their advance). */
static void test_glyphs_wider_than_255_px_draw_and_advance(void)
{
    static uint8_t wide_bits[38];
    memset(wide_bits, 0xFF, sizeof(wide_bits));
    const gfx_glyph_t wide_glyphs[] = { { 0x0031, 0, 300, 1, 0, -1, 300 } };
    const gfx_font_t wide = { wide_bits, wide_glyphs, 1, 1, 2, 1 };
    static uint8_t buf[320 / 8];
    memset(buf, 0, sizeof(buf));
    gfx_fb_t fb;
    gfx_fb_init(&fb, buf, 320, 1);
    TEST_ASSERT_EQUAL_INT(300, gfx_text_width(&wide, "1"));
    TEST_ASSERT_EQUAL_INT(300, gfx_text(&fb, &wide, 0, 1, "1", GFX_BLACK));
    TEST_ASSERT_TRUE(gfx_get_pixel(&fb, 299, 0));
    TEST_ASSERT_FALSE(gfx_get_pixel(&fb, 300, 0));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_utf8_decodes_multibyte_characters);
    RUN_TEST(test_malformed_utf8_becomes_replacement_and_stops_at_nul);
    RUN_TEST(test_text_width_sums_advances);
    RUN_TEST(test_missing_glyph_uses_box_advance);
    RUN_TEST(test_glyph_is_placed_relative_to_pen_and_baseline);
    RUN_TEST(test_missing_glyph_draws_a_hollow_box);
    RUN_TEST(test_missing_glyph_far_right_draws_nothing);
    RUN_TEST(test_null_strings_are_empty);
    RUN_TEST(test_ellipsize_keeps_text_that_fits_and_cuts_the_rest);
    RUN_TEST(test_text_in_rect_centres_horizontally_and_vertically);
    RUN_TEST(test_text_in_rect_aligns_right);
    RUN_TEST(test_text_in_rect_clips_to_the_rect_and_restores_the_clip);
    RUN_TEST(test_a_4bit_glyph_blends_its_coverage_on_4bpp);
    RUN_TEST(test_a_4bit_glyph_on_1bpp_inks_from_half_coverage);
    RUN_TEST(test_glyphs_wider_than_255_px_draw_and_advance);
    return UNITY_END();
}
