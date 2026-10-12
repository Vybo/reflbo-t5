# T4c: titles and shared presets — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Every slot and split cell can carry its own title, a split cell can show its title whatever its size, and the web page exports a preset to a file and imports one, on both boards; then the same code goes upstream (DT14).

**Architecture:** Titles live in one 768-byte pool in `ui_presets_t` (the presets ride the deep-sleep snapshot, which has no room for a title a slot), the force bits in `ui_preset_t`. The renderer gets a preset's titles as a `ui_titles_t` view beside the preset. A predicate says whether a widget shows its label at a size; a split cell with a forced title whose widget doesn't show one gets a title line above, or beside, the field. Export and import are the web page's alone: the device already validates a preset in `POST /api/preview.bmp` and `PUT /api/presets`.

**Tech Stack:** C17, ESP-IDF 5.5.5, Unity host tests, cJSON, plain JS with node's test runner.

**Spec:** `docs/specs/2026-10-06-t5-board-design.md` r14: DT14, §7.5 (the source of truth for the rules below).

## Global Constraints

- The RLCD's existing goldens stay byte-identical; so do the T5's (only new fixtures get new goldens).
- The owner approves the new renders before their goldens are written (DT2); the RLCD's new golden goes with them.
- Both boards build clean (`tools/idf.sh build`, `REFLBO_BOARD=t5 tools/idf.sh build`); host suite (`cmake --build build-host`, `ctest --test-dir build-host`) and the web tests green; run as separate commands.
- The snapshot stays within its new 7 KB limit on both boards (`_Static_assert` in `main/app.c`); `SNAP_VERSION` goes up.
- `presets.json` stays readable by older firmware: new keys only, never a new shape for an old one.
- Board rules (AGENTS.md §10): MAC `34:ab:95:5e:5d:58` on `/dev/cu.usbserial-52D60046741` before flashing, `-b 230400`; never erase; the owner's web password is typed by the owner (flash first).
- Commits: Conventional Commits, no trailers, nothing about tools or assistants.

## Review Focus

1. **The title pool when presets change in C** (the built-ins offered at the end, a factory reset): titles stay with their preset's index or go with it. Pinned in Task 1 (`ui_presets_offer_builtins` leaves titles; defaults clear them).
2. **A forced title where the stale policy hides the value:** the cell stays empty, title included. Pinned in Task 3.
3. **A title of multi-byte letters at the 23-byte limit:** cut at a letter's boundary, never inside "ř". Pinned in Task 1.
4. **Import of a preset the board can't show** (a T5 tree on the RLCD, an unknown field): refused with the device's reason, nothing added, the editor unchanged. Pinned in Task 4.
5. **Editing a split tree with titles** (Split, Join, a ratio that drops a field): a title and its force go with their field. Pinned in Task 4.

---

### Task 1: Titles in presets.json and the pool

**Files:** `components/ui/include/ui_preset.h`, `components/ui/ui_preset.c`, `components/ui/ui_preset_json.c`, `main/app.c` (snapshot limit and version); test `test/host/test_ui_preset.c`.

**Interfaces — produces:**

```c
#define UI_TITLE_LEN 24     /* a title with its terminator: as a preset's name */
#define UI_TITLES_BYTES 768 /* every preset's titles together (T5 spec §7.5) */
/* ui_preset_t: */ uint32_t force_title; /* split cells (bit = cell) whose title shows whatever their size */
/* ui_presets_t: */ uint8_t titles[UI_TITLES_BYTES]; /* records: preset index, slot, text, 0; index 0xFF ends */
typedef struct { const char *text[UI_SLOT_MAX]; } ui_titles_t; /* a preset's titles; NULL: the field's label */
const char *ui_presets_title(const ui_presets_t *p, int preset, int slot);           /* NULL for none */
bool ui_presets_set_title(ui_presets_t *p, int preset, int slot, const char *text); /* "" or NULL clears; false: no room */
void ui_presets_titles(const ui_presets_t *p, int preset, ui_titles_t *out);
```

JSON: a split cell's `"title"` and `"force_title"`, a fixed preset's `"titles": {"<slot>": "…"}`; written only when set; a title cut to 23 bytes at a letter's boundary, control characters as spaces; a file whose titles overflow the pool is refused ("titles: N bytes at most in all presets").

- [ ] Failing tests: `test_titles_round_trip` (a split cell's title and force, a fixed slot's title, back through `ui_presets_to_json`/`from_json`); `test_titles_are_cut_at_a_letter` (24 bytes of "ř…" → 22 bytes); `test_title_control_characters_become_spaces`; `test_titles_overflow_the_pool_is_refused`; `test_titles_unset_are_not_written` (no `title`, `force_title` or `titles` keys in a file without them); `test_set_title_keeps_order_and_clears` (set out of order, look up, overwrite, clear); `test_offer_builtins_keeps_titles`; `test_defaults_have_no_titles`; the worst-case file test gains a title on every cell up to the pool and `force_title` on all, and still fits `UI_PRESETS_JSON_MAX` (raise it if it must, keeping `app_web.c`'s backup assert).
- [ ] Run: FAIL. Implement. Run: PASS; suite green.
- [ ] `main/app.c`: `_Static_assert(sizeof(app_snapshot_t) <= 7168, …)` (spec §7.5), `SNAP_VERSION` 13. Both boards build; note the snapshot's size from each ELF (`nm -S`).
- [ ] Commit `feat(ui): titles for slots and cells in presets.json (T5 spec §7.5, DT14)`.

### Task 2: Whether a widget shows its label

**Files:** `components/ui/ui_widget.c`, `components/ui/ui_forecast.c`, `components/ui/ui_solar.c`, `components/ui/ui_internal.h`, `components/ui/include/ui_dashboard.h` (declaration); test `test/host/test_ui_titles.c` (new; `test/host/CMakeLists.txt`).

**Interfaces — produces:** `bool ui_widget_labelled(const ui_value_t *v, gfx_rect_t r, ui_size_t size, ui_stale_policy_t policy);` — whether `ui_widget_draw()` with these arguments draws `v->label`; `ui_forecast_labelled()` and `ui_solar_labelled()` answer for their widgets, next to their code (the flow's label shows only where it fits: one helper decides for both).

- [ ] Failing test `test_labelled_matches_what_is_drawn`: for every field, every size its kind takes, in a cell at that size's least width and height and in a large one, wide and narrow, fresh (the dashboard fixture's data) and missing: draw with the label "Wq" and with "" and compare; the predicate says true exactly when they differ. Both boards' profiles.
- [ ] Run: FAIL (undefined). Implement. Run: PASS; suite green.
- [ ] Commit `feat(ui): which widgets show their label at a size (T5 spec §7.5)`.

### Task 3: Titles on the dashboard

**Files:** `components/ui/include/ui_dashboard.h`, `components/ui/ui_dashboard.c`, `main/app_ui.c`, `main/app_web.c`, the host callers of `ui_draw_dashboard()` (`test/host/*.c`, `test/host/screen_fixtures.h`, `test/host/render_dashboard.c`), `test/host/dashboard_fixtures.h` (fixture `split_titles` and `fixture_titles()`), `test/host/test_ui_dashboard_golden.c` (its list); test `test/host/test_ui_titles.c`.

**Interfaces — consumes:** Task 1's `ui_titles_t`, `ui_presets_titles()`, `force_title`; Task 2's `ui_widget_labelled()`. **Produces:**
- `void ui_draw_dashboard(gfx_fb_t *fb, const ui_context_t *ctx, const ui_preset_t *preset, const ui_titles_t *titles);` (`titles` NULL: none).
- `bool ui_draw_titled_cell(gfx_fb_t *fb, gfx_rect_t cell, const ui_context_t *ctx, ui_field_id_t field, ui_stale_policy_t policy, const char *title, bool force);` with `ui_draw_cell()` its untitled form.

- [ ] Failing tests: `test_a_title_replaces_the_label` (an M cell, a fixed layout's M slot); `test_forced_title_changes_nothing_where_a_label_shows` (an M cell forced == not forced); `test_forced_title_goes_above_in_a_tall_small_cell` (an S cell: the title's line inked, the field below); `test_forced_title_goes_beside_in_a_short_wide_cell` (an XS row: title on the left, value on the right); `test_forced_title_without_room_shows_the_value_alone` (a least-size cell: equals the unforced drawing); `test_hidden_value_hides_its_title` (missing, policy hide: the cell blank).
- [ ] Run: FAIL. Implement (the rules of spec §7.5; the title line at `UI_PX(6)` like a label; beside: its font's baseline centred on the cell). Callers pass `ui_presets_titles()` (`app_ui.c`, `app_web.c`'s preview) or NULL (tests). Run: PASS; suite green; every existing golden unchanged.
- [ ] The fixture `split_titles` (both boards): a split preset with a title on an M cell, forced titles on an S cell, an XS row and a clock, and an unforced S cell with a title (not shown). `python3 tools/render.py` and `--board t5`; look at both; **the owner approves the renders**, then write the two goldens.
- [ ] Commit `feat(ui): titles on the dashboard, and a split cell's forced title (T5 spec §7.5)`.

### Task 4: The Presets page

**Files:** `web/app.js`, `web/style.css` if needed; test `test/web/test_app.mjs`.

- [ ] Failing tests (the fake device): `title inputs edit a cell's and a slot's title` (typing sets `split…title` / `titles[slot]`, an empty one removes the key; placeholder is the field's label); `force title checkbox sets force_title`; `split and join keep a cell's title with its field`; `export downloads the preset as a reflbo file` (`{reflbo:'preset', version:1, preset}`, name `<id>.json`); `import adds a fresh id after the selected preset, in the cycle`; `import refused by the device adds nothing and says why` (preview answers 400); `import of a file that is no preset says so`; `import at 16 presets says so`.
- [ ] Run: FAIL. Implement: a title input (max 23 bytes, cut at a letter) and, for a split cell with a field, "Always show the title"; the Export button in the editor's actions; Import in the list's actions (a hidden file input). Run: PASS; ctest green.
- [ ] Commit `feat(web): a slot's title, a cell's forced title, a preset's export and import (T5 spec §7.5)`.

### Task 5: Docs, build, the board

**Files:** `AGENTS.md` (§10 the T5's notes; §2 status), `docs/guide.md` (Presets: titles, export and import), the spec (§7.5 as built, §11 T4c done when checked, r15).

- [ ] Both boards build clean. Flash the T5 (MAC check). Boot log clean; `version`.
- [ ] Owner: log in on the CDP Chrome window; on the Presets page give a split preset titles and force one; Save; screenshot over USB; the panel matches. Export it, import the file: a copy appears; Undo changes (or Save and delete the copy at the owner's word). Log out.
- [ ] Commit docs; tag at the owner's word.

### Task 6: Upstream

- [ ] A worktree of `~/reflbo` on `feat/titles-share` from its `main` (M6e is there: the cycle window in `ui_preset_t` and `presets.json`, the Presets page's window editor). Port Tasks 1–4; the RLCD's goldens stay byte-identical; the new `split_titles` golden for the RLCD. Snapshot within the RLCD's limit.
- [ ] Upstream's spec: a decision for DT14 (the next D number), §5.2/§5.4/§10.3 and §6's snapshot limit; AGENTS.md (§9 the decision; the status line); `docs/guide.md`.
- [ ] Host suite (RLCD and the T5 profile tests the fork has) and web tests green; RLCD build clean. No flash of the RLCD (DT8; its board is upstream's sessions').
- [ ] Push the branch; tell upstream's session (`reflbo-0e`) what it holds and what its merge touches (M6e's files).
