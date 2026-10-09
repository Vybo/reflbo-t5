#define _POSIX_C_SOURCE 200809L /* setenv() in fixture_zone() */

#include <stdio.h>
#include <string.h>

#include "gfx.h"
#include "golden_io.h"
#include "screen_fixtures.h"
#include "ui_profile.h"
#include "unity.h"

/* Each screen fixture must match test/host/golden/screen_<name>.pbm byte for byte, and each of
 * k_t5_screen_fixtures test/host/golden/t5/screen_<name>.pgm.gz once unpacked. After an intentional change:
 * build-host/render_screen [--board t5] <name> <golden> for each fixture, look at the PNGs
 * (python3 tools/render.py [--board t5]), commit. */

static uint8_t s_buf[960 * 540 / 2];
static uint8_t s_img[960 * 540 + 32];
static uint8_t s_golden[960 * 540 + 32];

void setUp(void) {}
void tearDown(void) {}

/* Renders name under the profile in use and compares it with dir/screen_<name><ext>; a mismatch leaves
 * <prefix>screen_<name>.actual<ext> in the working directory. */
static void check(const char *name, const char *dir, const char *prefix, const char *ext)
{
    const ui_profile_t *p = ui_profile();
    gfx_fb_t fb;
    gfx_fb_init_fmt(&fb, s_buf, p->width, p->height, p->format);
    TEST_ASSERT_TRUE_MESSAGE(fixture_screen(name, &fb), name);
    size_t n = p->format == GFX_FMT_4BPP ? gfx_pgm_encode(&fb, s_img, sizeof(s_img))
                                         : gfx_pbm_encode(&fb, s_img, sizeof(s_img));
    char path[256];
    snprintf(path, sizeof(path), "%s/screen_%s%s", dir, name, ext);
    size_t golden = golden_read(path, s_golden, sizeof(s_golden));
    TEST_ASSERT_TRUE_MESSAGE(golden > 0, path);
    if (golden != n || memcmp(s_golden, s_img, n) != 0) {
        char actual[96];
        snprintf(actual, sizeof(actual), "%sscreen_%s.actual%s", prefix, name, ext);
        golden_write(actual, s_img, n);
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE((int)n, (int)golden, path);
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(s_golden, s_img, n, path);
}

static void test_every_screen_matches_its_golden(void)
{
    for (size_t i = 0; i < sizeof(k_screen_fixtures) / sizeof(k_screen_fixtures[0]); i++) {
        check(k_screen_fixtures[i], GOLDEN_DIR, "", ".pbm");
    }
}

static void test_every_t5_screen_matches_its_golden(void)
{
    ui_profile_use(&ui_profile_t547);
    for (size_t i = 0; k_t5_screen_fixtures[i] != NULL; i++) {
        check(k_t5_screen_fixtures[i], GOLDEN_DIR "/t5", "t5_", ".pgm.gz");
    }
    ui_profile_use(NULL);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_every_screen_matches_its_golden);
    RUN_TEST(test_every_t5_screen_matches_its_golden);
    return UNITY_END();
}
