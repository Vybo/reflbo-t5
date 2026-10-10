# T3b: The Views and the Grays Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The T5 draws its four views (Rain radar, Flights, Solar, Energy) and the radar, chart and flow widgets at its native 960×540, with the grays spec §6.4 names, the radar's fetch covering what the wider map shows; the RLCD draws exactly as before.

**Architecture:** T3a's UI profile gains the map's zoom step for the T5's denser pixels; the map component takes its label font, its marks' scale and its line colour from a style the UI fills from the profile, so it names no board's font; the radar's fetch takes the view's size and zoom from the app, which reads them from the same helpers the views draw with. The gray uses (rain levels, the chart's forecast bars, the map's borders, the status bar's line) are keyed to a 4 bpp frame, so the RLCD's 1 bpp drawing, dithers included, is untouched. `ui_solar.c`, `ui_radar.c` and `ui_flights.c` get T3a's `UI_PX()` pass.

**Tech Stack:** C17 (ESP-IDF 5.5.5, the host's Unity tests, zlib on the host), Python 3, epdiy 2.1.3.

**Spec:** [`docs/specs/2026-10-06-t5-board-design.md`](../specs/2026-10-06-t5-board-design.md) r6: §6.4 (where the T5 uses gray), §7.1 (the profile), §7.3 (Radar and Flights "full width, 16:9", Solar and Energy "rescaled, wider chart"), §11 T3b, §12 (radar frames at 960 px, the image's size); DT2 (renders before goldens).

## Global Constraints

- The RLCD draws as before: `git status --porcelain test/host/golden` lists nothing outside `t5/` after every task, the radar, flights, solar and energy goldens included.
- "Text and icon edges; the radar's rain intensity (in place of `radar_inks()`' dither); the solar chart's fill; the map's land and borders; the status bar's separators" use gray on the T5 (§6.4). "T3 takes its grays from the dark half": every gray level is 0–8.
- "Fixed paddings inside widgets become `UI_PX(n)`" (§7.1); T3a's rules apply (AGENTS.md §10; a divisor stays a divisor, a count stays a count, the split's narrow width is `UI_SPLIT_NARROW_W`).
- Radar and Flights: "full width, 16:9" below the status bar (§7.3): the Radar map is the split area, 960×505; Flights is its map over the panel.
- DT2: the owner approves the renders before their goldens are committed (Task 6 stops for that).
- `[host]` components include no ESP-IDF headers; `map` and `radar` don't depend on `ui` (the UI depends on them).
- The T5's image stays under 3.6 MB (`REFLBO_BOARD=t5 tools/idf.sh size`); its PSRAM is 4 MB mapped (T1).
- The fork never flashes the RLCD (DT8). The T5: `/dev/cu.usbserial-52D60046741`, MAC `34:ab:95:5e:5d:58`, `-b 230400`; flashing needs no new consent, erasing does. Console commands to the T5 are paced, one after each update (T5 gotcha T10).
- No AI or assistant attribution in commits, code or docs.

## Review Focus

1. **The fetch and the drawing disagreeing on the T5.** Expectation: the radar's fetch asks for the tiles of the very view the Radar layout draws (960×505 at the shown zoom), so RainViewer's rain reaches every edge; the flights' filter keeps every aircraft the wider map shows. Pinned by Task 2's and Task 3's tests that build the fetch's view and the drawn view through one helper.
2. **The RLCD changing.** Expectation: gray paths run only on a 4 bpp frame; the map's style on the RLCD is its old font, marks and black lines. Pinned by the RLCD goldens after every task.
3. **Grays the panel can't tell from white.** Expectation: every gray drawn is 0–8. Pinned by Task 1's and Task 2's tests that read the levels drawn.
4. **PSRAM on the T5 with RainViewer's loop.** Expectation: the hour's six RainViewer frames at the T5's view fit with the rest. Pinned by Task 2's size test (a frame for the T5's view is at most 5 × 3 tiles, 240 KB) and Task 7's `heap` at the board.
5. **The wider views' labels.** Expectation: a town, ring or aircraft label never overlaps another or the caption and legend boxes at 960×505. Pinned by Task 1's test at the T5's scale.

---

### Task 1: The map takes its font, marks and lines from the UI

**Files:**
- Modify: `components/map/include/map_draw.h`, `components/map/map_draw.c`, `components/ui/ui_radar.c`, `components/ui/ui_flights.c`, `components/ui/include/ui_profile.h`, `components/ui/ui_profile_rlcd42.c`, `components/ui/ui_profile_t547.c`
- Test: `test/host/test_map_draw.c`, `test/host/test_ui_profile.c`

**Interfaces:**
- Produces: `map_style_t` gains `const gfx_font_t *font` (labels; required), `int px_num, px_den` (the marks' scale; 0/0 reads as 1/1), `gfx_color_t line` (borders and coasts; 0 reads as `GFX_BLACK`). `map_draw_home(fb, area, v, lat_e4, lon_e4, s, l)` and `map_draw_rings(fb, area, v, range_m, s, l)` take the style. `void ui_map_style(map_style_t *s)` (`ui_internal.h`) fills font, scale and line from the profile: on the RLCD `UI_FONT(UI_F_SANS_12)`, 1/1, `GFX_BLACK`; on the T5 its sans 20 role, 17/10, `GFX_GRAY(6)`.

- [ ] **Step 1: The failing tests.** `test_map_draw.c`: with `s.px_num = 17, s.px_den = 10` a town's dot is 9×9 (5 scaled) and home's ⊙ radius scales (`HOME_R` × 1.7); a style's `line = GFX_GRAY(6)` draws a border at level 6 into a 4 bpp frame; labels use `s.font` (a label's box height is `s.font->line_height + 2`). Update the existing calls to the new signatures (font `&gfx_font_sans_12`, scale 0/0). `test_ui_profile.c`: `ui_map_style()` on the RLCD gives `UI_FONT(UI_F_SANS_12)`, 1/1, `GFX_BLACK`; on the T5 a 4-bit font, 17/10, a gray ≤ 8. Run: compile errors (members, signatures).
- [ ] **Step 2: The map.** Scale every mark through one helper, `static int px(const map_style_t *s, int n)` (rounded half away from zero, n when den is 0): the town dot 5×5 and its 3×3 core, the airport's 9×4 mark and 7×2 runway, home's ⊙ (`HOME_R`) and its 3×3 centre, the halo's 1 px stays 1. Every `&gfx_font_sans_12` becomes `s->font`. Lines draw in `s->line ? s->line : GFX_BLACK`; the halo stays white. `map_draw.c` names no font after this (its includes drop `gfx_fonts.h`).
- [ ] **Step 3: The views.** `ui_radar.c` and `ui_flights.c` start their styles from `ui_map_style()` and add their own `airports`, `halo`, `max_towns`; the ring and home calls pass the style.
- [ ] **Step 4: Run.** The new tests pass; the RLCD goldens unchanged; `nm` on the T5 ELF: `gfx_font_sans_12` remains only if `gfx_draw_test_pattern` still names it (ledger what remains); both builds.
- [ ] **Step 5: Commit** `feat(map): its font, marks and lines from the style the UI fills (T3b)`.

### Task 2: The Rain radar at 960×505 with gray rain

**Files:**
- Modify: `components/radar/include/radar.h`, `components/radar/include/radar_fetch.h`, `components/radar/radar_fetch.c`, `components/radar/radar_rainviewer.c`, `components/radar/radar_frame.c` (`radar_render`), `components/ui/ui_radar.c`, `components/ui/include/ui_radar.h`, `components/ui/include/ui_profile.h`, both profiles, `main/app_radar.c`
- Test: `test/host/test_radar.c`, `test/host/test_ui_radar.c` (or the radar view's existing test file)

**Interfaces:**
- Consumes: Task 1's `ui_map_style()`.
- Produces: `void ui_radar_fetch_size(uint8_t zoom_q, uint8_t *fetch_zoom_q, uint16_t *w, uint16_t *h)` (`ui_radar.h`): what `app_radar.c` puts in the fetch request; the profile's `uint8_t map_zoom_q` (quarters added to a map's zoom: RLCD 0, T5 3, so a kilometre spans about 1.7× the pixels); `void ui_radar_view(int32_t lat_e4, int32_t lon_e4, uint8_t zoom_q, gfx_rect_t r, map_view_t *v)` (`ui_radar.h`): the view any radar map draws in `r`, its zoom the setting's plus the profile's; `radar_fetch_req_t` gains `uint16_t view_w, view_h` (`RADAR_VIEW_W/H` go); `RADAR_RV_TILES_MAX` is 5; on a 4 bpp frame `radar_render()` fills rain in levels light `GFX_GRAY(8)`, moderate `GFX_GRAY(4)`, heavy black, and `radar_level_color(level)` gives them for the legend.

- [ ] **Step 1: The failing tests.** `radar_rv_tiles()` for a 960×505 view at zoom 7.25 over Berlin takes every tile the view touches (nx ≤ 5, ny ≤ 3, `radar_frame_covers()` true for that view); the same at 400×279 gives today's counts. A frame for that view is at most 5 × 3 × 256² / 4 + 48 bytes. `radar_render()` into a 4 bpp frame draws light, moderate and heavy as 8, 4 and 0, and into a 1 bpp frame exactly `radar_inks()`' pattern. `ui_radar_view()` on the T5 for the Radar layout's rect (`ui_split_area()`) gives 960×505 at the setting's zoom + 0.75; `ui_radar_fetch_size(zoom_q, &fetch_zoom_q, &w, &h)` gives exactly that view's zoom (in quarters) and size on both boards (RLCD: the setting's zoom, 400×279). Run: missing symbols.
- [ ] **Step 2: The fetch.** `radar_fetch_req_t.view_w/view_h` replace `RADAR_VIEW_W/H` in `radar_fetch.c`; `RADAR_RV_TILES_MAX` 5; `app_radar.c` fills the request's zoom and size from `ui_radar_fetch_size()`, and builds its own coverage view (line 72) through `ui_radar_view()` over `ui_split_area()`; `FRAME_FILE_MAX` from `RADAR_RV_TILES_MAX`.
- [ ] **Step 3: The drawing.** `draw_map()` builds its view with `ui_radar_view()`; `PAD`, `SWATCH_W/H`, the legend's gaps, the loop's dot pitch and radii, the "no frame" box: `UI_PX()`; the legend's swatches fill with `radar_level_color()` on a 4 bpp frame, the dither on 1 bpp; `max_towns` for XL 12 → 20 on the T5 (`UI_PX`-scaled area holds more).
- [ ] **Step 4: Run, render, look** (`tools/render.py --board t5`: the radar fixtures and `grid_rain_map`, `weather_rain_map`). RLCD goldens unchanged; both builds.
- [ ] **Step 5: Commit** `feat(radar): the T5's Rain radar at 960×505 with gray rain, fetched for the view it draws (T3b)`.

### Task 3: Flights at 960×505

**Files:**
- Modify: `components/ui/ui_flights.c`, `components/ui/include/ui_radar.h`, `main/app_flights.c`, `test/host/radar_fixtures.h`
- Test: `test/host/test_ui_flights.c` (or the flights view's existing test file)

**Interfaces:**
- Produces: `gfx_rect_t ui_flights_map_rect(gfx_rect_t below)` (`ui_radar.h`): the map's part of the view, above the panel (`UI_PX(40)`) and its line; `UI_FLIGHTS_MAP_H` goes. `void ui_flights_view(int32_t lat_e4, int32_t lon_e4, uint8_t range_km, gfx_rect_t map, map_view_t *v)`: the view the map draws and the app's filter uses.

- [ ] **Step 1: The failing tests.** `ui_flights_map_rect(ui_split_area())` is 400×238 at y 21 on the RLCD and 960×436 at y 35 on the T5; the app's filter view (`request()`'s, through `ui_flights_view()`) equals the drawn one on both; on the T5 an aircraft at the map's right edge passes the filter. Run: missing symbols.
- [ ] **Step 2: The view.** `ui_draw_flights_view()` and `app_flights.c`'s `request()` take the map from `ui_flights_map_rect()` and the view from `ui_flights_view()`; `radar_fixtures.h` too. `PANEL_H`, `NEAREST_R`, `ARROW_BOX`, `k_arrow`'s points (scaled in `arrow_at()`), the halo's offsets stay 1, the panel's margins: `UI_PX()`.
- [ ] **Step 3: Run, render, look** (the six flights fixtures). RLCD goldens unchanged; both builds.
- [ ] **Step 4: Commit** `feat(ui): the T5's Flights at 960×505, filtered for the map it draws (T3b)`.

### Task 4: Solar, Energy, the chart and the flow at 960×540

**Files:**
- Modify: `components/ui/ui_solar.c`
- Test: `test/host/test_ui_solar.c`

**Interfaces:**
- Consumes: T3a's `UI_PX()`, `ui_icon_px()`.

- [ ] **Step 1: The failing tests.** Under the T5 profile: the Solar layout's ink lies inside its area and its two day totals' row ends above the area's bottom; the Energy layout's grid and house nodes and their values lie inside it; the chart's forecast bars still to come are filled with a gray ≤ 8 (read a level inside a future bar), the past and actual ones black; the flow widget in a 480×144 cell keeps its values inside. Run: fails (ink outside, outlined bars).
- [ ] **Step 2: Convert** `ui_solar.c` literal by literal with T3a's rules (the layouts' absolute offsets such as `a.y + 82`, `a.x + 196`, `152`, the chart's label gaps, the flow's offsets, `jy`, `gx`, `hx`, the battery's 30×14); the chart's future bars `gfx_fill_rect(…, GFX_GRAY(8))` on a 4 bpp frame, `gfx_rect` on 1 bpp as before.
- [ ] **Step 3: Run, render, look** (`solar*`, `energy*`, `grid_solar*`, `weather_solar`, `focus_solar`, `home_energy`). RLCD goldens unchanged; both builds.
- [ ] **Step 4: Commit** `feat(ui): the T5's Solar and Energy layouts, chart and flow at its scale, the forecast in gray (T3b)`.

### Task 5: The status bar's line, and the views in the T5's cycle

**Files:**
- Modify: `components/ui/ui_status.c`, `components/ui/ui_preset.c`
- Test: `test/host/test_ui_profile.c`, `test/host/test_ui_preset.c`

- [ ] **Step 1: The failing tests.** On the T5 the status bar's line is a gray ≤ 8 (read at x 480, y 34); the RLCD's stays black. The T5's defaults put Rain radar and Flights in the cycle and leave Solar and Energy out, as the RLCD's (update `test_the_t5_defaults_have_no_env_field`).
- [ ] **Step 2:** the line `GFX_GRAY(6)` on a 4 bpp frame; drop the T5's out-of-cycle loop in `ui_presets_defaults()` (T3a review's minor 2 goes with it).
- [ ] **Step 3: Commit** `feat(ui): the T5's status line in gray; its views back in the cycle (T3b)`.

### Task 6: The renders, the owner, the goldens

- [ ] **Step 1:** `k_t5_dashboard_fixtures` gains the radar, flights, solar and energy fixtures and the six T3a left out (`grid_rain_map`, `weather_rain_map`, `grid_solar`, `weather_solar`, `focus_solar`, `grid_solar_low`); `render.py --board t5`; read each; fix what's wrong (a ruling a fix).
- [ ] **Step 2:** the review page (the T3a page's form: the T5 render beside the RLCD's), published as an Artifact.
- [ ] **Step 3: Ask the owner** (DT2), stop until they answer; apply their changes (rulings), re-render, ask again until they approve.
- [ ] **Step 4:** the goldens (`render_dashboard --board t5 … test/host/golden/t5/…pgm.gz`); the T5 golden tests pass; commit `test: the T5's approved views (T3b)` with the approval date.

### Task 7: At the board

- [ ] **Step 1:** MAC, flash at 230400, boot clean; clock from the Mac.
- [ ] **Step 2:** the T5 has no saved network since T3a's factory reset: ask the owner to set up Wi-Fi from a phone (Menu ▸ Wi-Fi, or BOOT long), then `sync now`; `radar status`, `heap` (PSRAM free with the radar's frames: Review Focus 4).
- [ ] **Step 3:** each view through `preset set`: rain, flights (sync mode `always`: BOOT double, or `flights_off` shows the panel), solar (`solar demo on`), energy (`solar demo on` gives the house's day too); a screenshot each.
- [ ] **Step 4: Ask the owner** for a photo of the Rain radar and of Solar; ledger what they say; fix findings (a commit `fix: board findings on the T5's views (T3b)`).

### Task 8: Record T3b

- [ ] **Step 1:** the spec r7: §6.4 as built (the levels used), §7.1 (the map's zoom step, the map style), §7.3 (the views' geometry), §11 T3b done, §12 (PSRAM measured), §13.
- [ ] **Step 2:** AGENTS.md §10: status (T3b done, next T3c), the radar's fetch view.
- [ ] **Step 3:** checks (both builds, the suite, the RLCD goldens), commit `docs: T3b as built (T5 spec r7)`, tag `t5-stable-t3b`, push `main` and the tag.

## Not in T3b

- The web page and the gray previews: T3c.
- Fast updates, the radar loop's frame time on e-paper, the clean counter: T4.
- Hiding the menu's items for missing hardware (T5 spec §9): a later milestone; T3a's review minors 1, 3 and 4 wait where T3b doesn't touch their code.
