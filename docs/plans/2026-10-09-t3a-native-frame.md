# T3a: The Native Frame Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The T5 draws its dashboard natively at 960×540 in 4 bpp gray, with its own fonts, icons, fixed layouts, menu, screens and presets, all read from the board's UI profile, while the RLCD draws exactly as before.

**Architecture:** The UI profile (`ui_profile_t`) grows from size and capabilities into the board's whole UI geometry: a font table by role, an icon table by size class, a pixel scale (`UI_PX()`), the fixed layouts with their separators, the menu's rows and the split layout's limits. Each board's instance is its own source file, compiled only into that board's image. The UI's code stops naming fonts, icons and pixel paddings directly; it asks the profile. On the RLCD every answer is today's literal, so its goldens stay byte-identical; on the T5 they come from the 4-bit asset set this plan generates. The T5's display hands the UI its 960×540 4 bpp panel frame directly, and the host's golden harness learns `--board t5` with gzip-compressed PGM goldens.

**Tech Stack:** C17 (ESP-IDF 5.5.5, the host's Unity tests, zlib on the host), Python 3 (the generators through `uv` and Pillow 12.3.0), epdiy 2.1.3.

**Spec:** [`docs/specs/2026-10-06-t5-board-design.md`](../specs/2026-10-06-t5-board-design.md) r5: §6.3–6.5, §7.1–7.3, §9 (menu), §10, §11 T3; DT2 (the owner approves the T5's renders before their goldens are committed), DT3.

## Global Constraints

- "The RLCD's profile holds today's numbers exactly." (§7.1) Every task keeps `test/host/golden/*.pbm` byte-identical: `git status --porcelain test/host/golden` lists nothing outside `t5/`.
- "Fixed paddings inside widgets become `UI_PX(n)`: the identity on the RLCD, × 1.7 rounded on the T5." (§7.1)
- "About 1.7× the RLCD's pixels (DT2): fonts 12 → 20, 16 → 26, 20 → 34, 28 → 46, numbers 48 → 80, 72 → 120, 110 → 180, 130 → 220; icons 16 → 26, 24 → 40, 48 → 80; the status bar 20 → 34." (§7.2)
- Layouts per §7.3: Classic "main, date row, 6 small"; Weather "now, today, hourly, 3 small"; Grid "4×2"; Focus "main, 3 small". "The menu shows 8 rows. The config, first-run, critical-battery and QR screens keep their content, rescaled. The T5's built-in presets leave out the `env.*` fields."
- DT2: "The owner approves the T5 layouts from host renders before goldens are committed." Task 9 stops for that.
- On the T5, levels 9–15 look alike (spec §6.4): any gray T3a draws takes a level from 0–8.
- `[host]` components include no ESP-IDF headers (AGENTS.md §5.3). Component sources are listed, never globbed; a source may depend on the board, REQUIRES may not (AGENTS.md §8, §10).
- The T5's 4 MB app slot: after Task 1 and Task 8, `REFLBO_BOARD=t5 tools/idf.sh size` shows the image under 3.6 MB.
- The fork never flashes the RLCD (DT8). The T5: `/dev/cu.usbserial-52D60046741`, MAC `34:ab:95:5e:5d:58`, every esptool and flash call at `-b 230400`; flashing it needs no new consent (owner, T1), erasing does.
- No AI or assistant attribution in commits, code or docs.

## Review Focus

1. **The RLCD drawing differently after the mechanical passes.** Expectation: fonts, icons and `UI_PX()` resolve to today's values on the RLCD, so every RLCD golden stays byte-identical. Pinned by the 100 RLCD goldens after each of Tasks 4–8, and by Task 2's tests that the RLCD profile's tables equal today's literals.
2. **A board's assets leaking into the other's image.** Expectation: the RLCD image links no `gfx_font_t5_*`/`gfx_icon_t5_*`, the T5's no RLCD font the UI no longer names. Task 1's and Task 8's symbol checks.
3. **A T5 slot or separator outside the panel or under the status bar.** Expectation: every T5 slot and separator lies in 0..959 × 35..539, slots don't overlap, and each slot is at least its size class's minimum. Task 7's geometry test.
4. **Icons of 4 bits read as 1 bit.** Expectation: `bitmap_ink_box()`, `bolt_ink()` and the test helpers read a 4-bit icon's coverage (ink from 8), so the age mark and the bolt sit where their ink is on the T5. Task 5's test on a 4-bit bitmap.
5. **The T5's frame and the profile disagreeing.** Expectation: the display's frame is the profile's size and format on both boards; a mismatch is caught at boot, not drawn. Task 8's boot check and its test of `ui_profile_matches()`.

## Not in T3a

- The views at 960×540: Radar and Flights (`RADAR_VIEW_W/H`, `UI_FLIGHTS_MAP_H`, RainViewer's tile count, the flights filter), Solar and Energy (absolute layouts), the map's fonts and marks, and the gray uses of spec §6.4: **T3b**. Until then those four layouts draw on the T5 with the RLCD's view geometry, and the T5's default presets keep them out of the cycle.
- The web page (`/api/layouts`'s panel size and aspect, `style.css`, `app.js`, the gray preview) and the fetch of the T5's gray BMPs (T2 review carry-over): **T3c**. Until then `/api/preview.bmp` renders 1 bpp at the profile's size.
- Fast updates, the clean counter, the previous frame across deep sleep: T4.

## Carried in

- T2 review: the `display_t547.c` tidy (Task 8: compose and commit only after `panel_up()` succeeds, `s_pushed` false before the bench's `panel_up()`, `clean_update()` without a size-mismatched parameter); imggen's 4-bit `render_fitted()` gets its first assets (Task 1: the Weather Icons at 4 bits).

## The T5's numbers (used by Tasks 2, 7)

`UI_PX(n)` on the T5 is `round(n × 17 / 10)`, halves away from zero. Status bar 34; the area below it is `{0, 35, 960, 505}`.

| Layout | Slot | Rect `{x, y, w, h}` | Size | Separators (`x, y, len`, h or v) |
|---|---|---|---|---|
| Classic | main | 0, 35, 960, 212 | XL | h 20, 316, 920; v 160·i, 335, 186 for i = 1..5 |
| | sub | 0, 247, 960, 68 | M (DATE, TEXT) | |
| | s1..s6 | 160·(i−1), 318, 160, 222 | S | |
| Weather | now | 0, 35, 480, 288 | L | v 480, 49, 260; h 494, 179, 452; h 20, 323, 920; v 320, 340, 186; v 640, 340, 186 |
| | today | 480, 35, 480, 144 | M | |
| | hourly | 480, 179, 480, 144 | M | |
| | s1..s3 | 320·(i−1), 324, 320, 216 | S | |
| Grid | g1..g4 | 240·(i−1), 35, 240, 252 | M | v 240, 49, 477; v 480, 49, 477; v 720, 49, 477; h 14, 287, 932 |
| | g5..g8 | 240·(i−5), 287, 240, 253 | M | |
| Focus | main | 0, 35, 960, 344 | XL | h 20, 379, 920; v 320, 395, 130; v 640, 395, 130 |
| | s1..s3 | 320·(i−1), 380, 320, 160 | M | |

Menu: header 51, first row at 61, rows of 55, 8 rows, footer at `height − UI_PX(18)` (509). Split: cells ≥ 68×34 (§7.3), narrow below 255, inset 14, size minima `UI_PX()` of the RLCD's (`ui_split.c` `k_sizes`), XL from 680 wide.

T5 default presets (no `env.*`):

| Preset | Layout | Slots in order |
|---|---|---|
| home | Classic | TIME_CLOCK, DATE_DAY, WX_NOW, WX_TODAY, SUN_TIMES, MOON_PHASE, AQ_INDEX, BAT_LEVEL |
| sky (replaces indoor) | Grid | WX_NOW, WX_TODAY, AQ_INDEX, AQ_UV, POLLEN_TOP, SUN_TIMES, MOON_PHASE, BAT_DAYS |
| weather | Weather | WX_NOW, WX_TODAY, WX_HOURLY, AQ_INDEX, POLLEN_TOP, SUN_TIMES |
| focus | Focus | TIME_CLOCK, DATE_DAY, WX_NOW, MOON_PHASE, BAT_LEVEL |

These are the renders' starting points; Task 9 settles them with the owner.

---

### Task 1: The T5's asset set

**Files:**
- Modify: `tools/gen_fonts.sh`, `assets/icons/icons_t5.txt`, `components/gfx/include/gfx_fonts.h`, `components/gfx/CMakeLists.txt`
- Create (generated): `components/gfx/fonts/gfx_font_t5_{sans_20,sans_34,bold_26,bold_34,bold_46,num_80,num_120,num_180,num_220}.c`; regenerated `components/gfx/icons/gfx_icons_t5.c`, `components/gfx/include/gfx_icons_t5.h`
- Test: `test/host/test_gfx_fonts.c`

**Interfaces:**
- Produces: `gfx_font_t5_sans_20`, `_sans_26`, `_sans_34`, `_bold_26`, `_bold_34`, `_bold_46`, `_num_80`, `_num_120`, `_num_180`, `_num_220` (4-bit); `gfx_icon_t5_<name>_{26,40,80}` for each of the 45 names in `assets/icons/icons.txt` (4-bit).

- [ ] **Step 1: The failing test.** In `test/host/test_gfx_fonts.c` add:

```c
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
```

(include `gfx_icons_t5.h`; add both `RUN_TEST`s). Run: `cmake --build build-host 2>&1 | grep -m2 error` — Expected: undeclared `gfx_font_t5_sans_20` (and the icons).

- [ ] **Step 2: Generate.** Append to `tools/gen_fonts.sh` (after the `t5_sans_26` line):

```bash
fontgen --ttf assets/fonts/DejaVuSans.ttf --size 20 --charset text --name t5_sans_20 --bpp 4 --licence assets/fonts/LICENSE-DejaVu.txt
fontgen --ttf assets/fonts/DejaVuSans.ttf --size 34 --charset text --name t5_sans_34 --bpp 4 --licence assets/fonts/LICENSE-DejaVu.txt
fontgen --ttf assets/fonts/DejaVuSans-Bold.ttf --size 26 --charset text --name t5_bold_26 --bpp 4 --licence assets/fonts/LICENSE-DejaVu.txt
fontgen --ttf assets/fonts/DejaVuSans-Bold.ttf --size 34 --charset text --name t5_bold_34 --bpp 4 --licence assets/fonts/LICENSE-DejaVu.txt
fontgen --ttf assets/fonts/DejaVuSans-Bold.ttf --size 46 --charset text --name t5_bold_46 --bpp 4 --licence assets/fonts/LICENSE-DejaVu.txt
fontgen --ttf assets/fonts/DejaVuSansCondensed-Bold.ttf --size 80 --charset digits --name t5_num_80 --bpp 4 --licence assets/fonts/LICENSE-DejaVu.txt
fontgen --ttf assets/fonts/DejaVuSansCondensed-Bold.ttf --size 120 --charset digits --name t5_num_120 --bpp 4 --licence assets/fonts/LICENSE-DejaVu.txt
fontgen --ttf assets/fonts/DejaVuSansCondensed-Bold.ttf --size 180 --charset digits --name t5_num_180 --bpp 4 --licence assets/fonts/LICENSE-DejaVu.txt
fontgen --ttf assets/fonts/DejaVuSansCondensed-Bold.ttf --size 220 --charset digits --name t5_num_220 --bpp 4 --licence assets/fonts/LICENSE-DejaVu.txt
```

Replace `assets/icons/icons_t5.txt`'s body (keep its header comment, changing "T2 brings the test pattern's; T3 adds the T5's set at its sizes (§7.2)" to "the T5's set at its sizes (T5 spec §7.2), one line for each of icons.txt's names") with one line per name of `assets/icons/icons.txt`, the C name prefixed `t5_`, its source unchanged, sizes `26 40 80`, in the same order (45 lines; e.g. `t5_thermometer    device_thermostat     26 40 80`, `t5_wx_rain    wi:rain    26 40 80`).

Run `PATH="$HOME/.local/bin:$PATH" tools/gen_fonts.sh && PATH="$HOME/.local/bin:$PATH" tools/gen_icons.sh`, then `git status --short components/gfx`. Expected: the RLCD fonts and `gfx_icons.c/.h` unchanged; 9 new font files; `gfx_icons_t5.c/.h` regenerated with 135 icons.

Look at the Weather Icons at 4 bits (their first 4-bit render through `render_fitted()`, T2 review): write a scratch host program, or a Python snippet over the generated C arrays, that draws all 135 T5 icons into a PNG grid; read the PNG. Expected: every glyph centred in its square, nothing clipped, edges anti-aliased.

- [ ] **Step 3: Wire, per board.** `gfx_fonts.h`: extern the nine new fonts under the existing T5 comment. `components/gfx/CMakeLists.txt`: move `"fonts/gfx_font_t5_sans_26.c"`, `"icons/gfx_icons_t5.c"` and `"gfx_test_pattern_t5.c"` out of the shared list into a T5-only one, with the nine new fonts:

```cmake
set(srcs "gfx.c" "gfx_bmp.c" ... the shared list as it is, without the T5 files ...)
if(CONFIG_REFLBO_BOARD_T547) # the T5's 4-bit assets: only its image carries them (T5 spec §4.1)
    list(APPEND srcs "gfx_test_pattern_t5.c" "icons/gfx_icons_t5.c"
                     "fonts/gfx_font_t5_sans_20.c" "fonts/gfx_font_t5_sans_26.c" "fonts/gfx_font_t5_sans_34.c"
                     "fonts/gfx_font_t5_bold_26.c" "fonts/gfx_font_t5_bold_34.c" "fonts/gfx_font_t5_bold_46.c"
                     "fonts/gfx_font_t5_num_80.c" "fonts/gfx_font_t5_num_120.c" "fonts/gfx_font_t5_num_180.c"
                     "fonts/gfx_font_t5_num_220.c")
endif()
idf_component_register(SRCS ${srcs} INCLUDE_DIRS "include" PRIV_INCLUDE_DIRS "qrcodegen")
```

- [ ] **Step 4: Run.** `cmake -S test/host -B build-host -G Ninja >/dev/null && cmake --build build-host 2>&1 | grep -E "error|warning:" | grep -v duplicate; ctest --test-dir build-host -j8 2>&1 | grep "tests passed"; git status --porcelain test/host/golden`; both firmware builds; `tools/idf.sh exec xtensa-esp32s3-elf-nm build/reflbo.elf | grep -c "gfx_font_t5_\|gfx_icon_t5_"`. Expected: `100% tests passed`, no golden listed, two "Project build complete" without warnings, `0` for the RLCD.

- [ ] **Step 5: Commit** `feat(gfx): the T5's 4-bit fonts and icons at its sizes, built into its image only (T3a)`.

---

### Task 2: The profile holds the board's UI

**Files:**
- Create: `components/ui/include/ui_icons.h`, `components/ui/ui_profile_rlcd42.c`, `components/ui/ui_profile_t547.c`
- Modify: `components/ui/include/ui_profile.h`, `components/ui/ui_profile.c`, `components/ui/include/ui_layout.h`, `components/ui/ui_layout.c`, `components/ui/CMakeLists.txt`, `test/host/CMakeLists.txt`
- Test: `test/host/test_ui_profile.c`

**Interfaces:**
- Produces (exact):

```c
/* ui_profile.h */
typedef enum { UI_F_SANS_12, UI_F_SANS_16, UI_F_SANS_20, UI_F_BOLD_16, UI_F_BOLD_20, UI_F_BOLD_28,
               UI_F_NUM_48, UI_F_NUM_72, UI_F_NUM_110, UI_F_NUM_130, UI_F_COUNT } ui_font_id_t;
typedef enum { UI_IC16, UI_IC24, UI_IC48, UI_IC_CLASSES } ui_icon_class_t;
typedef struct { int16_t header_h, row_y0, row_h, rows; } ui_menu_geometry_t;
typedef struct { int16_t min_w, min_h, narrow_w, inset; } ui_split_limits_t;
struct ui_layout; /* ui_layout.h */
typedef struct {
    const char *board;
    int16_t width, height;
    int16_t status_h;
    uint32_t caps;
    uint8_t format;         /* gfx_format_t: the panel's frame */
    uint8_t px_num, px_den; /* UI_PX(): n × num / den rounded; 1/1 on the RLCD, 17/10 on the T5 */
    const gfx_font_t *const *fonts;                         /* [UI_F_COUNT] */
    const gfx_bitmap_t *const (*icons)[UI_IC_CLASSES];      /* [UI_ICON_COUNT] */
    int16_t icon_px[UI_IC_CLASSES];                         /* 16 24 48 / 26 40 80 */
    const struct ui_layout *layouts;                        /* [UI_LAYOUT_COUNT] */
    ui_menu_geometry_t menu;
    ui_split_limits_t split;
} ui_profile_t;
int ui_px(int n);
#define UI_PX(n) ui_px(n)
#define UI_FONT(id) (ui_profile()->fonts[(id)])
const gfx_bitmap_t *ui_icon(ui_icon_id_t id, ui_icon_class_t cls);
ui_icon_class_t ui_icon_class(int rlcd_px); /* ≥ 48: UI_IC48; ≥ 24: UI_IC24; else UI_IC16 */
int ui_icon_px(int rlcd_px);                /* the profile's pixels for that class */
/* ui_icons.h */
#define UI_ICON_LIST(X) X(thermometer) X(drop) ... the 45 names of assets/icons/icons.txt, in its order ...
typedef enum { /* UI_ICON_<name> */ ..., UI_ICON_COUNT } ui_icon_id_t;
/* ui_layout.h */
typedef struct { int16_t x, y, len; uint8_t vertical; } ui_sep_t;
typedef struct ui_layout { const char *id; const ui_slot_t *slots; int slot_count; const ui_sep_t *seps; int sep_count; } ui_layout_t;
```

- [ ] **Step 1: The failing tests.** In `test/host/test_ui_profile.c` add tests that pin the RLCD instance to today's literals and check the T5's:
  - `ui_px()` on the RLCD is the identity for −5..400; on the T5 `ui_px(1)` 2, `ui_px(4)` 7, `ui_px(10)` 17, `ui_px(20)` 34, `ui_px(-6)` −10, `ui_px(0)` 0.
  - RLCD fonts: `UI_FONT(UI_F_SANS_12) == &gfx_font_sans_12` … `UI_FONT(UI_F_NUM_130) == &gfx_font_num_cb_130` (all ten).
  - RLCD icons: `ui_icon(UI_ICON_thermometer, UI_IC48) == &gfx_icon_thermometer_48`; `ui_icon(UI_ICON_bolt, UI_IC48) == &gfx_icon_bolt_24`; `ui_icon(UI_ICON_web, UI_IC24) == &gfx_icon_web_16`; every entry non-NULL; `ui_icon_px(16|24|48)` 16/24/48; `ui_icon_class(47)` UI_IC24.
  - RLCD layouts: each of the four fixed layouts' slots equal today's `ui_layout.c` tables, copied into the test as literals, and their separators equal `ui_dashboard.c:8-35`'s calls (as `ui_sep_t` lists: Classic `{12,188,376,0},{100,199,90,1},{200,199,90,1},{300,199,90,1}`, Weather `{200,29,144,1},{208,101,184,0},{12,181,376,0},{200,190,102,1}`, Grid `{133,29,263,1},{267,29,263,1},{8,160,384,0}`, Focus `{12,211,376,0},{200,220,72,1}`).
  - RLCD menu `{30, 36, 34, 7}`; split `{40, 20, 150, 8}`; format 1 bpp; 400×300; status 20.
  - T5 (`ui_profile_use(&ui_profile_t547)`): 960×540, status 34, format 4 bpp, every font 4-bit, every icon non-NULL with `width == icon_px[class]`, menu `{51, 61, 55, 8}`, split `{68, 34, 255, 14}`, the layouts' slot counts 8/6/8/5.
  - The existing `test_the_split_area_and_status_bar_follow_the_profile` copies `ui_profile_t547` into a local, sets `status_h = 34`, and keeps its expectations (positional initializers break under `-Wmissing-field-initializers`).

Run: build → errors for the new names. Expected.

- [ ] **Step 2: The header and the list.** Write `ui_icons.h` with `UI_ICON_LIST` over the 45 names in `assets/icons/icons.txt`'s order and the `ui_icon_id_t` enum generated from it (`#define UI_ICON_ID(n) UI_ICON_##n,`). Extend `ui_profile.h` as the Interfaces block says; keep the caps and `ui_cap_name()`. Add `ui_sep_t` and the tagged `struct ui_layout` to `ui_layout.h`.

- [ ] **Step 3: The instances.** `ui_profile.c` keeps `ui_profile()`, `ui_profile_use()`, `ui_cap_name()`, and adds:

```c
int ui_px(int n)
{
    const ui_profile_t *p = ui_profile();
    if (p->px_num == p->px_den) {
        return n;
    }
    int num = n * p->px_num, den = p->px_den;
    return num >= 0 ? (num + den / 2) / den : -((-num + den / 2) / den);
}

const gfx_bitmap_t *ui_icon(ui_icon_id_t id, ui_icon_class_t cls)
{
    return (unsigned)id < UI_ICON_COUNT && (unsigned)cls < UI_IC_CLASSES ? ui_profile()->icons[id][cls] : NULL;
}

ui_icon_class_t ui_icon_class(int rlcd_px)
{
    return rlcd_px >= 48 ? UI_IC48 : rlcd_px >= 24 ? UI_IC24 : UI_IC16;
}

int ui_icon_px(int rlcd_px)
{
    return ui_profile()->icon_px[ui_icon_class(rlcd_px)];
}
```

and the default `s_profile = &ui_profile_rlcd42` (declared `extern` in the header for both instances).

`ui_profile_rlcd42.c`: the fonts table (`k_fonts[UI_F_COUNT]` of the ten RLCD fonts, by role), the icons table through three macros (`I3(n)` → `_16, _24, _48`; `I2(n)` → `_16, _24, _24` for bolt and stale; `I1(n)` → `_16` three times for web, sync, sync_failed, wifi, wifi_off), the four fixed layouts' slot tables moved verbatim from `ui_layout.c` with their separators as `ui_sep_t` lists (the Step 1 literals), the slot-less layouts as today, and:

```c
const ui_profile_t ui_profile_rlcd42 = {
    .board = "rlcd42", .width = 400, .height = 300, .status_h = 20, .caps = UI_CAPS_RLCD42,
    .format = GFX_FMT_1BPP, .px_num = 1, .px_den = 1, .fonts = k_fonts, .icons = k_icons,
    .icon_px = { 16, 24, 48 }, .layouts = k_layouts,
    .menu = { .header_h = 30, .row_y0 = 36, .row_h = 34, .rows = 7 },
    .split = { .min_w = 40, .min_h = 20, .narrow_w = 150, .inset = 8 },
};
```

`ui_profile_t547.c`: the same shape with the T5's fonts (`sans_20, sans_26, sans_34, bold_26, bold_34, bold_46, num_80, num_120, num_180, num_220` by role), `T5(n)` → `_26, _40, _80` over `UI_ICON_LIST`, the layouts of "The T5's numbers" above (slot names and kinds as the RLCD's: Classic's `sub` takes DATE|TEXT, S slots `UI_KINDS_S`, M `UI_KINDS_M`, L `UI_KINDS_L`, XL `UI_KINDS_XL`), and `.width = 960, .height = 540, .status_h = 34, .caps = UI_CAPS_T547, .format = GFX_FMT_4BPP, .px_num = 17, .px_den = 10, .icon_px = { 26, 40, 80 }, .menu = { 51, 61, 55, 8 }, .split = { 68, 34, 255, 14 }`.

`ui_layout.c`: `ui_layout(id)` returns `&ui_profile()->layouts[id]`; `ui_layout_by_name()` and `ui_slot_by_name()` read the same table; the static tables move out.

`components/ui/CMakeLists.txt`: SRCS gain `ui_profile_rlcd42.c` without the T5 (`if(CONFIG_REFLBO_BOARD_T547) list(APPEND srcs "ui_profile_t547.c") else() list(APPEND srcs "ui_profile_rlcd42.c") endif()`); `main/app.c`'s `UI_PROFILE_BOARD` already picks the board's. The host's `ui` library compiles both (it globs `components/ui/*.c`, or add both to its source list if it lists them).

- [ ] **Step 4: Run.** The new tests pass; `100% tests passed`; no golden listed; both builds; the RLCD ELF has no `ui_profile_t547` and the T5's no `ui_profile_rlcd42`.

- [ ] **Step 5: Commit** `feat(ui): the profile holds fonts, icons, the pixel scale, layouts, the menu and split limits (T3a)`.

---

### Task 3: Goldens for both boards

**Files:**
- Modify: `test/host/render_dashboard.c`, `test/host/render_screen.c`, `test/host/test_ui_dashboard_golden.c`, `test/host/test_ui_screens_golden.c`, `test/host/CMakeLists.txt`, `tools/render.py`, `tools/pbm_png.py`
- Create: `test/host/golden_io.h`
- Test: `test/host/test_golden_io.c`, `tools/tests/test_pbm_png.py`

**Interfaces:**
- Produces: `render_dashboard [--board t5] <fixture> <out>` and `render_screen [--board t5] <fixture> <out>`: the T5's write a gzip-compressed PGM when `out` ends `.pgm.gz` (`golden_io.h`: `bool golden_write(const char *path, const uint8_t *data, size_t n)`, `size_t golden_read(const char *path, uint8_t *buf, size_t size)`, both gzip-aware by suffix); `pbm_png.png_from_image()` reads gzip-compressed input; T5 goldens live in `test/host/golden/t5/{dash,screen}_<fixture>.pgm.gz` (spec ruling, Task 11: compressed, as 518 KB a golden would add ~50 MB).

- [ ] **Step 1: The failing tests.** `test_golden_io.c`: write 300 000 bytes of a pattern to `golden_io_test.pgm.gz`, read them back equal, and check the file is under 10 000 bytes; a plain path round-trips uncompressed. `test_pbm_png.py`: `png_from_image(gzip.compress(pgm))` equals `png_from_image(pgm)`. Run: missing `golden_io.h`; Python failure.

- [ ] **Step 2: `golden_io.h`** (header-only, `static inline`, zlib's `gzopen/gzread/gzwrite` for `*.gz`, stdio otherwise). Link `test_golden_io`, `render_*` and both golden tests to `ZLIB::ZLIB` as the png tests already are. `pbm_png.png_from_image()` starts with `if data[:2] == b"\x1f\x8b": data = gzip.decompress(data)`.

- [ ] **Step 3: The renderers.** Both take an optional leading `--board t5`: `ui_profile_use(&ui_profile_t547)`, a 4 bpp framebuffer of the profile's size (static `uint8_t buf[960 * 540 / 2]`, `gfx_fb_init_fmt`), `gfx_pgm_encode` into a static 960×540+32 buffer, `golden_write()`. Without it, exactly today's path. `--list` unchanged.

- [ ] **Step 4: The golden tests.** Each keeps its RLCD loop and gains `test_every_t5_fixture_matches_its_golden`: for each fixture in `k_t5_dashboard_fixtures` / `k_t5_screen_fixtures` (new lists in the fixture headers, empty until Task 9), render under the T5 profile and compare with `golden/t5/<kind>_<name>.pgm.gz` via `golden_read()`; on a mismatch write `t5_<kind>_<name>.actual.pgm.gz`. Restore the RLCD profile at the end (`ui_profile_use(NULL)`).

- [ ] **Step 5: `tools/render.py`** gains `--board t5`: renders every fixture of `render_dashboard --board t5 --list`… (the same names), writing `captures/render/t5/<prefix>_<name>.pgm.gz` and the PNG next to it. Without it, as today.

- [ ] **Step 6: Run and commit.** All tests pass, no golden listed, `python3 tools/render.py --board t5` writes 100 PNGs (the T5 frame drawn with whatever the profile gives so far). Commit `test: golden renders for the T5 as compressed PGMs (T3a)`.

---

### Task 4: Fonts by role

**Files:** Modify every `components/ui/*.c` that names a `gfx_font_*` (`ui_widget.c`, `ui_forecast.c`, `ui_solar.c`, `ui_status.c`, `ui_menu_draw.c`, `ui_config.c`, `ui_screens.c`, `ui_radar.c`, `ui_flights.c`); test: the RLCD goldens.

**Interfaces:** Consumes `UI_FONT()`, `ui_font_id_t`.

The mapping, applied to every occurrence: `&gfx_font_sans_12` → `UI_FONT(UI_F_SANS_12)`, `_sans_16` → `UI_F_SANS_16`, `_sans_20` → `UI_F_SANS_20`, `_sans_bold_16` → `UI_F_BOLD_16`, `_sans_bold_20` → `UI_F_BOLD_20`, `_sans_bold_28` → `UI_F_BOLD_28`, `_num_cb_48` → `UI_F_NUM_48`, `_num_cb_72` → `UI_F_NUM_72`, `_num_cb_110` → `UI_F_NUM_110`, `_num_cb_130` → `UI_F_NUM_130`.

- [ ] **Step 1: The guard.** Add to `tools/tests/` a unittest `test_ui_sources.py` that scans `components/ui/*.c` and fails on any `&gfx_font_` or `&gfx_icon_` outside `ui_profile_rlcd42.c` and `ui_profile_t547.c`, listing file:line. Run it: FAIL with ~200 font and ~150 icon lines. (Task 5 makes the icon half pass; until then the test checks fonts only, with an `ICONS = False` switch Task 5 turns on.)

- [ ] **Step 2: The table in `ui_widget.c`.** `k_fonts[]` (lines 22-28) becomes role ids: a struct `{ int label, value, text, unit, icon; }` of `ui_font_id_t` (−1 for no label) and a function `static ui_fonts_t size_fonts(ui_size_t size)` that resolves them through `UI_FONT()`; every `&k_fonts[s]` becomes a local `ui_fonts_t f = size_fonts(s);` passed as `&f`. The fit lists (`k_fit_s/m/l/xl`, `k_fit_xs`, `k_fit[]`) become arrays of `ui_font_id_t` with a helper `static int fonts_of(const ui_font_id_t *ids, int n, const gfx_font_t **out)` that fills a local array before `fit_number()`.

- [ ] **Step 3: The rest.** Function-local `static const gfx_font_t *const k_faces[] = {...}` lists lose `static` and take `UI_FONT()` values (automatic arrays may have non-constant initializers); `xs_unit_font`'s pointer comparison compares with `UI_FONT(UI_F_BOLD_28)`; every remaining single use takes the mapping. Do it with a script over the mapping, then fix what the compiler refuses by hand.

- [ ] **Step 4: Run.** `test_ui_sources.py` (fonts) passes; host build without warnings; `100% tests passed`; **no golden listed** (Review Focus 1); both firmware builds. `python3 tools/render.py --board t5` and read three PNGs (a Classic, the menu, the QR screen): the T5's fonts in the RLCD's geometry.

- [ ] **Step 5: Commit** `refactor(ui): fonts by role from the profile (T3a)`.

---

### Task 5: Icons by class

**Files:** Modify `ui_widget.c`, `ui_forecast.c`, `ui_solar.c`, `ui_status.c`, `ui_internal.h` (`ui_sky_icon`), the test helpers in `test_ui_solar.c` and `test_ui_widget_fit.c`; test: `test/host/test_ui_widget_fit.c`, `tools/tests/test_ui_sources.py`.

**Interfaces:** Consumes `ui_icon()`, `ui_icon_class()`, `ui_icon_px()`, `UI_ICON_<name>`. Produces `bool ui_bitmap_ink(const gfx_bitmap_t *b, int x, int y)` (`ui_internal.h`: 1 bpp bit, or 4-bit coverage ≥ 8).

- [ ] **Step 1: The failing tests.** `test_ui_widget_fit.c`: `ui_bitmap_ink()` on a hand-made 4-bit 3×1 bitmap `{0xF0, 0x80}` reads ink, no ink, ink; on a 1 bpp one as the old expression. `test_ui_sources.py` turns `ICONS = True`. Run: undeclared `ui_bitmap_ink`; the source scan lists the icon lines.

- [ ] **Step 2: Convert.** `&gfx_icon_<name>_<px>` → `ui_icon(UI_ICON_<name>, UI_IC<px>)`; `ui_forecast.c`'s `ICON(name)` becomes `ui_icon(UI_ICON_##name, ui_icon_class(size))`; `field_icon()` sets a `ui_icon_id_t` in its switch and returns `ui_icon(id, ui_icon_class(size))` (−1 for none → NULL). Wherever an icon's pixel size enters arithmetic as a literal (`(r.w - 24) / 2`, `cx - 12`, `x - 18`, `+ 18`, `size - 16`, `f->icon` in sums, `r.h >= 34 ? 24 : 16`), use the drawn bitmap's `width`/`height` or `ui_icon_px(<the RLCD px>)`. `bitmap_ink_box()`, `bolt_ink()` and the two test helpers read through `ui_bitmap_ink()` (Review Focus 4). `ui_status.c`'s 20 px icon step becomes `ui_icon_px(16) + UI_PX(4)`.

- [ ] **Step 3: Run.** As Task 4 Step 4, plus the new tests. Commit `refactor(ui): icons by size class from the profile; ink read from 4-bit icons (T3a)`.

---

### Task 6: `UI_PX()`

**Files:** Modify `ui_widget.c`, `ui_forecast.c`, `ui_status.c`, `ui_menu_draw.c`, `ui_config.c`, `ui_screens.c`, `ui_dashboard.c`, `ui_split.c`, `ui_split.h`; test: the RLCD goldens and the T5 renders.

**Interfaces:** Consumes `UI_PX()`, `ui_profile()->menu`, `ui_profile()->split`.

The rules, applied literal by literal (`ui_solar.c`, `ui_radar.c`, `ui_flights.c` are T3b's):
- A padding, gap, margin, inset, offset or fixed size in pixels inside a widget or screen (`r.x + 4`, `r.y + 12`, `gap = 6`, a rect's `h 28`, the toast's `- 62`): `UI_PX(n)`.
- A size threshold on a rect (`r.w < 150`, `r.h >= 80`, `r.w < 120 && r.h >= 44`): `UI_PX(n)`; where it is the split's narrow width, `ui_profile()->split.narrow_w`.
- An icon's size: Task 5's rule.
- A position on the RLCD's whole screen (`{120, 0, 160, ...}` for the status clock, `ui_draw_battery(fb, 130, 44, 140, 64, …)`, `baseline = 110`, `FOOTER_Y 282`): rewrite relative to `fb->width`/`fb->height` and `UI_PX()` so that the RLCD gets the same number (the status clock: `{(fb->width - UI_PX(160)) / 2, 0, UI_PX(160), UI_STATUS_H}`; the menu's footer `fb->height - UI_PX(18)`).
- The menu's `HEADER_H`, `ROW_Y0`, `ROW_H`, `ROWS` come from `ui_profile()->menu`; the split's `UI_SPLIT_MIN_W/MIN_H/NARROW_W/INSET` macros read `ui_profile()->split`, and `k_sizes`' widths and heights and `k_needs` are scaled by `UI_PX()` at use (XL's `min_w` 400 → `UI_PX(400)`).
- Unchanged: counts, indices, percentages, string sizes, font metrics, values computed from rects and fonts, and the ±1 that places a thing beside a 1 px line (`UI_STATUS_H + 1`).
- `ui_dashboard.c`'s separators come from the layout's `seps` (Task 2), drawn with `gfx_hline`/`gfx_vline`.

- [ ] **Step 1: The guard.** `test_ui_profile.c` gains a test that renders every RLCD dashboard and screen fixture once under a copy of the RLCD profile with `px_num = px_den = 3` (the identity by another path) and compares with the goldens: `ui_px()` with equal num and den must change nothing. It passes before and after; it guards Step 2's edits beside the goldens.

- [ ] **Step 2: Convert file by file**, building and running the golden tests after each file (`ctest --test-dir build-host -R golden`): a file whose goldens change has a literal converted that the rules leave alone, or a rewrite that changed the RLCD's number; fix it before the next file.

- [ ] **Step 3: Look at the T5.** `python3 tools/render.py --board t5`; read the Classic, Weather, Grid and Focus fixtures' PNGs, the menu, the QR, first-run, critical and toast screens. Fix anything that overlaps or clips because a literal escaped the rules (still in the RLCD's geometry, as Task 7 brings the T5's layouts).

- [ ] **Step 4: Run and commit.** RLCD goldens unchanged, `100% tests passed`, both builds. Commit `refactor(ui): pixel sizes through UI_PX() and the profile's menu and split limits (T3a)`.

---

### Task 7: The T5's layouts, menu, screens and presets

**Files:** Modify `components/ui/ui_profile_t547.c` (if Task 2's tables need the renders' corrections), `components/ui/ui_preset.c`; test: `test/host/test_ui_profile.c`, `test/host/test_ui_preset.c` (or the presets' existing test file).

**Interfaces:** Consumes Task 2's T5 tables. Produces T5 default presets as "The T5's numbers".

- [ ] **Step 1: The failing tests.** Geometry (Review Focus 3): under the T5 profile every fixed layout's slots and separators lie within `{0, 35, 960, 505}`, slots don't overlap, each slot is at least `k_sizes[size]`' minimum after `UI_PX()`. Presets: under the T5 profile `ui_presets_defaults()` gives home/sky/weather/focus with the slots of the table above and no `UI_FIELD_ENV_*` anywhere; under the RLCD's, exactly today's (compare field by field with today's literals).

- [ ] **Step 2: The presets.** `ui_presets_defaults()` branches on `ui_profile()->caps & UI_CAP_ENV_SENSOR`: with it, today's code; without it, the T5's four (the `sky` preset named "Sky", in the cycle), then `ui_presets_offer_builtins()` as today. The status clock as today (on Grid and Weather).

- [ ] **Step 3: Run, render, look.** Tests pass; `python3 tools/render.py --board t5`; read each fixed layout's render and the menu. Correct the T5 tables where the renders show a problem (ledger each change); the RLCD goldens unchanged.

- [ ] **Step 4: Commit** `feat(ui): the T5's layouts, menu and presets (T3a)`.

---

### Task 8: The T5's display hands the UI its frame

**Files:** Modify `components/display/display_t547.c`, `components/display/include/display_board.h`, `main/app.c`, `components/epaper/` (drop the 1 bpp blit's use), `AGENTS.md`; test: `test/host/test_ui_profile.c` (`ui_profile_matches`).

**Interfaces:** Produces `bool ui_profile_matches(const gfx_fb_t *fb)` (`ui_profile.h`: width, height and format equal the profile's); `display_fb()` on the T5 returns the 960×540 4 bpp frame.

- [ ] **Step 1: The failing test.** `ui_profile_matches()`: true for a 400×300 1 bpp fb under the RLCD profile and a 960×540 4 bpp one under the T5's; false for each one-field mismatch. Run: undeclared.

- [ ] **Step 2: The display.** `display_t547.c`: one frame, `s_fb`, 960×540 4 bpp in PSRAM, white at allocation; `display_fb()` returns it, `display_screenshot_fb()` too; `display_commit()` CRCs its 259 200 bytes, skips when unchanged and pushed, else `clean_update()` copies it into epdiy's front buffer; `clean_update(void)` takes no parameter (T2 review); the commit records the CRC and `s_pushed` only after the update succeeded (T2 review). `display_t5_test_pattern()` draws into `s_fb` and sets `s_pushed = false` before updating; the app's next render redraws the frame. `display_t5_bench()` sets `s_pushed = false` before `panel_up()` (T2 review) and draws its pattern into `s_fb`. Drop `FB_W/FB_H/FB_X/FB_Y` and the 1 bpp blit (the `epaper_frame` blit stays in the component for its tests until T4 decides; the display no longer calls it).

- [ ] **Step 3: The check at boot.** In `main/app.c`'s `boot()`, after `display_init*`: `if (!ui_profile_matches(display_fb())) { ESP_LOGE(TAG, "the display's frame doesn't match the UI profile"); return ESP_ERR_INVALID_STATE; }` (Review Focus 5) — a failed boot, as any other.

- [ ] **Step 4: Run.** Host tests; both builds; symbol checks (the T5's ELF has no `gfx_font_sans_` the UI dropped, except those `gfx_draw_test_pattern` still uses — list what remains and ledger it); `REFLBO_BOARD=t5 tools/idf.sh size` under 3.6 MB (Global Constraints). AGENTS.md §10 Seams: the T5's display hands the UI its 960×540 4 bpp frame (no composition); the profile carries fonts, icons, `UI_PX()`, layouts, menu and split limits (one source a board).

- [ ] **Step 5: Commit** `feat(display): the T5's UI draws its 960×540 gray frame directly (T3a)`.

---

### Task 9: The renders, the owner, the goldens

- [ ] **Step 1: The render set.** Fill `k_t5_dashboard_fixtures` with every dashboard fixture on Classic, Weather, Grid and Focus, and `k_t5_screen_fixtures` with every screen fixture (menu, toast, critical, config, first run). `python3 tools/render.py --board t5`. Read every one; fix what's wrong (a ruling a fix), re-render.

- [ ] **Step 2: Publish them for the owner.** Build a single HTML page with the T5 renders side by side with their RLCD counterparts (the PNGs inlined), and send it as an Artifact (load the artifact-design skill first).

- [ ] **Step 3: Ask the owner** (DT2): "Here are the T5's dashboards, menu and screens as the firmware will draw them. Do they work for you? Anything too small, crowded or empty?" Stop until they answer. Apply their changes (each a ledgered ruling), re-render, and ask again until they approve.

- [ ] **Step 4: The goldens.** For each T5 fixture: `build-host/render_dashboard --board t5 <name> test/host/golden/t5/dash_<name>.pgm.gz` and `render_screen` likewise; the T5 golden tests pass; commit `test: the T5's approved goldens (T3a)` with the owner's approval date in the body.

---

### Task 10: At the board

The owner allows flashing the T5; nothing here erases.

- [ ] **Step 1:** confirm the MAC, flash at 230400, `devlog --reset` the boot (no `E (`, `reflbo ready`).
- [ ] **Step 2:** `rtc set` from the Mac (clockcmp), let it draw the home preset; `tools/screenshot.py -p … -t 150` and compare with the golden of the fixture closest to it by eye (the live data differ).
- [ ] **Step 3:** cycle the presets with `btn key short` (Home, Sky, Weather, Focus), a screenshot each; open the menu (`btn key long`), a screenshot.
- [ ] **Step 4: Ask the owner** for a photo of Home and of the menu at the board; ledger what they say.
- [ ] **Step 5:** if Steps 1–4 changed code, the suite, both builds, commit `fix: board findings on the T5's native frame (T3a)`.

---

### Task 11: Record T3a

- [ ] **Step 1: The spec, r6.** §11: T3 becomes T3a (the native frame), T3b (the views and §6.4's grays), T3c (the web page), each with its "Done when"; T3a done (date). §6.5: the T5's goldens are gzip-compressed PGMs (`.pgm.gz`), as plain ones are 518 KB each. §7.1–7.3: as built (the profile's members, `UI_PX` = 17/10, the T5's tables as the owner approved them, the presets). §13 r6.
- [ ] **Step 2: AGENTS.md §10** Status (T3a done, next T3b), the bench bullet (render with `--board t5`), §6's render commands.
- [ ] **Step 3:** checks (both builds, the suite, the RLCD goldens), commit `docs: T3a as built (T5 spec r6)`, tag `t5-stable-t3a`, push `main` and the tag.
