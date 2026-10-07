#include <string.h>

#include "epaper_frame.h"
#include "unity.h"

#define DW 16
#define DH 8

static uint8_t s_dst[DW / 2 * DH];
static uint8_t s_src_buf[4]; /* 8×4 at 1 bpp */
static gfx_fb_t s_src;

/* epdiy's 4 bpp layout: two pixels a byte, the even one in the low nibble; 0 black, 15 white. */
static uint8_t level(int x, int y)
{
    uint8_t b = s_dst[y * DW / 2 + x / 2];
    return x % 2 ? (uint8_t)(b >> 4) : (uint8_t)(b & 0x0F);
}

void setUp(void)
{
    memset(s_dst, 0x00, sizeof(s_dst)); /* black, so a missed white fill shows */
    gfx_fb_init(&s_src, s_src_buf, 8, 4);
    gfx_clear(&s_src, GFX_WHITE);
}

void tearDown(void) {}

static void test_everything_outside_the_frame_is_white(void)
{
    epaper_frame_blit_1bpp(&s_src, s_dst, DW, DH, 4, 2);
    for (int y = 0; y < DH; y++) {
        for (int x = 0; x < DW; x++) {
            TEST_ASSERT_EQUAL_HEX8(0x0F, level(x, y));
        }
    }
}

static void test_black_pixels_land_at_the_offset_in_their_nibble(void)
{
    gfx_pixel(&s_src, 0, 0, GFX_BLACK);
    gfx_pixel(&s_src, 1, 0, GFX_BLACK);
    gfx_pixel(&s_src, 7, 3, GFX_BLACK);
    epaper_frame_blit_1bpp(&s_src, s_dst, DW, DH, 4, 2);
    TEST_ASSERT_EQUAL_HEX8(0x00, s_dst[2 * DW / 2 + 2]); /* pixels (4, 2) and (5, 2) */
    TEST_ASSERT_EQUAL_HEX8(0x00, level(11, 5));
    TEST_ASSERT_EQUAL_HEX8(0x0F, level(10, 5));
    TEST_ASSERT_EQUAL_HEX8(0x0F, level(3, 2));
    TEST_ASSERT_EQUAL_HEX8(0x0F, level(6, 2));
}

static void test_an_odd_offset_splits_a_byte(void)
{
    gfx_pixel(&s_src, 0, 0, GFX_BLACK);
    epaper_frame_blit_1bpp(&s_src, s_dst, DW, DH, 3, 0);
    TEST_ASSERT_EQUAL_HEX8(0x0F, s_dst[1]); /* (2, 0) white in the low nibble, (3, 0) black in the high */
}

static void test_a_frame_past_the_edges_is_clipped(void)
{
    gfx_clear(&s_src, GFX_BLACK);
    epaper_frame_blit_1bpp(&s_src, s_dst, DW, DH, 12, 6);
    TEST_ASSERT_EQUAL_HEX8(0x00, level(12, 6));
    TEST_ASSERT_EQUAL_HEX8(0x00, level(15, 7));
    TEST_ASSERT_EQUAL_HEX8(0x0F, level(11, 7));
    TEST_ASSERT_EQUAL_HEX8(0x0F, level(12, 5));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_everything_outside_the_frame_is_white);
    RUN_TEST(test_black_pixels_land_at_the_offset_in_their_nibble);
    RUN_TEST(test_an_odd_offset_splits_a_byte);
    RUN_TEST(test_a_frame_past_the_edges_is_clipped);
    return UNITY_END();
}
