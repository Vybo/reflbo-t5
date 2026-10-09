# T2: Grayscale gfx Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** gfx draws grays on a 4 bpp framebuffer with anti-aliased fonts and icons, the T5 pushes a 4 bpp panel frame and shows a gray ramp and anti-aliased text on `panel test`, and screenshots of it come out as PGM (console) and 4-bit BMP (web) that match the host's golden.

**Architecture:** `gfx_fb_t` gains a format (1 bpp as today, 4 bpp in epdiy's layout); primitives pick a per-format pixel writer once per call, and a coverage writer blends anti-aliased ink. Fonts and icons gain a bit depth (0 still reads as 1, so the RLCD's generated sources stay valid) and wider glyph fields; the generators gain a 4-bit output. The T5's display keeps a 960×540 4 bpp panel frame: the UI's 400×300 1 bpp frame is composed into its middle until T3, and `panel test` draws the T5 test pattern (the RLCD's, plus a 16-step ramp and anti-aliased samples) straight into it.

**Tech Stack:** C17 (ESP-IDF 5.5.5 firmware and host tests with Unity), Python 3 (Pillow 12.3.0 through `uv` for the generators; stdlib for the PNG/PGM tools), epdiy 2.1.3.

**Spec:** [`docs/specs/2026-10-06-t5-board-design.md`](../specs/2026-10-06-t5-board-design.md) r4: §6 (grayscale gfx), §7.2 (the T5's sizes), §9 (`panel test`, `screenshot`), §10 (testing), §11 T2. Upstream: [`docs/specs/2026-09-25-firmware-design.md`](../specs/2026-09-25-firmware-design.md) §4.3–4.5, §15.

## Global Constraints

- "The 1 bpp code paths stay as they are, so the RLCD's goldens don't move. Primitives branch on the format once per call, not per pixel." (T5 spec §6.1) Every task keeps `test/host/golden/*.pbm` byte-identical (`git status --porcelain test/host/golden` lists nothing but new `t5/` files).
- `GFX_FMT_4BPP` is epdiy's layout: "two pixels a byte, the first in the low nibble, 0 = black, 15 = white" (§6.1).
- "`GFX_BLACK`, `GFX_WHITE` and `GFX_INVERT` stay; `GFX_GRAY(n)`, n = 1–14, joins them. On a 1 bpp buffer a gray is drawn as a 4×4 ordered dither." (§6.2)
- "Anti-aliased glyphs and icons blend their coverage with what's beneath: towards the ink's level on the background's. `GFX_INVERT` on 4 bpp inverts the level (15 − v)." (§6.2)
- "Glyph width, height and advance widen from `uint8_t` to `uint16_t`, and offsets from `int8_t` to `int16_t` … The RLCD's fonts are regenerated in the wider format; they draw the same." (§6.3)
- "`screenshot` prints a PGM (P5, 8-bit) between `-----BEGIN RLCD PGM-----` and `-----END RLCD PGM-----` on a 4 bpp board, and the PBM as today on 1 bpp." (§6.5) "`/api/screenshot.bmp` and `/api/preview.bmp` send a 4-bit BMP with a 16-gray palette on the T5." (§6.5)
- `[host]` components include no ESP-IDF headers; gfx stays pure C (AGENTS.md §5.3).
- Component sources are listed in `SRCS`, never globbed (AGENTS.md §8).
- The fork never flashes the RLCD board (DT8). The T5 is `/dev/cu.usbserial-52D60046741`, MAC `34:ab:95:5e:5d:58`; every esptool and flash call passes `-b 230400` (T5 gotcha T1).
- No AI or assistant attribution in commits, code or docs (AGENTS.md §8).

## Review Focus

1. **The RLCD's drawing changing through the shared primitives.** Expectation: every 1 bpp pixel is written exactly as before for WHITE, BLACK and INVERT. Pinned by the RLCD goldens in every task and by Task 1's test that a 1 bpp buffer drawn with each primitive equals the bytes the old code wrote.
2. **4 bpp odd widths and the last nibble.** Expectation: a 5-pixel-wide buffer has a 3-byte stride; drawing the last pixel touches only its own nibble, and clipping at a nibble boundary leaves the neighbour alone. Task 1's odd-width test.
3. **Blending at the extremes.** Expectation: coverage 0 leaves the pixel, 15 paints the ink exactly, INVERT at full coverage gives 15 − v, rounding is symmetric towards black and white. Task 1's blending tests.
4. **Regenerated RLCD fonts drawing differently.** Expectation: regenerating with the pinned Pillow changes only each font's struct line, not one bitmap byte. Task 3's diff check, then the goldens.
5. **A 4-bit font or icon on a 1 bpp buffer.** Expectation: a pixel inks when its coverage is 8 or more, so a T5 asset drawn on the RLCD is legible, not blank. Task 2's threshold test.

## Not in T2

- The T5's full font and icon sets at the §7.2 sizes, and the dense UI drawing in 4 bpp: T3. T2 generates one 4-bit font (`t5_sans_26`) and one 4-bit icon (`t5_thermometer_40`) for the test pattern.
- `components/ui/ui_widget.c` reads icon bits as 1 bpp (`bitmap_ink_box`, the bolt); T3 makes it depth-aware when it uses 4-bit icons.
- `/api/preview.bmp` stays 1-bit while the UI renders 1 bpp (until T3); the encoder already follows the framebuffer's format.
- `tools/docs_images.py` and T5 dashboard goldens: T3.
- Where the T5 uses gray (spec §6.4: the radar's rain, the solar chart's fill, the map, the status bar's separators): T3, with the dense UI.
- The previous frame across deep sleep (the T5's panel frame starts white after a wake until the next commit): T4.

## Carried from T1's review (deferred minors)

Fixed where T2 touches the code: minor 6 (`panel bench` leaves the border black) in Task 6. Minor 5's texts (`rtcchip.h` names the PCF8563; `idf.sh`'s port hint; `CONFIG_ESP32_REV_MIN_3` in the docs) in Task 8. Minors 1–4 stay deferred.

---

### Task 1: The 4 bpp framebuffer, grays and coverage

**Files:**
- Modify: `components/gfx/include/gfx.h`, `components/gfx/gfx.c`
- Test: `test/host/test_gfx.c`

**Interfaces:**
- Produces:
  - `typedef enum { GFX_FMT_1BPP = 0, GFX_FMT_4BPP = 1 } gfx_format_t;`, `gfx_fb_t.format` (`uint8_t`, the last member; 0 is 1 bpp)
  - `#define GFX_GRAY_BASE 16`, `#define GFX_GRAY(n) ((gfx_color_t)(GFX_GRAY_BASE + (n)))`
  - `size_t gfx_fb_size_fmt(gfx_format_t format, int16_t width, int16_t height);`
  - `void gfx_fb_init_fmt(gfx_fb_t *fb, uint8_t *buf, int16_t width, int16_t height, gfx_format_t format);`
  - `uint8_t gfx_get_level(const gfx_fb_t *fb, int x, int y);` (0 black … 15 white; 15 outside)
  - `void gfx_pixel_coverage(gfx_fb_t *fb, int x, int y, gfx_color_t color, uint8_t coverage);`

- [ ] **Step 1: The failing tests**

Append to `test/host/test_gfx.c`, before `int main(void)`:

```c
/* T5 spec §6.1: 4 bpp is epdiy's layout, two pixels a byte, the even one in the low nibble, 0 black. */
static uint8_t s_buf4[16];
static gfx_fb_t s_fb4;

static void fb4(int16_t w, int16_t h)
{
    memset(s_buf4, 0xFF, sizeof(s_buf4));
    gfx_fb_init_fmt(&s_fb4, s_buf4, w, h, GFX_FMT_4BPP);
}

static void test_sizes_follow_the_format(void)
{
    TEST_ASSERT_EQUAL_UINT32(9, gfx_fb_size_fmt(GFX_FMT_4BPP, 5, 3));
    TEST_ASSERT_EQUAL_UINT32(4, gfx_fb_size_fmt(GFX_FMT_1BPP, 9, 2));
    TEST_ASSERT_EQUAL_UINT32(gfx_fb_size(400, 300), gfx_fb_size_fmt(GFX_FMT_1BPP, 400, 300));
    fb4(5, 3);
    TEST_ASSERT_EQUAL_INT(3, s_fb4.stride);
    TEST_ASSERT_EQUAL_INT(GFX_FMT_4BPP, s_fb4.format);
}

static void test_4bpp_pixels_pack_two_a_byte_even_in_the_low_nibble(void)
{
    fb4(4, 1);
    gfx_pixel(&s_fb4, 0, 0, GFX_BLACK);
    TEST_ASSERT_EQUAL_HEX8(0xF0, s_buf4[0]);
    gfx_pixel(&s_fb4, 1, 0, GFX_GRAY(5));
    TEST_ASSERT_EQUAL_HEX8(0x50, s_buf4[0]);
    gfx_pixel(&s_fb4, 2, 0, GFX_GRAY(9));
    TEST_ASSERT_EQUAL_HEX8(0xF9, s_buf4[1]);
    TEST_ASSERT_EQUAL_UINT8(5, gfx_get_level(&s_fb4, 1, 0));
    TEST_ASSERT_EQUAL_UINT8(15, gfx_get_level(&s_fb4, 3, 0));
    TEST_ASSERT_EQUAL_UINT8(15, gfx_get_level(&s_fb4, 9, 0)); /* outside */
}

/* Review Focus 2: the last pixel of an odd width owns only its nibble; a clip at a nibble edge holds. */
static void test_4bpp_odd_width_and_a_clip_at_a_nibble_edge(void)
{
    fb4(5, 1);
    gfx_hline(&s_fb4, -3, 0, 20, GFX_BLACK);
    TEST_ASSERT_EQUAL_HEX8(0x00, s_buf4[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, s_buf4[1]);
    TEST_ASSERT_EQUAL_HEX8(0xF0, s_buf4[2]); /* the padding nibble stays white */
    TEST_ASSERT_EQUAL_HEX8(0xFF, s_buf4[3]);
    fb4(5, 1);
    gfx_set_clip(&s_fb4, (gfx_rect_t){ 1, 0, 3, 1 });
    gfx_hline(&s_fb4, 0, 0, 5, GFX_GRAY(4));
    TEST_ASSERT_EQUAL_HEX8(0x4F, s_buf4[0]);
    TEST_ASSERT_EQUAL_HEX8(0x44, s_buf4[1]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, s_buf4[2]);
}

static void test_4bpp_invert_flips_the_level_and_clear_fills_both_nibbles(void)
{
    fb4(4, 2);
    gfx_clear(&s_fb4, GFX_GRAY(3));
    TEST_ASSERT_EQUAL_HEX8(0x33, s_buf4[0]);
    gfx_pixel(&s_fb4, 0, 0, GFX_INVERT);
    TEST_ASSERT_EQUAL_UINT8(12, gfx_get_level(&s_fb4, 0, 0));
    gfx_pixel(&s_fb4, 0, 0, GFX_INVERT);
    TEST_ASSERT_EQUAL_UINT8(3, gfx_get_level(&s_fb4, 0, 0));
    gfx_clear(&s_fb4, GFX_INVERT);
    TEST_ASSERT_EQUAL_HEX8(0xCC, s_buf4[3]);
    gfx_clear(&s_fb4, GFX_WHITE);
    TEST_ASSERT_EQUAL_HEX8(0xFF, s_buf4[0]);
    gfx_clear(&s_fb4, GFX_BLACK);
    TEST_ASSERT_EQUAL_HEX8(0x00, s_buf4[0]);
}

static void test_get_pixel_and_level_read_both_formats(void)
{
    fb4(2, 1);
    gfx_pixel(&s_fb4, 0, 0, GFX_GRAY(7));
    gfx_pixel(&s_fb4, 1, 0, GFX_GRAY(8));
    TEST_ASSERT_TRUE(gfx_get_pixel(&s_fb4, 0, 0)); /* level < 8 reads as black */
    TEST_ASSERT_FALSE(gfx_get_pixel(&s_fb4, 1, 0));
    uint8_t one[2] = { 0 };
    gfx_fb_t fb1;
    gfx_fb_init(&fb1, one, 8, 2);
    TEST_ASSERT_EQUAL_INT(GFX_FMT_1BPP, fb1.format);
    gfx_pixel(&fb1, 3, 1, GFX_BLACK);
    TEST_ASSERT_EQUAL_UINT8(0, gfx_get_level(&fb1, 3, 1));
    TEST_ASSERT_EQUAL_UINT8(15, gfx_get_level(&fb1, 2, 1));
}

/* T5 spec §6.2: a gray on 1 bpp is a 4×4 ordered dither: (15 − n) / 15 of the pixels black. */
static void test_a_gray_on_1bpp_is_an_ordered_dither(void)
{
    const struct {
        int n;
        int black;
    } cases[] = { { 1, 15 }, { 8, 8 }, { 14, 2 } };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        uint8_t one[4] = { 0 };
        gfx_fb_t fb1;
        gfx_fb_init(&fb1, one, 4, 4);
        gfx_fill_rect(&fb1, (gfx_rect_t){ 0, 0, 4, 4 }, GFX_GRAY(cases[i].n));
        int black = 0;
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                black += gfx_get_pixel(&fb1, x, y) ? 1 : 0;
            }
        }
        TEST_ASSERT_EQUAL_INT(cases[i].black, black);
    }
}

/* Review Focus 3: coverage blends towards the ink; 0 leaves it, 15 paints it, INVERT aims at 15 − v. */
static void test_coverage_blends_towards_the_ink_on_4bpp(void)
{
    fb4(8, 1);
    gfx_pixel_coverage(&s_fb4, 0, 0, GFX_BLACK, 8);
    gfx_pixel_coverage(&s_fb4, 1, 0, GFX_BLACK, 5);
    gfx_pixel_coverage(&s_fb4, 2, 0, GFX_BLACK, 0);
    gfx_pixel_coverage(&s_fb4, 3, 0, GFX_BLACK, 15);
    TEST_ASSERT_EQUAL_UINT8(7, gfx_get_level(&s_fb4, 0, 0));
    TEST_ASSERT_EQUAL_UINT8(10, gfx_get_level(&s_fb4, 1, 0));
    TEST_ASSERT_EQUAL_UINT8(15, gfx_get_level(&s_fb4, 2, 0));
    TEST_ASSERT_EQUAL_UINT8(0, gfx_get_level(&s_fb4, 3, 0));
    gfx_pixel(&s_fb4, 4, 0, GFX_BLACK);
    gfx_pixel_coverage(&s_fb4, 4, 0, GFX_WHITE, 8); /* towards white: 0 + 15 × 8 / 15 */
    TEST_ASSERT_EQUAL_UINT8(8, gfx_get_level(&s_fb4, 4, 0));
    gfx_pixel(&s_fb4, 5, 0, GFX_GRAY(4));
    gfx_pixel_coverage(&s_fb4, 5, 0, GFX_INVERT, 15);
    TEST_ASSERT_EQUAL_UINT8(11, gfx_get_level(&s_fb4, 5, 0));
    gfx_pixel(&s_fb4, 6, 0, GFX_GRAY(4));
    gfx_pixel_coverage(&s_fb4, 6, 0, GFX_INVERT, 8); /* 4 + (11 − 4) × 8 / 15, rounded */
    TEST_ASSERT_EQUAL_UINT8(8, gfx_get_level(&s_fb4, 6, 0));
    gfx_set_clip(&s_fb4, (gfx_rect_t){ 0, 0, 7, 1 });
    gfx_pixel_coverage(&s_fb4, 7, 0, GFX_BLACK, 15); /* clipped */
    TEST_ASSERT_EQUAL_UINT8(15, gfx_get_level(&s_fb4, 7, 0));
}

/* Review Focus 5's base: on 1 bpp, coverage of 8 or more inks the pixel. */
static void test_coverage_on_1bpp_inks_from_half(void)
{
    uint8_t one[1] = { 0 };
    gfx_fb_t fb1;
    gfx_fb_init(&fb1, one, 8, 1);
    gfx_pixel_coverage(&fb1, 0, 0, GFX_BLACK, 7);
    gfx_pixel_coverage(&fb1, 1, 0, GFX_BLACK, 8);
    TEST_ASSERT_EQUAL_HEX8(0x40, one[0]);
}

/* Review Focus 1: the 1 bpp writer is the old one, byte for byte, for every primitive. */
static void test_1bpp_primitives_write_the_same_bytes(void)
{
    uint8_t one[2 * 6] = { 0 };
    gfx_fb_t fb1;
    gfx_fb_init(&fb1, one, 16, 6);
    gfx_hline(&fb1, 1, 0, 9, GFX_BLACK);
    gfx_vline(&fb1, 15, 0, 6, GFX_BLACK);
    gfx_fill_rect(&fb1, (gfx_rect_t){ 2, 2, 4, 2 }, GFX_BLACK);
    gfx_fill_rect(&fb1, (gfx_rect_t){ 3, 3, 4, 2 }, GFX_INVERT);
    gfx_pixel(&fb1, 1, 0, GFX_WHITE);
    const uint8_t want[] = { 0x3F, 0xC1, 0x00, 0x01, 0x3C, 0x01, 0x22, 0x01, 0x1E, 0x01, 0x00, 0x01 };
    TEST_ASSERT_EQUAL_HEX8_ARRAY(want, one, sizeof(want));
}
```

and these `RUN_TEST` lines before `return UNITY_END();`:

```c
    RUN_TEST(test_sizes_follow_the_format);
    RUN_TEST(test_4bpp_pixels_pack_two_a_byte_even_in_the_low_nibble);
    RUN_TEST(test_4bpp_odd_width_and_a_clip_at_a_nibble_edge);
    RUN_TEST(test_4bpp_invert_flips_the_level_and_clear_fills_both_nibbles);
    RUN_TEST(test_get_pixel_and_level_read_both_formats);
    RUN_TEST(test_a_gray_on_1bpp_is_an_ordered_dither);
    RUN_TEST(test_coverage_blends_towards_the_ink_on_4bpp);
    RUN_TEST(test_coverage_on_1bpp_inks_from_half);
    RUN_TEST(test_1bpp_primitives_write_the_same_bytes);
```

If `test_gfx.c` doesn't include `<string.h>`, add it.

`test_1bpp_primitives_write_the_same_bytes`' bytes are what today's code writes (worked by hand: the hline sets x 1–9 of row 0, the vline x 15 of every row, the black fill x 2–5 of rows 2–3, the inverting fill x 3–6 of rows 3–4, the white pixel clears x 1). Run it against the unchanged `gfx.c` first, once the header compiles, and confirm it passes before Step 4 changes the writers.

- [ ] **Step 2: Run them**

Run: `cd ~/reflbo-t5 && cmake --build build-host 2>&1 | grep -m3 -E "error"`
Expected: compile errors for `gfx_fb_init_fmt`, `GFX_FMT_4BPP`, `GFX_GRAY`, `gfx_get_level`, `gfx_pixel_coverage`.

- [ ] **Step 3: The header**

In `components/gfx/include/gfx.h`, replace the top comment and the colour, framebuffer and size declarations (from `/*\n * 1-bpp drawing` through `void gfx_clear(...)`) with:

```c
/*
 * Drawing (spec §4.3, T5 spec §6). Two framebuffer formats:
 * - GFX_FMT_1BPP: row-major, MSB = leftmost pixel, bit 1 = black, the PBM P4 raster layout;
 * - GFX_FMT_4BPP: epdiy's, row-major, two pixels a byte, the even one in the low nibble, 0 = black, 15 = white.
 * Primitives pick the format's writer once per call. Pure C with no ESP-IDF headers, so it builds on the host.
 */

typedef enum {
    GFX_WHITE = 0,
    GFX_BLACK = 1,
    GFX_INVERT = 2,
} gfx_color_t;

/* Grays between black and white, the panel's levels: n = 1 (darkest) to 14 (lightest). On a 1 bpp buffer a
 * gray is a 4×4 ordered dither, (15 − n) / 15 of its pixels black (T5 spec §6.2). */
#define GFX_GRAY_BASE 16
#define GFX_GRAY(n) ((gfx_color_t)(GFX_GRAY_BASE + (n)))

typedef enum {
    GFX_FMT_1BPP = 0,
    GFX_FMT_4BPP = 1,
} gfx_format_t;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
} gfx_rect_t;

typedef struct {
    uint8_t *buf;
    int16_t width;
    int16_t height;
    int16_t stride;  /* bytes per row: (width + 7) / 8 at 1 bpp, (width + 1) / 2 at 4 bpp */
    gfx_rect_t clip; /* drawing is limited to this rectangle */
    uint8_t format;  /* gfx_format_t; a zeroed struct is 1 bpp */
} gfx_fb_t;

size_t gfx_fb_size(int16_t width, int16_t height);                         /* 1 bpp */
void gfx_fb_init(gfx_fb_t *fb, uint8_t *buf, int16_t width, int16_t height); /* 1 bpp */
size_t gfx_fb_size_fmt(gfx_format_t format, int16_t width, int16_t height);
void gfx_fb_init_fmt(gfx_fb_t *fb, uint8_t *buf, int16_t width, int16_t height, gfx_format_t format);
void gfx_clear(gfx_fb_t *fb, gfx_color_t color); /* whole buffer; ignores the clip */
```

After `bool gfx_get_pixel(...)` add:

```c
/* The pixel's level: 0 black to 15 white (1 bpp: 0 or 15); 15 outside the buffer. gfx_get_pixel() is level < 8. */
uint8_t gfx_get_level(const gfx_fb_t *fb, int x, int y);
/* Ink `color` at `coverage` (0 none to 15 full) over what's there, clipped like gfx_pixel(). 4 bpp moves the
 * level towards the ink's (towards 15 − v for GFX_INVERT) by coverage / 15, rounded; 1 bpp inks the pixel
 * from coverage 8 (T5 spec §6.2). */
void gfx_pixel_coverage(gfx_fb_t *fb, int x, int y, gfx_color_t color, uint8_t coverage);
```

- [ ] **Step 4: The implementation**

In `components/gfx/gfx.c`, replace everything from `size_t gfx_fb_size(` through the end of `gfx_get_pixel()` with:

```c
size_t gfx_fb_size(int16_t width, int16_t height)
{
    return gfx_fb_size_fmt(GFX_FMT_1BPP, width, height);
}

size_t gfx_fb_size_fmt(gfx_format_t format, int16_t width, int16_t height)
{
    int stride = format == GFX_FMT_4BPP ? (width + 1) / 2 : (width + 7) / 8;
    return (size_t)stride * (size_t)height;
}

void gfx_fb_init(gfx_fb_t *fb, uint8_t *buf, int16_t width, int16_t height)
{
    gfx_fb_init_fmt(fb, buf, width, height, GFX_FMT_1BPP);
}

void gfx_fb_init_fmt(gfx_fb_t *fb, uint8_t *buf, int16_t width, int16_t height, gfx_format_t format)
{
    fb->buf = buf;
    fb->width = width;
    fb->height = height;
    fb->format = (uint8_t)format;
    fb->stride = (int16_t)(format == GFX_FMT_4BPP ? (width + 1) / 2 : (width + 7) / 8);
    gfx_reset_clip(fb);
}

/* 4×4 Bayer thresholds for grays on 1 bpp. */
static const uint8_t k_bayer4[4][4] = {
    { 0, 8, 2, 10 },
    { 12, 4, 14, 6 },
    { 3, 11, 1, 9 },
    { 15, 7, 13, 5 },
};

/* The level an ink paints: 0 black, 15 white, n for GFX_GRAY(n). */
static uint8_t ink_level(gfx_color_t color)
{
    if (color == GFX_BLACK) {
        return 0;
    }
    if (color >= GFX_GRAY_BASE) {
        int n = (int)color - GFX_GRAY_BASE;
        return (uint8_t)(n > 15 ? 15 : n);
    }
    return 15;
}

static void put1(gfx_fb_t *fb, int x, int y, gfx_color_t color)
{
    uint8_t *byte = &fb->buf[y * fb->stride + (x >> 3)];
    uint8_t mask = (uint8_t)(0x80u >> (x & 7));
    if (color >= GFX_GRAY_BASE) { /* (15 − n) / 15 of the pixels black, by the 4×4 thresholds */
        color = k_bayer4[y & 3][x & 3] * 15 < (15 - ink_level(color)) * 16 ? GFX_BLACK : GFX_WHITE;
    }
    switch (color) {
    case GFX_BLACK:
        *byte |= mask;
        break;
    case GFX_WHITE:
        *byte &= (uint8_t)~mask;
        break;
    case GFX_INVERT:
        *byte ^= mask;
        break;
    default:
        break;
    }
}

static void put4_level(gfx_fb_t *fb, int x, int y, uint8_t level)
{
    uint8_t *byte = &fb->buf[y * fb->stride + (x >> 1)];
    int shift = (x & 1) ? 4 : 0; /* the even pixel in the low nibble */
    *byte = (uint8_t)((*byte & ~(0x0Fu << shift)) | ((unsigned)level << shift));
}

static uint8_t level4(const gfx_fb_t *fb, int x, int y)
{
    return (uint8_t)((fb->buf[y * fb->stride + (x >> 1)] >> ((x & 1) ? 4 : 0)) & 0x0F);
}

static void put4(gfx_fb_t *fb, int x, int y, gfx_color_t color)
{
    put4_level(fb, x, y, color == GFX_INVERT ? (uint8_t)(15 - level4(fb, x, y)) : ink_level(color));
}

typedef void (*put_fn)(gfx_fb_t *fb, int x, int y, gfx_color_t color);

static put_fn writer(const gfx_fb_t *fb)
{
    return fb->format == GFX_FMT_4BPP ? put4 : put1;
}

void gfx_clear(gfx_fb_t *fb, gfx_color_t color)
{
    size_t size = (size_t)fb->stride * (size_t)fb->height;
    if (color == GFX_INVERT) { /* 1 bpp flips each bit, 4 bpp makes each nibble 15 − v */
        for (size_t i = 0; i < size; i++) {
            fb->buf[i] ^= 0xFF;
        }
        return;
    }
    if (fb->format == GFX_FMT_4BPP) {
        uint8_t v = ink_level(color);
        memset(fb->buf, (int)(v | (v << 4)), size);
        return;
    }
    if (color >= GFX_GRAY_BASE) {
        for (int y = 0; y < fb->height; y++) {
            for (int x = 0; x < fb->width; x++) {
                put1(fb, x, y, color);
            }
        }
        return;
    }
    memset(fb->buf, color == GFX_BLACK ? 0xFF : 0x00, size);
}
```

Keep `gfx_rect_intersect`, `gfx_set_clip` and `gfx_reset_clip` as they are (they move below `gfx_clear` unchanged). Then replace `gfx_pixel()`, `gfx_get_pixel()`, `gfx_hline()` and `gfx_vline()` with:

```c
static bool visible(const gfx_fb_t *fb, int x, int y)
{
    const gfx_rect_t *c = &fb->clip;
    if (x < c->x || y < c->y || x >= c->x + c->w || y >= c->y + c->h) {
        return false;
    }
    return x >= 0 && y >= 0 && x < fb->width && y < fb->height; /* a clip set by hand may reach outside */
}

void gfx_pixel(gfx_fb_t *fb, int x, int y, gfx_color_t color)
{
    if (visible(fb, x, y)) {
        writer(fb)(fb, x, y, color);
    }
}

uint8_t gfx_get_level(const gfx_fb_t *fb, int x, int y)
{
    if (x < 0 || y < 0 || x >= fb->width || y >= fb->height) {
        return 15;
    }
    if (fb->format == GFX_FMT_4BPP) {
        return level4(fb, x, y);
    }
    return (fb->buf[y * fb->stride + (x >> 3)] & (0x80u >> (x & 7))) ? 0 : 15;
}

bool gfx_get_pixel(const gfx_fb_t *fb, int x, int y)
{
    return gfx_get_level(fb, x, y) < 8;
}

void gfx_pixel_coverage(gfx_fb_t *fb, int x, int y, gfx_color_t color, uint8_t coverage)
{
    if (coverage == 0) {
        return;
    }
    if (fb->format != GFX_FMT_4BPP || coverage >= 15) {
        if (coverage >= 8) {
            gfx_pixel(fb, x, y, color);
        }
        return;
    }
    if (!visible(fb, x, y)) {
        return;
    }
    int d = level4(fb, x, y);
    int t = color == GFX_INVERT ? 15 - d : ink_level(color);
    int num = (t - d) * coverage;
    int step = num >= 0 ? (num + 7) / 15 : -((-num + 7) / 15); /* rounded half away from zero */
    put4_level(fb, x, y, (uint8_t)(d + step));
}

void gfx_hline(gfx_fb_t *fb, int x, int y, int w, gfx_color_t color)
{
    const gfx_rect_t *c = &fb->clip;
    if (y < c->y || y >= c->y + c->h || y < 0 || y >= fb->height) {
        return;
    }
    int x0 = max_int(max_int(x, c->x), 0);
    int x1 = min_int(min_int(x + w, c->x + c->w), fb->width);
    put_fn put = writer(fb);
    for (int i = x0; i < x1; i++) {
        put(fb, i, y, color);
    }
}

void gfx_vline(gfx_fb_t *fb, int x, int y, int h, gfx_color_t color)
{
    const gfx_rect_t *c = &fb->clip;
    if (x < c->x || x >= c->x + c->w || x < 0 || x >= fb->width) {
        return;
    }
    int y0 = max_int(max_int(y, c->y), 0);
    int y1 = min_int(min_int(y + h, c->y + c->h), fb->height);
    put_fn put = writer(fb);
    for (int i = y0; i < y1; i++) {
        put(fb, x, i, color);
    }
}
```

The rest of `gfx.c` (lines, rects, circles, triangle, bitmap) is unchanged: it goes through `gfx_pixel`, `gfx_hline` and `gfx_vline`.

- [ ] **Step 5: Run the tests**

Run: `cd ~/reflbo-t5 && cmake --build build-host 2>&1 | grep -E "error|warning" | head; ./build-host/test_gfx | tail -2; ctest --test-dir build-host -j8 2>&1 | grep "tests passed"; git status --porcelain test/host/golden`
Expected: no errors or warnings; `test_gfx` `0 Failures`; `100% tests passed`; no golden listed.

- [ ] **Step 6: Both firmware builds**

Run: `cd ~/reflbo-t5 && tools/idf.sh build 2>&1 | grep -E "warning:|error:|Project build complete"; REFLBO_BOARD=t5 tools/idf.sh build 2>&1 | grep -E "warning:|error:|Project build complete" | grep -v SPIRAM_MODE_OCT`
Expected: two "Project build complete", no warnings (GCC sees `-Wswitch` and conversions clang may not).

- [ ] **Step 7: Commit**

```bash
cd ~/reflbo-t5 && git add components/gfx/include/gfx.h components/gfx/gfx.c test/host/test_gfx.c
git commit -m "feat(gfx): a 4 bpp framebuffer, grays and coverage blending (T2)"
```

---

### Task 2: Wider fonts and bitmaps, 4-bit glyphs and icons

**Files:**
- Modify: `components/gfx/include/gfx_font.h`, `components/gfx/include/gfx.h` (`gfx_bitmap_t`), `components/gfx/gfx.c` (`gfx_bitmap`), `components/gfx/gfx_text.c` (`draw_glyph`)
- Test: `test/host/test_gfx_text.c`, `test/host/test_gfx.c`

**Interfaces:**
- Consumes: Task 1's `gfx_pixel_coverage()`.
- Produces:
  - `gfx_glyph_t`: `uint16_t width, height, advance; int16_t x_offset, y_offset;`
  - `gfx_font_t`: `uint16_t ascent, line_height; uint8_t bpp;` (the last member; 0 reads as 1)
  - `gfx_bitmap_t`: `uint16_t width, height; uint8_t bpp;` (the last member; 0 reads as 1)
  - 4-bit rows: whole bytes a row, two pixels a byte, the first in the high nibble, coverage 0–15.

- [ ] **Step 1: The failing tests**

Append to `test/host/test_gfx_text.c`, before `int main(void)`:

```c
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
```

with `RUN_TEST` lines for the three. Append to `test/host/test_gfx.c`:

```c
static void test_a_4bit_bitmap_blends_on_4bpp_and_thresholds_on_1bpp(void)
{
    static const uint8_t bits[] = { 0xF0, 0x80 }; /* 3×1: coverage 15, 0, 8 */
    const gfx_bitmap_t bm = { bits, 3, 1, 4 };
    fb4(4, 1);
    gfx_bitmap(&s_fb4, 0, 0, &bm, GFX_BLACK);
    TEST_ASSERT_EQUAL_UINT8(0, gfx_get_level(&s_fb4, 0, 0));
    TEST_ASSERT_EQUAL_UINT8(15, gfx_get_level(&s_fb4, 1, 0));
    TEST_ASSERT_EQUAL_UINT8(7, gfx_get_level(&s_fb4, 2, 0));
    uint8_t one[1] = { 0 };
    gfx_fb_t fb1;
    gfx_fb_init(&fb1, one, 8, 1);
    gfx_bitmap(&fb1, 0, 0, &bm, GFX_BLACK);
    TEST_ASSERT_EQUAL_HEX8(0xA0, one[0]);
}
```

with `RUN_TEST(test_a_4bit_bitmap_blends_on_4bpp_and_thresholds_on_1bpp);`.

- [ ] **Step 2: Run them**

Run: `cd ~/reflbo-t5 && cmake --build build-host 2>&1 | grep -m3 -E "error"`
Expected: errors on the extra initializer member (`excess elements in struct initializer`) for `s_font4`, `wide` and `bm`.

- [ ] **Step 3: The formats**

`components/gfx/include/gfx_font.h` becomes:

```c
#pragma once

#include <stdint.h>

/* Bitmap font generated by tools/fontgen.py (spec §4.4, T5 spec §6.3). */
typedef struct {
    uint32_t codepoint;
    uint32_t offset;  /* into gfx_font_t.bitmap */
    uint16_t width;   /* bitmap width in px */
    uint16_t height;  /* bitmap height in px */
    int16_t x_offset; /* left edge relative to the pen */
    int16_t y_offset; /* top edge relative to the baseline; negative is above */
    uint16_t advance; /* pen advance in px */
} gfx_glyph_t;

typedef struct {
    const uint8_t *bitmap;     /* glyph rows, each padded to whole bytes: at 1 bpp MSB first, 1 = ink; at 4 bpp
                                * two pixels a byte, the first in the high nibble, coverage 0-15 */
    const gfx_glyph_t *glyphs; /* sorted by codepoint */
    uint16_t glyph_count;
    uint16_t ascent;      /* baseline distance from the top of a line */
    uint16_t line_height; /* ascent + descent */
    uint8_t bpp;          /* 1 or 4; 0 (an initializer without it) reads as 1 */
} gfx_font_t;
```

In `components/gfx/include/gfx.h`, `gfx_bitmap_t` and its comment become:

```c
/* An image in the glyph format: rows padded to whole bytes, at 1 bpp MSB first with 1 = ink, at 4 bpp two
 * pixels a byte, the first in the high nibble, coverage 0-15. */
typedef struct {
    const uint8_t *bits;
    uint16_t width;
    uint16_t height;
    uint8_t bpp; /* 1 or 4; 0 (an initializer without it) reads as 1 */
} gfx_bitmap_t;

/* Draws the bitmap's ink in `color` with its top-left corner at (x, y), blending 4-bit coverage
 * (gfx_pixel_coverage()); other pixels are left alone. For an opaque image, fill its rectangle first. */
```

- [ ] **Step 4: The drawing**

In `components/gfx/gfx.c`, `gfx_bitmap()` becomes:

```c
void gfx_bitmap(gfx_fb_t *fb, int x, int y, const gfx_bitmap_t *bm, gfx_color_t color)
{
    if (bm == NULL || bm->bits == NULL) {
        return;
    }
    if (bm->bpp == 4) {
        int row_bytes = (bm->width + 1) / 2;
        for (int r = 0; r < bm->height; r++) {
            for (int c = 0; c < bm->width; c++) {
                uint8_t b = bm->bits[r * row_bytes + (c >> 1)];
                gfx_pixel_coverage(fb, x + c, y + r, color, (uint8_t)((c & 1) ? (b & 0x0F) : (b >> 4)));
            }
        }
        return;
    }
    int row_bytes = (bm->width + 7) / 8;
    for (int r = 0; r < bm->height; r++) {
        for (int c = 0; c < bm->width; c++) {
            if (bm->bits[r * row_bytes + (c >> 3)] & (0x80u >> (c & 7))) {
                gfx_pixel(fb, x + c, y + r, color);
            }
        }
    }
}
```

In `components/gfx/gfx_text.c`, `draw_glyph()` becomes:

```c
static void draw_glyph(gfx_fb_t *fb, const gfx_font_t *font, const gfx_glyph_t *g, int x, int baseline,
                       gfx_color_t color)
{
    const uint8_t *rows = font->bitmap + g->offset;
    int left = x + g->x_offset, top = baseline + g->y_offset;
    if (font->bpp == 4) { /* coverage, blended (T5 spec §6.2) */
        int row_bytes = (g->width + 1) / 2;
        for (int r = 0; r < g->height; r++) {
            for (int c = 0; c < g->width; c++) {
                uint8_t b = rows[r * row_bytes + (c >> 1)];
                gfx_pixel_coverage(fb, left + c, top + r, color, (uint8_t)((c & 1) ? (b & 0x0F) : (b >> 4)));
            }
        }
        return;
    }
    int row_bytes = (g->width + 7) / 8;
    for (int r = 0; r < g->height; r++) {
        for (int c = 0; c < g->width; c++) {
            if (rows[r * row_bytes + (c >> 3)] & (0x80u >> (c & 7))) {
                gfx_pixel(fb, left + c, top + r, color);
            }
        }
    }
}
```

- [ ] **Step 5: Run the tests and the builds**

Run: `cd ~/reflbo-t5 && cmake --build build-host 2>&1 | grep -E "error|warning" | head; ctest --test-dir build-host -j8 2>&1 | grep "tests passed"; git status --porcelain test/host/golden; tools/idf.sh build 2>&1 | grep -E "warning:|error:|Project build complete"; REFLBO_BOARD=t5 tools/idf.sh build 2>&1 | grep -E "warning:|error:|Project build complete" | grep -v SPIRAM_MODE_OCT`
Expected: `100% tests passed`; no golden listed; two "Project build complete", no warnings. The generated fonts and icons still compile: their initializers leave `bpp` at 0, which reads as 1.

- [ ] **Step 6: Commit**

```bash
cd ~/reflbo-t5 && git add components/gfx test/host/test_gfx.c test/host/test_gfx_text.c
git commit -m "feat(gfx): wider glyphs and 4-bit glyphs and icons, blended (T2)"
```

---

### Task 3: The generators' 4-bit output

**Files:**
- Modify: `tools/fontgen.py`, `tools/imggen.py`, `tools/gen_fonts.sh`, `tools/gen_icons.sh`
- Create: `assets/icons/icons_t5.txt`
- Regenerate: `components/gfx/fonts/*.c`, `components/gfx/icons/gfx_icons.c`, `components/gfx/include/gfx_icons.h`
- Create (generated): `components/gfx/fonts/gfx_font_t5_sans_26.c`, `components/gfx/icons/gfx_icons_t5.c`, `components/gfx/include/gfx_icons_t5.h`
- Modify: `components/gfx/include/gfx_fonts.h`, `components/gfx/CMakeLists.txt`
- Test: `tools/tests/test_fontgen.py`, `tools/tests/test_imggen.py`, `test/host/test_gfx_fonts.c`

**Interfaces:**
- Consumes: Task 2's formats.
- Produces: `extern const gfx_font_t gfx_font_t5_sans_26;` (4-bit, DejaVu Sans 26 px, charset `text`), `extern const gfx_bitmap_t gfx_icon_t5_thermometer_40;` (4-bit, in `gfx_icons_t5.h`); `fontgen.py --bpp 4`, `imggen.py --bpp 4 --out-c … --out-h …`.

- [ ] **Step 1: The failing tool tests**

In `tools/tests/test_fontgen.py`:
- `PackRowsTest` gains:

```python
    def test_4bit_rows_put_the_first_pixel_in_the_high_nibble(self):
        self.assertEqual(fontgen.pack_rows([[15, 8, 1]], bpp=4), bytes([0xF8, 0x10]))
```

- In `EmitCTest.test_emits_glyph_table_with_offsets`, the last assertion becomes `self.assertIn("const gfx_font_t gfx_font_tiny = { s_bitmap, s_glyphs, 2, 7, 9, 1 };", text)`.
- `EmitCTest` gains:

```python
    def test_a_4bit_font_records_its_depth_and_packs_nibbles(self):
        glyphs = [dict(cp=0x41, width=3, height=1, x=0, y=-1, advance=4, rows=[[15, 0, 8]])]
        text = fontgen.emit_c("aa", "Tiny.ttf", 8, glyphs, ascent=7, line_height=9, bpp=4)
        self.assertIn("0xF0, 0x80,", text)
        self.assertIn("const gfx_font_t gfx_font_aa = { s_bitmap, s_glyphs, 1, 7, 9, 4 };", text)

    def test_accepts_glyphs_over_255_px(self):
        glyph = dict(cp=0x31, width=300, height=1, x=-2, y=-300, advance=300, rows=[[1] * 300])
        self.assertIn("{ 0x0031, 0, 300, 1, -2, -300, 300 },",
                      fontgen.emit_c("big", "Big.ttf", 400, [glyph], ascent=300, line_height=310))
```

- `test_rejects_glyphs_that_do_not_fit_the_c_types` uses `width=70000` and `rows=[[1] * 70000]`.

In `tools/tests/test_imggen.py`:
- `test_c_defines_one_square_bitmap_per_icon_and_size`'s last assertion becomes `self.assertIn("const gfx_bitmap_t gfx_icon_x_9 = { s_x_9, 9, 9, 1 };", text)`.
- `EmitTest` gains:

```python
    def test_a_4bit_icon_records_its_depth_and_packs_nibbles(self):
        text = imggen.emit_c([("y_2", 2, [[15, 0], [8, 1]])], "Icons.ttf", bpp=4, header="gfx_icons_t5.h")
        self.assertIn('#include "gfx_icons_t5.h"', text)
        self.assertIn("0xF0, 0x81,", text)
        self.assertIn("const gfx_bitmap_t gfx_icon_y_2 = { s_y_2, 2, 2, 4 };", text)
```

Run: `cd ~/reflbo-t5/tools && python3 -m unittest tests.test_fontgen tests.test_imggen 2>&1 | tail -3`
Expected: `FAILED` (failures and errors: `pack_rows()` and `emit_c()` take no `bpp`, the struct lines lack `, 1`).

- [ ] **Step 2: `fontgen.py`**

- `pack_rows(rows)` becomes:

```python
def pack_rows(rows, bpp=1):
    """Rows of pixels -> bytes, each row padded to whole bytes. 1 bpp: 0/1, MSB first. 4 bpp: coverage
    0-15, two pixels a byte, the first in the high nibble."""
    out = bytearray()
    for row in rows:
        if bpp == 4:
            for start in range(0, len(row), 2):
                pair = list(row[start:start + 2]) + [0]
                out.append((pair[0] & 0x0F) << 4 | (pair[1] & 0x0F))
            continue
        for start in range(0, len(row), 8):
            byte = 0
            for i, bit in enumerate(row[start:start + 8]):
                if bit:
                    byte |= 0x80 >> i
            out.append(byte)
    return bytes(out)
```

- `render_glyph(font, cp)` becomes `render_glyph(font, cp, bpp=1)`. Keep the 1 bpp path exactly; add the 4 bpp one:

```python
def render_glyph(font, cp, bpp=1):
    """One glyph positioned relative to the pen at the baseline: at 1 bpp as FreeType renders it in
    monochrome, at 4 bpp anti-aliased, its coverage quantised to 0-15."""
    from PIL import Image, ImageDraw

    ch = chr(cp)
    mode = "1" if bpp == 1 else "L"
    advance = round(font.getlength(ch, mode=mode))
    left, top, right, bottom = font.getbbox(ch, mode=mode, anchor="ls")
    width, height = right - left, bottom - top
    if width <= 0 or height <= 0:
        return dict(cp=cp, width=0, height=0, x=0, y=0, advance=advance, rows=[])
    img = Image.new(mode, (width, height), 0)
    draw = ImageDraw.Draw(img)
    draw.fontmode = mode
    draw.text((-left, -top), ch, font=font, fill=1 if bpp == 1 else 255, anchor="ls")
    px = img.load()
    if bpp == 1:
        rows = [[1 if px[x, y] else 0 for x in range(width)] for y in range(height)]
    else:
        rows = [[(px[x, y] * 15 + 127) // 255 for x in range(width)] for y in range(height)]
    return trim_glyph(dict(cp=cp, width=width, height=height, x=left, y=top, advance=advance, rows=rows))
```

- `_check(glyph)` becomes:

```python
def _check(glyph):
    ok = (0 <= glyph["width"] <= 65535 and 0 <= glyph["height"] <= 65535 and -32768 <= glyph["x"] <= 32767
          and -32768 <= glyph["y"] <= 32767 and 0 <= glyph["advance"] <= 65535)
    if not ok:
        raise ValueError(f"glyph U+{glyph['cp']:04X} does not fit gfx_glyph_t: {glyph}")
```

- `emit_c(name, source, size, glyphs, ascent, line_height, licence=None)` gains `bpp=1`: `data += pack_rows(glyph["rows"], bpp)`; the first comment line reads `f"/* Generated by tools/fontgen.py from {source} at {size} px{', 4-bit coverage' if bpp == 4 else ''}. Do not edit; run tools/gen_fonts.sh. */"`; the font line ends `f"{ascent}, {line_height}, {bpp} }};"`.
- `main()` gains `parser.add_argument("--bpp", type=int, choices=(1, 4), default=1, help="1: monochrome; 4: anti-aliased coverage")`, passes `args.bpp` to `render_glyph(font, cp, args.bpp)` and `emit_c(..., args.licence, bpp=args.bpp)`.
- The docstring's first line: `Render a TrueType font into a 1-bpp or 4-bit anti-aliased bitmap font for components/gfx (spec §4.4, T5 spec §6.3).`

- [ ] **Step 3: `imggen.py`**

- `pack_rows(rows)` becomes the same `pack_rows(rows, bpp=1)` as in `fontgen.py` (copy it).
- `render_fitted(ttf, codepoint, size)` becomes `render_fitted(ttf, codepoint, size, bpp=1)`: its inner `ink(font_size)` becomes `ink(font_size, mode)` (canvas `Image.new(mode, …)`, `draw.fontmode = mode`, `fill=1 if mode == "1" else 255`); the probe stays `ink(size * 4, "1")`; the glyph is `ink(fit_font_size(...), "1" if bpp == 1 else "L")`; the final `img = Image.new(glyph.mode, (size, size), 0)`; the rows are `1 if px[x, y] else 0` at 1 bpp and `(px[x, y] * 15 + 127) // 255` at 4.
- `render_icon(font, codepoint, size)` becomes `render_icon(font, codepoint, size, bpp=1)`: mode `"1"` or `"L"`, `fill=1` or `255`, rows as above.
- `emit_c(icons, source, licence=None)` becomes `emit_c(icons, source, licence=None, bpp=1, header="gfx_icons.h")`: the include line is `f'#include "{header}"'`, the data `pack_rows(rows, bpp)`, the bitmap line `f"const gfx_bitmap_t gfx_icon_{symbol} = {{ s_{symbol}, {size}, {size}, {bpp} }};"`.
- `main()` gains `--bpp` (as in `fontgen.py`), passes it to both renderers and to `emit_c(..., bpp=args.bpp, header=pathlib.Path(args.out_h).name)`.

Run: `cd ~/reflbo-t5/tools && python3 -m unittest tests.test_fontgen tests.test_imggen 2>&1 | tail -1`
Expected: `OK`.

- [ ] **Step 4: The T5's first 4-bit assets**

Create `assets/icons/icons_t5.txt`:

```
# The T5's 4-bit icons (T5 spec §6.3), rendered into components/gfx/icons/gfx_icons_t5.c by tools/gen_icons.sh.
# T2 brings the test pattern's; T3 adds the T5's set at its sizes (§7.2).
# C name          Material Icons name, or wi:<name> from Weather Icons   sizes (px)
t5_thermometer    device_thermostat     40
```

Append to `tools/gen_icons.sh`:

```bash
# The T5's 4-bit icons (T5 spec §6.3).
uv run --quiet --python 3.13 --with-requirements tools/requirements.txt tools/imggen.py --bpp 4 \
    --ttf assets/icons/MaterialIcons-Regular.ttf --codepoints assets/icons/MaterialIcons-Regular.codepoints \
    --manifest assets/icons/icons_t5.txt --licence assets/icons/LICENSE-MaterialIcons.txt \
    --font wi=assets/icons/WeatherIcons-Regular.ttf,assets/icons/WeatherIcons-Regular.codepoints,assets/icons/LICENSE-WeatherIcons.txt \
    --out-c components/gfx/icons/gfx_icons_t5.c --out-h components/gfx/include/gfx_icons_t5.h
```

Append to `tools/gen_fonts.sh`:

```bash
# The T5's 4-bit fonts (T5 spec §6.3, §7.2); T2 brings the test pattern's, T3 the rest.
fontgen --ttf assets/fonts/DejaVuSans.ttf --size 26 --charset text --name t5_sans_26 --bpp 4 --licence assets/fonts/LICENSE-DejaVu.txt
```

- [ ] **Step 5: Regenerate, and prove the RLCD's assets draw the same**

Run:

```bash
cd ~/reflbo-t5 && tools/gen_fonts.sh && tools/gen_icons.sh
git diff --stat components/gfx
git diff -U0 components/gfx/fonts components/gfx/icons/gfx_icons.c components/gfx/include/gfx_icons.h | grep '^[-+]' | grep -v '^[-+][-+]' | grep -v 'gfx_font_t gfx_font_\|gfx_bitmap_t gfx_icon_' | head
```

Expected: every RLCD font file and `gfx_icons.c` changes only in its struct lines (the font line gains `, 1`, each icon line `, 1`); the last command prints nothing (Review Focus 4). If any bitmap byte changes, stop: restore the generated files with `git checkout components/gfx/fonts components/gfx/icons components/gfx/include/gfx_icons.h`, ledger it, and add the `, 1` by hand instead (a ruling), regenerating only the new T5 files.

- [ ] **Step 6: Wire the new assets**

`components/gfx/include/gfx_fonts.h` gains at the end:

```c
/* The T5's 4-bit anti-aliased fonts (T5 spec §6.3); T3 adds the rest of the set (§7.2). */
extern const gfx_font_t gfx_font_t5_sans_26;
```

`components/gfx/CMakeLists.txt`: the first comment line reads `# Drawing at 1 and 4 bpp, fonts and icons (spec §4.3–4.5, T5 spec §6). Pure C: also built on the host by test/host.`; add `"fonts/gfx_font_t5_sans_26.c"` after `"fonts/gfx_font_num_cb_130.c"` and `"icons/gfx_icons_t5.c"` after `"icons/gfx_icons.c"`.

`test/host/test_gfx_fonts.c` gains, before `int main(void)`:

```c
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
```

with `RUN_TEST(test_each_font_records_its_depth);`.

- [ ] **Step 7: Run everything**

Run: `cd ~/reflbo-t5 && cmake -S test/host -B build-host -G Ninja >/dev/null && cmake --build build-host 2>&1 | grep -E "error|warning" | head; ctest --test-dir build-host -j8 2>&1 | grep "tests passed"; git status --porcelain test/host/golden; tools/idf.sh build 2>&1 | grep -E "warning:|error:|Project build complete"; REFLBO_BOARD=t5 tools/idf.sh build 2>&1 | grep -E "warning:|error:|Project build complete" | grep -v SPIRAM_MODE_OCT`
Expected: `100% tests passed` (the tool tests included); no golden listed; two "Project build complete", no warnings. The host build's glob picks the new sources up after the `cmake -S` reconfigure.

- [ ] **Step 8: Commit**

```bash
cd ~/reflbo-t5 && git add tools/fontgen.py tools/imggen.py tools/gen_fonts.sh tools/gen_icons.sh tools/tests assets/icons/icons_t5.txt components/gfx test/host/test_gfx_fonts.c
git commit -m "feat(gfx): 4-bit fonts and icons from the generators; the T5's first (T2)"
```

---

### Task 4: PGM and 4-bit BMP, and the tools

**Files:**
- Modify: `components/gfx/include/gfx.h`, `components/gfx/gfx_pbm.c`, `components/gfx/gfx_bmp.c`
- Modify: `tools/pbm_png.py`, `tools/screenshot.py`, `tools/render.py`, `test/host/render_test_pattern.c`
- Test: `test/host/test_gfx.c`, `test/host/test_gfx_bmp_qr.c`, `tools/tests/test_pbm_png.py`, `tools/tests/test_screenshot.py`

**Interfaces:**
- Consumes: Task 1's `gfx_get_level()` and formats.
- Produces:
  - `size_t gfx_pgm_size(const gfx_fb_t *fb);`, `size_t gfx_pgm_encode(const gfx_fb_t *fb, uint8_t *out, size_t out_size);` (P5, 8 bits, level × 17; either format)
  - `gfx_bmp_*` on a 4 bpp framebuffer: a 4-bit BMP with a 16-gray palette (index i = gray i × 17)
  - `gfx_pbm_size/encode` return 0 for a 4 bpp framebuffer
  - `pbm_png.parse_pgm(data) -> (width, height, raster)`, `pbm_png.png_from_pgm(data)`, `pbm_png.png_from_image(data)` (P4 or P5)
  - `screenshot.BEGIN_PGM`, `screenshot.END_PGM`, `screenshot.extract_image(text) -> (kind, data)` with kind `"pbm"` or `"pgm"`; `extract_pbm()` stays
  - `render_test_pattern [--board t5] OUT` (Task 5 fills the T5 branch)

- [ ] **Step 1: The failing tests**

`test/host/test_gfx.c` gains:

```c
/* T5 spec §6.5: PGM P5, 8 bits a pixel, level × 17, from either format. */
static void test_pgm_has_header_and_8bit_levels(void)
{
    fb4(3, 1);
    gfx_pixel(&s_fb4, 0, 0, GFX_BLACK);
    gfx_pixel(&s_fb4, 1, 0, GFX_GRAY(7));
    uint8_t out[32];
    size_t n = gfx_pgm_encode(&s_fb4, out, sizeof(out));
    const char header[] = "P5\n3 1\n255\n";
    TEST_ASSERT_EQUAL_UINT32(sizeof(header) - 1 + 3, n);
    TEST_ASSERT_EQUAL_UINT32(n, gfx_pgm_size(&s_fb4));
    TEST_ASSERT_EQUAL_MEMORY(header, out, sizeof(header) - 1);
    TEST_ASSERT_EQUAL_UINT8(0, out[n - 3]);
    TEST_ASSERT_EQUAL_UINT8(119, out[n - 2]);
    TEST_ASSERT_EQUAL_UINT8(255, out[n - 1]);
    uint8_t one[1] = { 0x80 };
    gfx_fb_t fb1;
    gfx_fb_init(&fb1, one, 2, 1);
    n = gfx_pgm_encode(&fb1, out, sizeof(out));
    TEST_ASSERT_EQUAL_UINT8(0, out[n - 2]);
    TEST_ASSERT_EQUAL_UINT8(255, out[n - 1]);
    TEST_ASSERT_EQUAL_UINT32(0, gfx_pgm_encode(&fb1, out, 4)); /* too small */
    TEST_ASSERT_EQUAL_UINT32(0, gfx_pbm_encode(&s_fb4, out, sizeof(out))); /* PBM is 1 bpp only */
}
```

with `RUN_TEST(test_pgm_has_header_and_8bit_levels);`. In `test/host/test_gfx_bmp_qr.c`, before `int main(void)`:

```c
/* T5 spec §6.5: a 4 bpp framebuffer gives a 4-bit BMP with a 16-gray palette; its rows bottom-up, the
 * left pixel in the high nibble (BMP's order, the swap of epdiy's). */
static void test_bmp_of_a_4bpp_frame_is_4bit_gray(void)
{
    uint8_t buf[4];
    memset(buf, 0xFF, sizeof(buf));
    gfx_fb_t fb;
    gfx_fb_init_fmt(&fb, buf, 3, 2, GFX_FMT_4BPP);
    gfx_pixel(&fb, 0, 0, GFX_BLACK);
    gfx_pixel(&fb, 1, 0, GFX_GRAY(5));
    gfx_pixel(&fb, 2, 1, GFX_GRAY(9));
    uint8_t out[160];
    size_t n = gfx_bmp_encode(&fb, out, sizeof(out));
    TEST_ASSERT_EQUAL_UINT32(14 + 40 + 64 + 4 * 2, n);
    TEST_ASSERT_EQUAL_UINT32(n, gfx_bmp_size(&fb));
    TEST_ASSERT_EQUAL_UINT8(4, out[28]);   /* bits per pixel */
    TEST_ASSERT_EQUAL_UINT8(16, out[46]);  /* colours */
    TEST_ASSERT_EQUAL_UINT8(118, out[10]); /* where the pixels start */
    const uint8_t gray5[] = { 85, 85, 85, 0 };
    TEST_ASSERT_EQUAL_MEMORY(gray5, out + 54 + 5 * 4, 4);
    const uint8_t bottom[] = { 0xFF, 0x9F, 0, 0 }; /* row 1: 15 15 | 9 (pad) */
    const uint8_t top[] = { 0x05, 0xFF, 0, 0 };    /* row 0: 0 5 | 15 (pad) */
    TEST_ASSERT_EQUAL_MEMORY(bottom, out + 118, 4);
    TEST_ASSERT_EQUAL_MEMORY(top, out + 122, 4);
}
```

with its `RUN_TEST`; include `<string.h>` if missing.

`tools/tests/test_pbm_png.py` gains:

```python
class PgmTest(unittest.TestCase):
    def test_reads_an_8bit_pgm(self):
        self.assertEqual(pbm_png.parse_pgm(b"P5\n# c\n3 1\n255\n\x00\x77\xFF"), (3, 1, b"\x00\x77\xFF"))

    def test_rejects_16bit_and_short_pgms(self):
        with self.assertRaises(ValueError):
            pbm_png.parse_pgm(b"P5\n1 1\n65535\n\x00\x00")
        with self.assertRaises(ValueError):
            pbm_png.parse_pgm(b"P5\n3 1\n255\n\x00")

    def test_png_from_a_pgm_is_8bit_greyscale(self):
        png = pbm_png.png_from_image(b"P5\n3 1\n255\n\x00\x77\xFF")
        width, height, depth, colour = struct.unpack(">IIBB", png[16:26])
        self.assertEqual((width, height, depth, colour), (3, 1, 8, 0))
        idat_len = struct.unpack(">I", png[33:37])[0]
        self.assertEqual(zlib.decompress(png[41:41 + idat_len]), b"\x00\x00\x77\xFF")

    def test_png_from_image_still_reads_pbm(self):
        self.assertEqual(pbm_png.png_from_image(b"P4\n8 2\n\xF0\x0F"), pbm_png.png_from_pbm(b"P4\n8 2\n\xF0\x0F"))
```

`tools/tests/test_screenshot.py` gains:

```python
PGM = b"P5\n3 1\n255\n\x00\x77\xFF"


class ExtractImageTest(unittest.TestCase):
    def test_a_pgm_between_its_markers_is_decoded(self):
        text = "\n".join(["reflbo> screenshot", screenshot.BEGIN_PGM, base64.b64encode(PGM).decode(),
                          screenshot.END_PGM, "reflbo> "])
        self.assertEqual(screenshot.extract_image(text), ("pgm", PGM))

    def test_a_pbm_still_comes_through(self):
        encoded = base64.b64encode(PBM).decode()
        self.assertEqual(screenshot.extract_image(transcript([encoded])), ("pbm", PBM))

    def test_a_pgm_of_the_wrong_length_is_rejected(self):
        text = "\n".join([screenshot.BEGIN_PGM, base64.b64encode(PGM + b"\x00").decode(), screenshot.END_PGM])
        with self.assertRaises(ValueError):
            screenshot.extract_image(text)
```

Run: `cd ~/reflbo-t5 && cmake --build build-host 2>&1 | grep -m2 error; cd tools && python3 -m unittest tests.test_pbm_png tests.test_screenshot 2>&1 | tail -1`
Expected: undeclared `gfx_pgm_encode`/`gfx_pgm_size`; Python `FAILED` (no `parse_pgm`, `png_from_image`, `BEGIN_PGM`, `extract_image`).

- [ ] **Step 2: The encoders**

`gfx.h`: after the PBM declarations add:

```c
/* PGM P5 image of the framebuffer, either format: "P5\n<w> <h>\n255\n", then a byte a pixel, level × 17
 * (T5 spec §6.5). gfx_pbm_*() take 1 bpp only (4 bpp: size and encode return 0). */
size_t gfx_pgm_size(const gfx_fb_t *fb);
size_t gfx_pgm_encode(const gfx_fb_t *fb, uint8_t *out, size_t out_size); /* 0 if out_size is too small */
```

and the BMP comment becomes `/* BMP of the framebuffer, for browsers (the web UI's preview and screenshot): 1-bit at 1 bpp, 4-bit with a 16-gray palette at 4 bpp (T5 spec §6.5). */`.

`gfx_pbm.c`: `gfx_pbm_size()` and `gfx_pbm_encode()` start with `if (fb->format == GFX_FMT_4BPP) { return 0; }`; append:

```c
static int pgm_header(const gfx_fb_t *fb, char *out, size_t size)
{
    return snprintf(out, size, "P5\n%d %d\n255\n", fb->width, fb->height);
}

size_t gfx_pgm_size(const gfx_fb_t *fb)
{
    char header[32];
    return (size_t)pgm_header(fb, header, sizeof(header)) + (size_t)fb->width * (size_t)fb->height;
}

size_t gfx_pgm_encode(const gfx_fb_t *fb, uint8_t *out, size_t out_size)
{
    char header[32];
    int n = pgm_header(fb, header, sizeof(header));
    size_t total = gfx_pgm_size(fb);
    if (n <= 0 || out_size < total) {
        return 0;
    }
    memcpy(out, header, (size_t)n);
    uint8_t *p = out + n;
    for (int y = 0; y < fb->height; y++) {
        for (int x = 0; x < fb->width; x++) {
            *p++ = (uint8_t)(gfx_get_level(fb, x, y) * 17);
        }
    }
    return total;
}
```

`gfx_bmp.c` becomes:

```c
#include <string.h>

#include "gfx.h"

/* A BMP (spec §4.3, T5 spec §6.5) for the web UI's preview and screenshot: BITMAPFILEHEADER,
 * BITMAPINFOHEADER, a palette, then the rows bottom-up, each padded to 4 bytes. At 1 bpp a two-colour
 * palette with index 1 black lets the canonical rows (1 = black, MSB first) go in unchanged. At 4 bpp a
 * 16-gray palette (index i is gray i × 17) takes the levels as they are, each byte's nibbles swapped: BMP
 * puts the left pixel in the high nibble, epdiy in the low. */

static bool gray(const gfx_fb_t *fb)
{
    return fb->format == GFX_FMT_4BPP;
}

static size_t headers_size(const gfx_fb_t *fb)
{
    return 14 + 40 + (gray(fb) ? 16 * 4 : 2 * 4);
}

static size_t row_size(const gfx_fb_t *fb)
{
    return ((size_t)fb->stride + 3u) & ~(size_t)3u;
}

size_t gfx_bmp_size(const gfx_fb_t *fb)
{
    return headers_size(fb) + row_size(fb) * (size_t)fb->height;
}

static void put16(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *p, uint32_t v)
{
    put16(p, v & 0xFFFFu);
    put16(p + 2, v >> 16);
}

size_t gfx_bmp_encode(const gfx_fb_t *fb, uint8_t *out, size_t out_size)
{
    size_t total = gfx_bmp_size(fb), row = row_size(fb), headers = headers_size(fb);
    if (out_size < total) {
        return 0;
    }
    memset(out, 0, headers);
    out[0] = 'B';
    out[1] = 'M';
    put32(out + 2, (uint32_t)total);
    put32(out + 10, (uint32_t)headers); /* where the pixels start */
    put32(out + 14, 40);                /* BITMAPINFOHEADER */
    put32(out + 18, (uint32_t)fb->width);
    put32(out + 22, (uint32_t)fb->height); /* positive: bottom-up rows */
    put16(out + 26, 1);                    /* planes */
    put16(out + 28, gray(fb) ? 4 : 1);     /* bits per pixel */
    put32(out + 34, (uint32_t)(row * (size_t)fb->height));
    put32(out + 38, 2835); /* 72 dpi */
    put32(out + 42, 2835);
    put32(out + 46, gray(fb) ? 16 : 2); /* colours in the palette */
    if (gray(fb)) {
        for (int i = 0; i < 16; i++) {
            uint8_t v = (uint8_t)(i * 17);
            uint8_t *entry = out + 54 + i * 4;
            entry[0] = entry[1] = entry[2] = v;
        }
    } else {
        memcpy(out + 54, "\xFF\xFF\xFF\x00\x00\x00\x00\x00", 8); /* 0 white, 1 black */
    }
    for (int y = 0; y < fb->height; y++) {
        uint8_t *dst = out + headers + row * (size_t)(fb->height - 1 - y);
        const uint8_t *src = fb->buf + (size_t)y * (size_t)fb->stride;
        if (gray(fb)) {
            for (int i = 0; i < fb->stride; i++) {
                dst[i] = (uint8_t)((src[i] << 4) | (src[i] >> 4));
            }
        } else {
            memcpy(dst, src, (size_t)fb->stride);
        }
        memset(dst + fb->stride, 0, row - (size_t)fb->stride);
    }
    return total;
}
```

(`gfx_bmp.c` needs `#include <stdbool.h>` through `gfx.h`, which has it.)

- [ ] **Step 3: The tools**

`tools/pbm_png.py`: the docstring's first line becomes `Convert binary PBM (P4) and 8-bit PGM (P5) images to PNG using only the standard library.`; factor the header reading out of `parse_pbm()`:

```python
def _header(data, magic, count):
    """The first `count` whitespace-separated header fields after skipping comments, and the raster's start."""
    fields = []
    pos = 0
    while len(fields) < count:
        while pos < len(data) and data[pos:pos + 1].isspace():
            pos += 1
        if data[pos:pos + 1] == b"#":
            while pos < len(data) and data[pos:pos + 1] not in (b"\n", b"\r"):
                pos += 1
            continue
        start = pos
        while pos < len(data) and not data[pos:pos + 1].isspace():
            pos += 1
        if start == pos:
            raise ValueError("truncated header")
        fields.append(data[start:pos])
    if fields[0] != magic:
        raise ValueError(f"not a {magic.decode()} image")
    return fields, pos + 1  # the single whitespace after the last field


def parse_pbm(data):
    """Returns (width, height, raster) for a P4 image; the raster is row-major, MSB first, 1 = black."""
    fields, pos = _header(data, b"P4", 3)
    width, height = int(fields[1]), int(fields[2])
    size = (width + 7) // 8 * height
    raster = data[pos:pos + size]
    if len(raster) != size:
        raise ValueError("truncated PBM raster")
    return width, height, raster


def parse_pgm(data):
    """Returns (width, height, raster) for an 8-bit P5 image; a byte a pixel, 0 = black."""
    fields, pos = _header(data, b"P5", 4)
    width, height, maxval = int(fields[1]), int(fields[2]), int(fields[3])
    if maxval > 255:
        raise ValueError("only 8-bit PGMs")
    raster = data[pos:pos + width * height]
    if len(raster) != width * height:
        raise ValueError("truncated PGM raster")
    return width, height, raster
```

Keep `png_from_pbm()`, but take its chunk/PNG assembly into `_png(width, height, depth, raw)`; add:

```python
def png_from_pgm(data):
    width, height, raster = parse_pgm(data)
    raw = bytearray()
    for y in range(height):
        raw.append(0)  # filter: none
        raw += raster[y * width:(y + 1) * width]
    return _png(width, height, 8, bytes(raw))


def png_from_image(data):
    """PNG from a P4 PBM or a P5 PGM."""
    return png_from_pgm(data) if data[:2] == b"P5" else png_from_pbm(data)
```

with `_png()`:

```python
def _png(width, height, depth, raw):
    def chunk(tag, body):
        return struct.pack(">I", len(body)) + tag + body + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", width, height, depth, 0, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 9))
            + chunk(b"IEND", b""))
```

and `png_from_pbm()` ending in `return _png(width, height, 1, bytes(raw))`; `main()` writes `png_from_image(...)`.

`tools/screenshot.py`:
- Docstring: `Grab the board's framebuffer over USB as a PNG (spec §15): a PBM from 1 bpp, a PGM from the T5's 4 bpp (T5 spec §6.5). … Writes OUT.png and the raw OUT.pbm or OUT.pgm. A T5 screenshot is about 690 KB of base64 at 115200 baud: give it -t 120.`
- After `END`: `BEGIN_PGM = "-----BEGIN RLCD PGM-----"` and `END_PGM = "-----END RLCD PGM-----"`.
- Replace `extract_pbm()` with:

```python
def _between(lines, begin, end):
    start = next(i for i, line in enumerate(lines) if line.endswith(begin))
    stop = next(i for i in range(start + 1, len(lines)) if lines[i] == end)
    return lines[start + 1:stop]


def extract_image(text):
    """Returns ("pbm" or "pgm", data) for the image carried between markers; raises ValueError."""
    lines = [line.strip() for line in text.splitlines()]
    for kind, begin, end in (("pbm", BEGIN, END), ("pgm", BEGIN_PGM, END_PGM)):
        try:
            body = _between(lines, begin, end)
        except StopIteration:
            continue
        try:
            data = base64.b64decode("".join(body), validate=True)
        except binascii.Error as err:
            raise ValueError(f"corrupt screenshot data: {err}") from None
        if kind == "pbm":
            width, height, raster = pbm_png.parse_pbm(data)
            header = len(f"P4\n{width} {height}\n")
        else:
            width, height, raster = pbm_png.parse_pgm(data)
            header = len(f"P5\n{width} {height}\n255\n")
        if len(data) != header + len(raster):
            raise ValueError("screenshot data has the wrong length")  # e.g. base64-looking noise got in
        return kind, data
    raise ValueError("no complete screenshot in the console output")


def extract_pbm(text):
    """The PBM carried between the markers (1 bpp boards); raises ValueError."""
    kind, data = extract_image(text)
    if kind != "pbm":
        raise ValueError("the screenshot is a PGM")
    return data
```

- In `main()`: `--compare` help `"PBM or PGM the screenshot must equal byte for byte"`; `kind, image = extract_image(...)`; `out.with_suffix("." + kind).write_bytes(image)`; `out.write_bytes(pbm_png.png_from_image(image))`; compare against `image`.

`tools/render.py`: `renderers()` adds `"t5_test_pattern": [str(build_dir / "render_test_pattern"), "--board", "t5"]`; in `main()` the output is `out_dir / f"{name}.{'pgm' if name.startswith('t5_') else 'pbm'}"` and the PNG `pbm_png.png_from_image(path.read_bytes())`; the docstring's output line reads `captures/render/<name>.{pbm,pgm,png}`.

`test/host/render_test_pattern.c` becomes:

```c
#include <stdio.h>
#include <string.h>

#include "gfx.h"
#include "gfx_test_pattern.h"

/* Writes the test pattern: render_test_pattern OUT.pbm (the RLCD's, 400×300 1 bpp), or
 * render_test_pattern --board t5 OUT.pgm (the T5's, 960×540 4 bpp, T5 spec §6.5). */
int main(int argc, char **argv)
{
    bool t5 = argc == 4 && strcmp(argv[1], "--board") == 0 && strcmp(argv[2], "t5") == 0;
    if (argc != 2 && !t5) {
        fprintf(stderr, "usage: %s [--board t5] OUT\n", argv[0]);
        return 2;
    }
    static uint8_t buf[960 * 540 / 2];
    static uint8_t img[960 * 540 + 32];
    gfx_fb_t fb;
    size_t n;
    if (t5) {
        gfx_fb_init_fmt(&fb, buf, 960, 540, GFX_FMT_4BPP);
        gfx_draw_test_pattern_t5(&fb);
        n = gfx_pgm_encode(&fb, img, sizeof(img));
    } else {
        gfx_fb_init(&fb, buf, 400, 300);
        gfx_draw_test_pattern(&fb);
        n = gfx_pbm_encode(&fb, img, sizeof(img));
    }
    const char *path = argv[argc - 1];
    FILE *f = fopen(path, "wb");
    if (f == NULL || fwrite(img, 1, n, f) != n) {
        perror(path);
        return 1;
    }
    fclose(f);
    return 0;
}
```

It calls `gfx_draw_test_pattern_t5()`, which Task 5 adds; until then add to `gfx_test_pattern.h` the declaration `void gfx_draw_test_pattern_t5(gfx_fb_t *fb);` and to `gfx_test_pattern.c` a stub `void gfx_draw_test_pattern_t5(gfx_fb_t *fb) { gfx_draw_test_pattern(fb); }` that Task 5 replaces.

- [ ] **Step 4: Run everything**

Run: `cd ~/reflbo-t5 && cmake --build build-host 2>&1 | grep -E "error|warning" | head; ctest --test-dir build-host -j8 2>&1 | grep "tests passed"; git status --porcelain test/host/golden; python3 tools/render.py >/dev/null && ls captures/render/test_pattern.png captures/render/t5_test_pattern.png; tools/idf.sh build 2>&1 | grep -E "warning:|error:|Project build complete"; REFLBO_BOARD=t5 tools/idf.sh build 2>&1 | grep -E "warning:|error:|Project build complete" | grep -v SPIRAM_MODE_OCT`
Expected: `100% tests passed`; no golden listed; both PNGs exist; two "Project build complete", no warnings.

- [ ] **Step 5: Commit**

```bash
cd ~/reflbo-t5 && git add components/gfx tools test/host/test_gfx.c test/host/test_gfx_bmp_qr.c test/host/render_test_pattern.c
git commit -m "feat(gfx): PGM and 4-bit BMP of a 4 bpp frame; the tools read PGM (T2)"
```

---

### Task 5: The T5's test pattern and its golden

**Files:**
- Create: `components/gfx/gfx_test_pattern_t5.c`, `test/host/golden/t5/test_pattern.pgm` (generated)
- Modify: `components/gfx/gfx_test_pattern.c` (drop Task 4's stub), `components/gfx/include/gfx_test_pattern.h`, `components/gfx/CMakeLists.txt`
- Test: `test/host/test_test_pattern_golden.c`

**Interfaces:**
- Consumes: Task 3's `gfx_font_t5_sans_26`, `gfx_icon_t5_thermometer_40`; Task 4's `gfx_pgm_encode()`.
- Produces: `void gfx_draw_test_pattern_t5(gfx_fb_t *fb);` (a 960×540 4 bpp framebuffer), and the golden `test/host/golden/t5/test_pattern.pgm`.

The T5's extras live in their own file, which only the T5's display, the golden test and the render tool reference: the RLCD's image doesn't link the 4-bit font.

- [ ] **Step 1: The failing golden test**

In `test/host/test_test_pattern_golden.c`, add before `int main(void)`:

```c
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
```

and their `RUN_TEST` lines.

Run: `cd ~/reflbo-t5 && cmake --build build-host >/dev/null 2>&1; ./build-host/test_test_pattern_golden | tail -4`
Expected: `test_t5_test_pattern_matches_the_golden_image` FAILS with "golden image missing"; the 1 bpp test passes (the stub).

- [ ] **Step 2: The pattern**

`gfx_test_pattern.h` gains (Task 4 added the declaration; give it this comment):

```c
/* The T5's (T5 spec §9, `panel test`): the RLCD's pattern, then on a 4 bpp buffer a 16-step gray ramp with
 * its levels, anti-aliased text and an icon in black on white and in white on black. Meant for 960×540; on
 * 1 bpp it is the RLCD's pattern alone. */
void gfx_draw_test_pattern_t5(gfx_fb_t *fb);
```

Remove Task 4's stub from `gfx_test_pattern.c`. Create `components/gfx/gfx_test_pattern_t5.c`:

```c
#include "gfx_test_pattern.h"

#include <stdio.h>

#include "gfx_fonts.h"
#include "gfx_icons_t5.h"

#define RAMP_X    20
#define RAMP_Y    300
#define RAMP_STEP 57
#define RAMP_H    56

void gfx_draw_test_pattern_t5(gfx_fb_t *fb)
{
    gfx_draw_test_pattern(fb);
    if (fb->format != GFX_FMT_4BPP) {
        return;
    }
    for (int i = 0; i < 16; i++) {
        gfx_color_t c = i == 0 ? GFX_BLACK : i == 15 ? GFX_WHITE : GFX_GRAY(i);
        int16_t x = (int16_t)(RAMP_X + i * RAMP_STEP);
        gfx_fill_rect(fb, (gfx_rect_t){ x, RAMP_Y, RAMP_STEP, RAMP_H }, c);
        char label[4];
        snprintf(label, sizeof(label), "%d", i);
        gfx_text_in_rect(fb, &gfx_font_sans_12, (gfx_rect_t){ x, RAMP_Y + RAMP_H + 2, RAMP_STEP, 16 },
                         GFX_ALIGN_CENTER, label, GFX_BLACK);
    }
    gfx_rect(fb, (gfx_rect_t){ RAMP_X - 1, RAMP_Y - 1, 16 * RAMP_STEP + 2, RAMP_H + 2 }, GFX_BLACK);

    gfx_bitmap(fb, 20, 382, &gfx_icon_t5_thermometer_40, GFX_BLACK);
    gfx_text(fb, &gfx_font_t5_sans_26, 70, 412, "Anti-aliased: Žluťoučký kůň úpěl ďábelské ódy", GFX_BLACK);
    gfx_fill_rect(fb, (gfx_rect_t){ 20, 432, 920, 48 }, GFX_BLACK);
    gfx_bitmap(fb, 20, 436, &gfx_icon_t5_thermometer_40, GFX_WHITE);
    gfx_text(fb, &gfx_font_t5_sans_26, 70, 466, "White on black: 0123456789 °C € …", GFX_WHITE);
}
```

`components/gfx/CMakeLists.txt`: add `"gfx_test_pattern_t5.c"` after `"gfx_test_pattern.c"`.

- [ ] **Step 3: Generate the golden and look at it**

Run: `cd ~/reflbo-t5 && cmake -S test/host -B build-host -G Ninja >/dev/null && cmake --build build-host 2>&1 | grep -E "error|warning"; mkdir -p test/host/golden/t5 && build-host/render_test_pattern --board t5 test/host/golden/t5/test_pattern.pgm && python3 tools/render.py >/dev/null && ls -l test/host/golden/t5/test_pattern.pgm captures/render/t5_test_pattern.png`
Expected: a 518 415-byte PGM (`P5\n960 540\n255\n` + 518 400) and its PNG.

Read `captures/render/t5_test_pattern.png` (the Read tool shows images) and check: the RLCD pattern at the top left, TL/TR/BL/BR in the panel's corners, 16 ramp steps from black to white with labels 0–15, the thermometer and the anti-aliased line in black on white, then the black band with the white icon and line; nothing overlaps or runs off the right edge. If something does, move it (the coordinates are this step's to settle), regenerate, and look again.

- [ ] **Step 4: Run everything**

Run: `cd ~/reflbo-t5 && ./build-host/test_test_pattern_golden | tail -2; ctest --test-dir build-host -j8 2>&1 | grep "tests passed"; git status --porcelain test/host/golden; tools/idf.sh build 2>&1 | grep -E "warning:|error:|Project build complete"; REFLBO_BOARD=t5 tools/idf.sh build 2>&1 | grep -E "warning:|error:|Project build complete" | grep -v SPIRAM_MODE_OCT; tools/idf.sh exec xtensa-esp32s3-elf-nm build/reflbo.elf | grep -c "gfx_font_t5_sans_26\|gfx_draw_test_pattern_t5"`
Expected: `0 Failures`; `100% tests passed`; only `?? test/host/golden/t5/` listed; two "Project build complete", no warnings; the RLCD's ELF has `0` of the T5's symbols.

- [ ] **Step 5: Commit**

```bash
cd ~/reflbo-t5 && git add components/gfx test/host/test_test_pattern_golden.c test/host/golden/t5/test_pattern.pgm
git commit -m "feat(gfx): the T5's test pattern with a gray ramp and anti-aliased text, and its golden (T2)"
```

---

### Task 6: The T5's 4 bpp panel frame, screenshots and the web's BMP

**Files:**
- Modify: `components/display/include/display.h`, `components/display/display_rlcd42.c`, `components/display/display_t547.c`, `components/display/include/display_board.h`
- Modify: `components/diag/diag_cmd_display.c`, `main/app_web.c`, `components/webui/include/webui.h`
- Modify: `AGENTS.md`

**Interfaces:**
- Consumes: Tasks 1, 4 and 5.
- Produces:
  - `const gfx_fb_t *display_screenshot_fb(void);` (both boards: the RLCD's `display_fb()`, the T5's panel frame)
  - T547: `esp_err_t display_t5_test_pattern(void);`
  - `WEBUI_REPLY_MAX` 264 KB on the T5

- [ ] **Step 1: The display API**

`display.h`, after `display_fb()`'s declaration:

```c
/* The image `screenshot` and the web's screenshot show: the frame as the panel last got it. The RLCD's is
 * display_fb(); the T5's is its 960×540 4 bpp panel frame (T5 spec §6.5). NULL before display_init. */
const gfx_fb_t *display_screenshot_fb(void);
```

`display_rlcd42.c`, after `display_fb()`:

```c
const gfx_fb_t *display_screenshot_fb(void)
{
    return display_fb();
}
```

`display_board.h`, T547 branch, after `display_t5_bench()`:

```c
/* `panel test` (T5 spec §9): gfx_draw_test_pattern_t5() on the panel frame, clean. The next commit puts the
 * dashboard back. */
esp_err_t display_t5_test_pattern(void);
```

- [ ] **Step 2: The T5's panel frame**

In `components/display/display_t547.c`:

1. The file comment's second sentence reads: `The UI still draws the RLCD's 400×300 at 1 bpp until T3; each commit composes it into the middle of the 960×540 4 bpp panel frame (T5 spec §6), which goes to epdiy as it is.`
2. After `static gfx_fb_t s_fb;` add `static gfx_fb_t s_panel; /* the 4 bpp frame the panel gets: epdiy's layout */`.
3. `alloc_fb()` becomes:

```c
static esp_err_t alloc_fb(void)
{
    ESP_RETURN_ON_FALSE(s_fb.buf == NULL, ESP_ERR_INVALID_STATE, TAG, "already initialised");
    uint8_t *buf = heap_caps_calloc(1, gfx_fb_size(FB_W, FB_H), MALLOC_CAP_SPIRAM);
    uint8_t *panel = heap_caps_malloc(gfx_fb_size_fmt(GFX_FMT_4BPP, PANEL_W, PANEL_H), MALLOC_CAP_SPIRAM);
    if (buf == NULL || panel == NULL) {
        heap_caps_free(buf);
        heap_caps_free(panel);
        ESP_LOGE(TAG, "framebuffers");
        return ESP_ERR_NO_MEM;
    }
    gfx_fb_init(&s_fb, buf, FB_W, FB_H);
    gfx_fb_init_fmt(&s_panel, panel, PANEL_W, PANEL_H, GFX_FMT_4BPP);
    gfx_clear(&s_panel, GFX_WHITE); /* after a deep-sleep wake too, until the next commit (T4: the frame kept) */
    return ESP_OK;
}
```

4. `clean_update(const gfx_fb_t *fb)` takes the panel frame: replace its `epaper_frame_blit_1bpp(fb, epd_hl_get_framebuffer(&s_hl), PANEL_W, PANEL_H, FB_X, FB_Y);` with `memcpy(epd_hl_get_framebuffer(&s_hl), fb->buf, PANEL_W / 2 * PANEL_H); /* the same layout as epdiy's */`.
5. `display_commit()`'s `ESP_RETURN_ON_ERROR(clean_update(&s_fb), TAG, "update");` becomes:

```c
    epaper_frame_blit_1bpp(&s_fb, s_panel.buf, PANEL_W, PANEL_H, FB_X, FB_Y); /* T1-T2: the UI in the middle */
    ESP_RETURN_ON_ERROR(clean_update(&s_panel), TAG, "update");
```

6. Add after `display_fb()`:

```c
const gfx_fb_t *display_screenshot_fb(void)
{
    return s_panel.buf != NULL ? &s_panel : NULL;
}

esp_err_t display_t5_test_pattern(void)
{
    ESP_RETURN_ON_FALSE(s_panel.buf != NULL, ESP_ERR_INVALID_STATE, TAG, "not initialised");
    gfx_draw_test_pattern_t5(&s_panel);
    s_pushed = false; /* the next commit puts the dashboard back */
    return clean_update(&s_panel);
}
```

7. `display_t5_bench()` draws the black-and-white pattern on the panel frame and restores the whole frame for DU (T1 review minor 6: the inverted border stayed):

```c
esp_err_t display_t5_bench(display_t5_bench_t *out)
{
    ESP_RETURN_ON_FALSE(s_panel.buf != NULL, ESP_ERR_INVALID_STATE, TAG, "not initialised");
    gfx_draw_test_pattern(&s_panel); /* black and white only, so DU draws it as it is */
    const size_t size = PANEL_W / 2 * PANEL_H;

    ESP_RETURN_ON_ERROR(panel_up(), TAG, "panel up");
    uint8_t *front = epd_hl_get_framebuffer(&s_hl);
    int64_t t0 = now_ms();
    clear_to_white();
    memcpy(front, s_panel.buf, size);
    int err = (int)epd_hl_update_screen(&s_hl, MODE_GC16, TEMPERATURE_C);
    int64_t t1 = now_ms();
    for (size_t i = 0; i < size; i++) {
        front[i] = (uint8_t)~front[i];
    }
    err |= (int)epd_hl_update_screen(&s_hl, MODE_GL16, TEMPERATURE_C);
    int64_t t2 = now_ms();
    memcpy(front, s_panel.buf, size); /* the whole frame back, border included */
    err |= (int)epd_hl_update_screen(&s_hl, MODE_DU, TEMPERATURE_C);
    int64_t t3 = now_ms();
    panel_down();

    s_pushed = false; /* the panel shows the pattern: the next commit puts the frame back */
    *out = (display_t5_bench_t){ (uint32_t)(t1 - t0), (uint32_t)(t2 - t1), (uint32_t)(t3 - t2) };
    ESP_LOGI(TAG, "bench: clean %lu ms, GL16 %lu ms, DU %lu ms", (unsigned long)out->clean_ms,
             (unsigned long)out->gl16_ms, (unsigned long)out->du_ms);
    ESP_RETURN_ON_FALSE(err == EPD_DRAW_SUCCESS, ESP_FAIL, TAG, "epdiy draw error 0x%x", (unsigned)err);
    return ESP_OK;
}
```

- [ ] **Step 3: The console and the web**

`components/diag/diag_cmd_display.c`:
- `screenshot_body()` becomes:

```c
static int screenshot_body(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    const gfx_fb_t *fb = display_screenshot_fb();
    if (fb == NULL) {
        printf("screenshot: display not initialised\n");
        return 1;
    }
    bool gray = fb->format == GFX_FMT_4BPP; /* T5 spec §6.5: PGM from 4 bpp, PBM from 1 bpp */
    size_t size = gray ? gfx_pgm_size(fb) : gfx_pbm_size(fb);
    uint8_t *img = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (img == NULL) {
        printf("screenshot: out of memory\n");
        return 1;
    }
    size_t len = gray ? gfx_pgm_encode(fb, img, size) : gfx_pbm_encode(fb, img, size);
    char line[80];
    printf("-----BEGIN RLCD %s-----\n", gray ? "PGM" : "PBM");
    for (size_t off = 0; off < len; off += PBM_CHUNK) {
        size_t chunk = len - off < PBM_CHUNK ? len - off : PBM_CHUNK;
        util_base64_encode(img + off, chunk, line, sizeof(line));
        printf("%s\n", line);
    }
    printf("-----END RLCD %s-----\n", gray ? "PGM" : "PBM");
    free(img);
    return 0;
}
```

- In `panel_body()`, the `test` branch becomes:

```c
    } else if (argc == 2 && strcmp(argv[1], "test") == 0) {
#if BOARD_HAS_LPM_RATE
        gfx_draw_test_pattern(fb);
        err = display_commit(false);
#else
        err = display_t5_test_pattern(); /* T5 spec §9: the pattern with the gray ramp, on the whole panel */
#endif
```

- The `screenshot` command's help: `"Print the screen as base64 PBM (PGM on a 4 bpp panel) between markers"`.

`main/app_web.c`: `reply_bmp(display_fb(), out, size, reply);` for `/api/screenshot.bmp` becomes `reply_bmp(display_screenshot_fb(), out, size, reply);`.

`components/webui/include/webui.h`: add `#include "sdkconfig.h"` beside `esp_err.h`; `WEBUI_REPLY_MAX` becomes:

```c
#if CONFIG_REFLBO_BOARD_T547
#define WEBUI_REPLY_MAX (264 * 1024) /* the largest reply: the T5's screenshot, a 4-bit BMP of 960×540 (259 318 bytes) */
#else
#define WEBUI_REPLY_MAX (64 * 1024) /* the largest reply: a backup, or a BMP (15 662 bytes) */
#endif
```

- [ ] **Step 4: Builds, symbols, host tests**

Run:

```bash
cd ~/reflbo-t5 && tools/idf.sh build 2>&1 | grep -E "warning:|error:|Project build complete"
REFLBO_BOARD=t5 tools/idf.sh build 2>&1 | grep -E "warning:|error:|Project build complete" | grep -v SPIRAM_MODE_OCT
tools/idf.sh exec xtensa-esp32s3-elf-nm build-t5/reflbo.elf > build-t5/nm.txt; tools/idf.sh exec xtensa-esp32s3-elf-nm build/reflbo.elf > build/nm.txt
for s in st7305_ shtc3_ pcf85063_ write_codec; do printf 't5 %s %s\n' "$s" "$(grep -c "$s" build-t5/nm.txt)"; done
for s in gfx_draw_test_pattern_t5 gfx_font_t5_sans_26 gfx_pgm_encode display_t5_test_pattern; do printf 't5 has %s %s\n' "$s" "$(grep -c "$s" build-t5/nm.txt)"; done
for s in epd_ epaper_ gfx_draw_test_pattern_t5 gfx_font_t5_sans_26; do printf 'rlcd %s %s\n' "$s" "$(grep -c "$s" build/nm.txt)"; done
cmake --build build-host >/dev/null && ctest --test-dir build-host -j8 2>&1 | grep "tests passed"; git status --porcelain test/host/golden
```

Expected: two "Project build complete", no warnings; the `t5` RLCD-driver counts `0`, every `t5 has` ≥ `1`, every `rlcd` count `0`; `100% tests passed`; no golden listed.

- [ ] **Step 5: AGENTS.md and commit**

In AGENTS.md §10, append to the **Seams** bullet: `The T5's display keeps a 960×540 4 bpp panel frame (epdiy's layout): each commit composes the UI's 400×300 1 bpp frame into its middle until T3, and \`panel test\` draws \`gfx_draw_test_pattern_t5()\` (a 16-step ramp, anti-aliased text and an icon) on the whole of it. \`display_screenshot_fb()\` is what \`screenshot\` and \`/api/screenshot.bmp\` show: on the T5 the panel frame, as a PGM (console) or a 4-bit BMP (web).` and to **The T5 at the bench**: `A screenshot is a 960×540 PGM, about 690 KB of base64 at 115200 baud, about a minute: \`tools/idf.sh exec python tools/screenshot.py -p <port> -t 120 -o captures/t5/screen.png\`.`

```bash
cd ~/reflbo-t5 && git add components/display components/diag/diag_cmd_display.c main/app_web.c components/webui/include/webui.h AGENTS.md
git commit -m "feat(display): the T5 pushes a 4 bpp panel frame; PGM and gray BMP screenshots of it (T2)"
```

---

### Task 7: At the board

Every **Ask the owner** stops the run until they answer. The T5 is `/dev/cu.usbserial-52D60046741`; set `T5=` to it. Its idle is deep (from T1's measurements); console lines hold it awake 5 minutes, which covers these steps.

**Files:**
- Possibly modify, with rulings: `components/gfx/gfx_test_pattern_t5.c` (layout), `components/display/display_t547.c`
- Create (gitignored): `captures/t5/`

- [ ] **Step 1: The board is the one; flash**

Run: `cd ~/reflbo-t5 && T5=/dev/cu.usbserial-52D60046741 && tools/idf.sh exec python -m esptool -p $T5 -b 230400 read_mac 2>&1 | grep -E "Chip is|MAC:" | head -2 && REFLBO_BOARD=t5 tools/idf.sh -p $T5 -b 230400 flash 2>&1 | tail -1 && tools/idf.sh exec python tools/devlog.py -p $T5 --reset --cmd version -t 30 -o captures/t5/t2-boot.log >/dev/null; grep -E "display: clean|reflbo ready|E \(|elf" captures/t5/t2-boot.log`
Expected: `ESP32-D0WD-V3`, `34:ab:95:5e:5d:58`; the flash `Done`/`Leaving...`; two clean updates, `reflbo ready`, the new ELF hash, no `E (` lines.

The owner allows flashing the T5 (T1, 2026-10-07/09); nothing here erases.

- [ ] **Step 2: The test pattern and its screenshot**

Run:

```bash
cd ~/reflbo-t5 && T5=/dev/cu.usbserial-52D60046741
tools/idf.sh exec python tools/devlog.py -p $T5 --cmd "panel test" --cmd "panel status" -t 30 | grep -E "panel|display:"
tools/idf.sh exec python tools/screenshot.py -p $T5 -t 150 -o captures/t5/t2-test-pattern.png --compare test/host/golden/t5/test_pattern.pgm; echo rc=$?
```

Expected: a clean update; `screenshot: identical to test/host/golden/t5/test_pattern.pgm`, `rc=0`. If a minute's redraw came between the two calls, the screenshot shows the dashboard instead (rc 5): run both again right after a minute turns.

- [ ] **Step 3: Ask the owner**

Ask: "The panel shows the T5 test pattern now. Please photograph it and paste the photo. Expected: 16 ramp steps from black to white, each visibly different; the line 'Anti-aliased: Žluťoučký kůň…' and the thermometer with smooth edges; the same in white on the black band. Any banding, two steps that look the same, or rough edges?"

Proceed on the answer. If steps merge or the ramp isn't monotonic, that's the waveform's levels on this panel: ledger it for T3's gray choices (spec §6.4), don't change the ramp.

- [ ] **Step 4: The dashboard as a gray frame**

Wait for the next minute (the dashboard comes back), then run:

```bash
cd ~/reflbo-t5 && T5=/dev/cu.usbserial-52D60046741
tools/idf.sh exec python tools/screenshot.py -p $T5 -t 150 -o captures/t5/t2-dashboard.png; echo rc=$?
```

Expected: `rc=0`. Read `captures/t5/t2-dashboard.png`: the 400×300 dashboard in the middle of a white 960×540 frame, as on the panel.

- [ ] **Step 5: The bench**

Run: `cd ~/reflbo-t5 && tools/idf.sh exec python tools/devlog.py -p /dev/cu.usbserial-52D60046741 --cmd "panel bench" --cmd "panel status" -t 60 | grep -E "panel"`
Expected: one `panel bench: clean … GL16 … DU …` line, within ~5 % of T1's (2.22 s, 1.10 s, 0.55 s). Ledger it. The panel shows the pattern afterwards with no black border (T1 minor 6).

- [ ] **Step 6: Commit any changes**

If Steps 1–5 changed code, run the host tests and both builds (Task 6 Step 4), then commit with `fix: board findings on the T5's gray frame (T2)`, a ruling a line in its body. If nothing changed, there's no commit.

---

### Task 8: Record T2

**Files:**
- Modify: `docs/specs/2026-10-06-t5-board-design.md` (r5), `AGENTS.md`, `components/rtc/include/rtcchip.h`, `tools/idf.sh`

- [ ] **Step 1: The spec, r5**

In the T5 spec:
- **Status** line: `r4 records T1 as built; r5 records T2 (§13)`.
- §6.1: add "`gfx_fb_init_fmt()` and `gfx_fb_size_fmt()` take the format; `gfx_fb_init()`/`gfx_fb_size()` stay 1 bpp, and a zeroed `format` is 1 bpp."
- §6.2: add "`gfx_pixel_coverage()` blends: towards the ink by coverage / 15, rounded half away from zero; 1 bpp inks from coverage 8."
- §6.3: add "A font's and a bitmap's `bpp` is their last member, so an initializer without it reads as 1; 4-bit rows put the first pixel in the high nibble. T2 generated `t5_sans_26` and `t5_thermometer_40` (`assets/icons/icons_t5.txt`) for the test pattern."
- §6.5: add "The T5 keeps a 960×540 4 bpp panel frame; `display_screenshot_fb()` returns it. A T5 screenshot is about 690 KB of base64, about a minute at 115200 baud; `/api/screenshot.bmp`'s reply buffer is 264 KB on the T5."
- §9 `panel test`: "the T5's pattern: the RLCD's, a 16-step ramp, anti-aliased text and an icon (`gfx_draw_test_pattern_t5()`)."
- §11 T2 row's "Done when" gains `(done <date>)`.
- §13: an r5 row: `T2 as built: the formats and blending (§6.1–6.2), the asset formats (§6.3), the panel frame and screenshots (§6.5), panel test (§9)`; and the board's ramp finding from Task 7 Step 3 if there was one.
- §4.1: "The ESP32 build sets `CONFIG_ESP32_REV_MIN_3`: the WROVER-E's chip is revision 3, and without the PSRAM cache workaround IRAM fits (about 12 KB free with epdiy). An M7 merge that runs short can turn off `CONFIG_ESP_WIFI_IRAM_OPT`, `CONFIG_ESP_WIFI_RX_IRAM_OPT` or `CONFIG_LWIP_IRAM_OPTIMIZATION`." (T1 minor 5)

- [ ] **Step 2: AGENTS.md, the stale texts**

- AGENTS.md §10 **Status**: `T2 is done (<date>): gfx draws 4 bpp grays with anti-aliased fonts and icons; the T5 pushes a 960×540 gray panel frame, \`panel test\` shows the ramp, and screenshots are PGM (console) and 4-bit BMP (web). Next: T3, the dense UI.` and the plan link becomes `docs/plans/2026-10-09-t2-grayscale-gfx.md`.
- AGENTS.md §6's command list gains `tools/gen_fonts.sh` and `tools/gen_icons.sh` notes: "(also the T5's 4-bit fonts and \`assets/icons/icons_t5.txt\`)"; and `python3 tools/render.py` "(also \`t5_test_pattern.pgm\`)".
- `components/rtc/include/rtcchip.h`: "the PCF8563 on the T5" becomes "the ESP32's system clock on the T5 (`rtcchip_t547.c`, DT10)" (T1 minor 5).
- `tools/idf.sh`: its "found:" port hint lists `/dev/cu.usbmodem*` and `/dev/cu.usbserial-*` (T1 minor 5). Read the hint's lines first and change only the glob it prints.

- [ ] **Step 3: Checks, tag, push**

Run:

```bash
cd ~/reflbo-t5 && cd tools && python3 -m unittest tests.test_idf_sh 2>&1 | tail -1 && cd .. \
  && tools/idf.sh build 2>&1 | grep -E "warning:|Project build complete" && REFLBO_BOARD=t5 tools/idf.sh build 2>&1 | grep -E "warning:|Project build complete" | grep -v SPIRAM_MODE_OCT \
  && cmake --build build-host >/dev/null && ctest --test-dir build-host -j8 2>&1 | grep "tests passed" && git status --porcelain test/host/golden
git add docs/specs/2026-10-06-t5-board-design.md AGENTS.md components/rtc/include/rtcchip.h tools/idf.sh && git commit -m "docs: T2 as built (T5 spec r5)"
git tag t5-stable-t2 && git push origin main && git push origin t5-stable-t2
```

Expected: `OK`; two "Project build complete"; `100% tests passed`; no golden listed; `main` and `t5-stable-t2` on `origin`.
