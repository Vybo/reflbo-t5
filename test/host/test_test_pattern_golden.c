#include <stdio.h>
#include <string.h>

#include "gfx.h"
#include "gfx_test_pattern.h"
#include "unity.h"

/* The test pattern must match test/host/golden/test_pattern.pbm byte for byte. After an intentional
 * change: build-host/render_test_pattern test/host/golden/test_pattern.pbm, look at the PNG, commit. */

static uint8_t s_buf[400 * 300 / 8];
static uint8_t s_pbm[16000];
static uint8_t s_golden[16000];

void setUp(void) {}
void tearDown(void) {}

static void test_test_pattern_matches_the_golden_image(void)
{
    gfx_fb_t fb;
    gfx_fb_init(&fb, s_buf, 400, 300);
    gfx_draw_test_pattern(&fb);
    size_t n = gfx_pbm_encode(&fb, s_pbm, sizeof(s_pbm));

    FILE *f = fopen(GOLDEN_DIR "/test_pattern.pbm", "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, "golden image missing: " GOLDEN_DIR "/test_pattern.pbm");
    size_t golden = fread(s_golden, 1, sizeof(s_golden), f);
    fclose(f);

    if (golden != n || memcmp(s_golden, s_pbm, n) != 0) {
        FILE *out = fopen("test_pattern.actual.pbm", "wb");
        if (out != NULL) {
            fwrite(s_pbm, 1, n, out);
            fclose(out);
        }
    }
    TEST_ASSERT_EQUAL_INT((int)n, (int)golden);
    TEST_ASSERT_EQUAL_MEMORY(s_golden, s_pbm, n);
}

/* T5 spec §10.1: the T5's test pattern (960×540, 4 bpp: the RLCD's, a 16-step ramp and anti-aliased text and
 * an icon) must match test/host/golden/t5/test_pattern.pgm byte for byte. After an intentional change:
 * build-host/render_test_pattern --board t5 test/host/golden/t5/test_pattern.pgm, look at the PNG, commit. */
static uint8_t s_buf4[960 * 540 / 2];
static uint8_t s_pgm[960 * 540 + 32];
static uint8_t s_golden4[960 * 540 + 32];

static void test_t5_test_pattern_matches_the_golden_image(void)
{
    gfx_fb_t fb;
    gfx_fb_init_fmt(&fb, s_buf4, 960, 540, GFX_FMT_4BPP);
    gfx_draw_test_pattern_t5(&fb);
    size_t n = gfx_pgm_encode(&fb, s_pgm, sizeof(s_pgm));
    TEST_ASSERT_TRUE(n > 0);

    FILE *f = fopen(GOLDEN_DIR "/t5/test_pattern.pgm", "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, "golden image missing: " GOLDEN_DIR "/t5/test_pattern.pgm");
    size_t golden = fread(s_golden4, 1, sizeof(s_golden4), f);
    fclose(f);

    if (golden != n || memcmp(s_golden4, s_pgm, n) != 0) {
        FILE *out = fopen("t5_test_pattern.actual.pgm", "wb");
        if (out != NULL) {
            fwrite(s_pgm, 1, n, out);
            fclose(out);
        }
    }
    TEST_ASSERT_EQUAL_INT((int)n, (int)golden);
    TEST_ASSERT_EQUAL_MEMORY(s_golden4, s_pgm, n);
}

/* The extras draw only on 4 bpp: on 1 bpp the T5 pattern is the RLCD's, so the RLCD's golden holds. */
static void test_t5_test_pattern_on_1bpp_is_the_rlcds(void)
{
    static uint8_t a[400 * 300 / 8], b[400 * 300 / 8];
    gfx_fb_t fa, fb;
    gfx_fb_init(&fa, a, 400, 300);
    gfx_fb_init(&fb, b, 400, 300);
    gfx_draw_test_pattern(&fa);
    gfx_draw_test_pattern_t5(&fb);
    TEST_ASSERT_EQUAL_MEMORY(a, b, sizeof(a));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_test_pattern_matches_the_golden_image);
    RUN_TEST(test_t5_test_pattern_matches_the_golden_image);
    RUN_TEST(test_t5_test_pattern_on_1bpp_is_the_rlcds);
    return UNITY_END();
}
