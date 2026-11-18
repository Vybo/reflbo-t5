# T5a and T5b: upstream's M6e and M7 in the fork — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bring upstream reflbo into the T5 fork in two steps: T5a to `60dccab` (M6e's cycle windows, D41; the solar gaps fix, r50) and T5b to `14593da` (M7's PC sensors, D44/D45; the GPL switch, D46; `stable-m7`), with the T5's app task on core 0 (DT17) and M7's visuals and PC layout drawn at 960×540.

**Architecture:** Two `--no-ff` merges on the branch `t5-merge`, each resolved so that upstream's logic lands unchanged and the fork's board seams (profile, `UI_PX()`, fonts by role, icon classes, `board_caps.h`) stay in charge of sizes and hardware. After T5b's merge, three fork commits: the cores (DT17), M7's drawing through the profile with the T5's PC layout, and the T5 goldens once the owner approves the renders.

**Tech Stack:** C17 on ESP-IDF v5.5.5 (ESP32 for the T5, ESP32-S3 for the RLCD), host tests with CMake/ctest, the page's tests with Node, git merges.

**Spec:** `docs/specs/2026-10-06-t5-board-design.md` (r19; Task 1 writes r20), upstream's `docs/specs/2026-09-25-firmware-design.md` (r56 at `14593da`).

## Global Constraints

- The RLCD's goldens stay byte-identical to upstream's at each merge target (TR2): `git diff --exit-code --diff-filter=MD <target> HEAD -- 'test/host/golden/*.pbm'` exits 0, and ctest's RLCD golden tests pass.
- One codebase: upstream's logic is taken as is; only sizes, fonts, icons, cores and missing hardware differ per board, through the profile and `board_caps.h` (TR3).
- Every UI size in shared code goes through `UI_PX()`, `UI_FONT(role)`, `ui_icon(id, class)`/`ui_icon_px()`; every UI line through `ui_hline`/`ui_vline`/`ui_frame`/`ui_ring` (UI_PX(1) wide: 2 px on the T5) (T5 spec §7.1, T3b).
- T5 renders need the owner's approval before their goldens are committed (DT2).
- Never flash the RLCD board from the fork (DT8). Flash the T5 only after the owner agrees in this session, after reading MAC `34:ab:95:5e:5d:58` on `/dev/cu.usbserial-52D60046741`, always with `-b 230400`. Never erase flash, NVS or the storage partition.
- Ask before changing the board's settings or presets; back them up before such a test and restore them after. Never reset or ask for the owner's web password; the owner logs in themselves in the CDP-driven Chrome.
- Commits: Conventional Commits, imperative, no attribution trailers of any kind.
- `NOTICE` keeps both Apache cut-offs: the fork's (`b7d0fb5`) and upstream's (`c1cf153`) (DT16, D46).
- Builds: `tools/idf.sh build` (RLCD, `build/`) and `REFLBO_BOARD=t5 tools/idf.sh build` (T5, `build-t5/`), both with no new warnings. Host: `cmake -S test/host -B build-host -G Ninja && cmake --build build-host && ctest --test-dir build-host --output-on-failure`. Page: `node --test test/web/test_app.mjs`.

## Review Focus

- Another task than the app task reading or writing `s_presets_snap` on the T5: on the ESP32 only the PRO CPU reaches RTC FAST memory, so any other core faults or reads garbage → Task 5's static assertion and its grep check.
- The presets after a deep-sleep wake on the T5 (RTC FAST powered through deep sleep): a warm wake keeps them without reading `presets.json` → Task 8, Step 4.
- Core 0's load with the app task beside Wi-Fi (priority 23) and `epd_prep[0]` (24): a button press in sync mode `always` with the radar on still updates the panel within 1 s → Task 8, Step 5.
- Internal RAM on the ESP32 in a PC session with the radar on: the lowest internal free heap is recorded; below 16 KB is a finding → Task 8, Step 6.
- A PC session's 1 s redraws on e-paper: fast updates each second, a clean one every `clean_after`-th, no ghost that outlives a clean → Task 8, Step 6 (the owner looks).

---

## Part A: T5a, upstream to `60dccab`

### Task 1: T5 spec r20, the design of T5a and T5b

**Files:**
- Modify: `docs/specs/2026-10-06-t5-board-design.md` (§1.2 DT17, new §7.7, §11, §12, §13, status line)
- Create: `docs/plans/2026-11-18-t5ab-upstream-merge.md` (this file)

- [ ] **Step 1: Write r20.** Add:
  - DT17 to §1.2: "Owner, 2026-10-29: on the T5 the app task runs on core 0, so M7's presets block stays in RTC FAST memory as upstream has it (on the ESP32 only the PRO CPU reaches it); sync, adsb and the place search move to core 1. Fork-only; the RLCD keeps the app on core 1."
  - §7.7 "The PC's sensors on the T5 (T5b)": the PC layout rescaled below the status bar, same slots and sizes as the RLCD's — c1 0, 35, 478×178; c2 482, 35, 478×178; c3 0, 217, 478×178; c4 482, 217, 478×178 (M); b1 0, 399, 317×141; b2 321, 399, 318×141; b3 643, 399, 317×141 (S); no lines; the visuals' fonts by role, their sizes and thresholds `UI_PX()` of the RLCD's, their lines `UI_PX(1)`; no new gray uses (the owner may ask at the renders); "PC asleep" centred as on the RLCD; the panel's frame time for D44's warning is the fast update's (about 0.4 s, T4); clean refreshes follow the setting (every 60th by default, a minute at 1 s).
  - §11: rows T5a ("Upstream to `60dccab`: M6e's cycle windows (D41), the solar gaps fix" | "Both boards build; RLCD goldens identical; a window set from the console closes on the T5 and the cycle skips its preset") and T5b ("Upstream to `14593da`: M7 (D44, D45), GPL (D46); DT17's cores; M7's visuals and PC layout at 960×540" | "The owner approves the renders; a PC session from the app's demo on the panel; presets kept through deep sleep; core 0's load checked").
  - §12: the T5's internal RAM in a PC session; e-paper at a PC session's rate.
  - §13: `| r20 | 2026-11-18 | T5a and T5b: upstream's M6e and M7 in the fork; DT17, the app task on core 0; the PC's sensors on the T5 (§1.2, §7.7, §11, §12) |`; the status line gains "r20 designs T5a and T5b (DT17, §7.7)".

- [ ] **Step 2: Commit**

```bash
git switch -c t5-merge
git add docs/specs/2026-10-06-t5-board-design.md docs/plans/2026-11-18-t5ab-upstream-merge.md
git commit -m "docs: T5a and T5b, upstream's M6e and M7 in the fork (T5 spec r20, DT17)"
```

### Task 2: Merge upstream to `60dccab`

**Files:** the 29 conflicting files of `git merge-tree --write-tree --name-only refs/heads/main 60dccab`, and whatever auto-merges.

**Interfaces:**
- Produces: a merge commit whose tree builds for both boards; `SNAP_VERSION` 15 (upstream's, new to the fork: the T4d image used 14).

- [ ] **Step 1: Merge**

Run: `git merge --no-ff --no-commit 60dccab`
Expected: `Automatic merge failed; fix conflicts and then commit the result.` with the 29 files listed.

- [ ] **Step 2: Resolve by these rules**
  - D42/D43 code both sides carry (`ui_status.c`, `ui_widget.c`, `ui_dashboard.c`, `ui_preset*.c`, `ui_solar.c`, `ui_forecast.c`, `ui_internal.h`, `ui_dashboard.h`, `ui_fields.h`, `ui_preset.h`, `components/ui/CMakeLists.txt`, the D42/D43 tests and fixtures, `web/app.js`, `test/web/test_app.mjs`): take the fork's side (profile-aware), then apply on top whatever upstream changed after `a32cfa2` (`git diff a32cfa2 60dccab -- <file>`), sizes through the profile.
  - M6e and the solar fix (`ui_window.c`, `solar.*`, `sync.c`, `app_solar.c`, `test_ui_window.c`, `test_solar.c`, `UI_PRESET_ERR_LEN`): upstream's side.
  - `main/app*.c`, `app_internal.h`, `app_cmds.c`, `app_sync.c`: union; the fork's board seams (`#if BOARD_…`, epaper calls, `APP_CORE`) stay; `SNAP_VERSION` 15 with upstream's comment.
  - `docs/specs/2026-09-25-firmware-design.md`: upstream's text, keeping the fork's D8/D46 lines (DT16 carries them ahead of upstream).
  - `AGENTS.md`, `README.md`, `docs/guide.md`: upstream's text plus the fork's T5 additions, none lost (`git diff a32cfa2 refs/heads/main -- <file>` lists them).
  - Generated sources (`components/gfx/icons/*`, `components/gfx/fonts/*`): never hand-merged; run `tools/gen_icons.sh` / `tools/gen_fonts.sh` if their inputs changed, and check the RLCD's arrays equal upstream's.

Run: `git diff --name-only --diff-filter=U`
Expected: empty.

- [ ] **Step 3: Host tests and the RLCD goldens**

Run: `cmake -S test/host -B build-host -G Ninja && cmake --build build-host && ctest --test-dir build-host --output-on-failure > build-host/ctest.log 2>&1; tail -3 build-host/ctest.log; git diff --exit-code --diff-filter=MD 60dccab -- 'test/host/golden/*.pbm'`
Expected: `100% tests passed`; the diff exits 0. A T5 golden that now differs is a merge fault to find, not a golden to rewrite (M6e draws nothing new).

- [ ] **Step 4: Page tests and both builds**

Run: `node --test test/web/test_app.mjs && tools/idf.sh build 2>&1 | tail -3 && REFLBO_BOARD=t5 tools/idf.sh build 2>&1 | tail -3`
Expected: page tests pass; both builds end `Project build complete`, with no warning outside managed components.

- [ ] **Step 5: Commit**

```bash
git commit -m "chore: merge upstream to 60dccab, M6e's cycle windows and the solar fix (T5a)"
```

### Task 3: T5a at the board, as built, tag (board stop)

Runs when the owner is at the board; Tasks 4–7 may run first on the host.

- [ ] **Step 1: Ask the owner to flash the T5.** On yes: read the MAC (`tools/idf.sh exec python -m esptool --chip esp32 -p /dev/cu.usbserial-52D60046741 -b 230400 read_mac` → `34:ab:95:5e:5d:58`), then `REFLBO_BOARD=t5 tools/idf.sh -p /dev/cu.usbserial-52D60046741 -b 230400 flash`, boot log to `captures/t5/boot-t5a.log`; `version` shows the build's ELF hash.
- [ ] **Step 2: The window.** One devlog session: `preset list` (save it), `preset window <a cycled preset> <now+2h HH:MM> <now+3h HH:MM>`, `preset list`, `preset window <id> off`, `preset list`.
  Expected: the preset shows its window closed and out of the cycle; after `off` the list equals the first one.
- [ ] **Step 3: Docs as built.** T5 spec r21 (§11 T5a done), `AGENTS.md` (T5a done, the board check). Commit `docs: T5a done, M6e on the T5 (T5 spec r21)`.
- [ ] **Step 4: Main and the tag.** `git switch main && git merge --ff-only <T5a's docs commit>`, `git tag -a t5-stable-t5a -m "T5a: upstream's M6e in the fork"`, `git push origin main t5-stable-t5a`, `git switch t5-merge`.

---

## Part B: T5b, upstream to `14593da`

### Task 4: Merge upstream to `14593da`

**Interfaces:**
- Consumes: Task 2's merge.
- Produces: a merge commit that builds for both boards with upstream's M7 drawing (RLCD sizes on the T5 until Task 6); `SNAP_VERSION` 17, `PRESETS_VERSION` 1; `s_presets_snap` in RTC FAST. Not flashed before Task 5.

- [ ] **Step 1: Merge.** `git merge --no-ff --no-commit 14593da`. Expected: conflicts listed.
- [ ] **Step 2: Resolve by Task 2's rules, and:**
  - New M7 files (`ui_visual.c`, `ui_pc.c`, `ui_history.c`, `app_pc.c`, `pcstats/`, their tests and goldens): upstream's, untouched here.
  - Files where M7 meets the fork's profile work (`ui_catalog.c`, `ui_layout.c`, `ui_status.c`, `ui_widget.c`, `ui_dashboard.c`, `ui_menu.c`, `app_menu.c`): upstream's logic, the fork's sizes; `ui_profile_t547.c`'s layout table gets `[UI_LAYOUT_PC]` with NULL slots for now (Task 6 fills it).
  - `docs/specs/2026-09-25-firmware-design.md`: upstream's, whole. Check: `git diff --exit-code 14593da -- docs/specs/2026-09-25-firmware-design.md`.
  - `NOTICE`: upstream's text with "Versions up to commit c1cf153 of upstream reflbo, and up to b7d0fb5 of this fork, were released under the Apache License, Version 2.0, and remain available under its terms."
  - `dependencies.lock`: upstream's, then `REFLBO_BOARD=t5 tools/idf.sh reconfigure` and keep what it writes for the T5 if it differs only in the target's lines.
- [ ] **Step 3: Host tests, the RLCD goldens, the page tests, both builds** — as Task 2's Steps 3–4 with `14593da`. Expected: all pass; the T5 build links with `s_presets_snap` in `.rtc.force_fast` (`grep s_presets_snap build-t5/reflbo.map` shows an `0x3ff8…` address).
- [ ] **Step 4: Commit** `chore: merge upstream to 14593da, M7's PC sensors and the GPL (T5b)`.

### Task 5: The app task on core 0 on the T5 (DT17)

**Files:**
- Modify: `components/board/include/board_caps.h`, `main/app.c:47`, `components/sync/sync.c:675`, `components/adsb/adsb_task.c:151`, `components/webui/webui.c:629`, the three components' `CMakeLists.txt` (`PRIV_REQUIRES board`)

**Interfaces:**
- Produces: `BOARD_APP_CORE` (T5 0, RLCD 1) and `BOARD_NET_CORE` (T5 1, RLCD 0) in `board_caps.h`.

- [ ] **Step 1: The failing guard.** In `main/app.c`, beside `s_presets_snap`:

```c
#if CONFIG_IDF_TARGET_ESP32 && !CONFIG_FREERTOS_UNICORE
_Static_assert(APP_CORE == 0, "on the ESP32 only the PRO CPU reaches RTC FAST memory, where the presets sleep (T5 DT17)");
#endif
```

Run: `REFLBO_BOARD=t5 tools/idf.sh build 2>&1 | grep -m1 'static assertion failed'`
Expected: the assertion fails (`APP_CORE` is 1).
- [ ] **Step 2: Add the two macros** to both branches of `board_caps.h` with one comment each (DT17), set `#define APP_CORE BOARD_APP_CORE`, and pass `BOARD_NET_CORE` in place of the literal `0` at the three task creations.
- [ ] **Step 3: Verify.** Both builds pass; `git grep -n s_presets_snap -- main components` lists `main/app.c` only, and every use inside `boot()` or `prepare_deep_sleep()`, which run on the app task.
- [ ] **Step 4: Commit** `feat(board): the T5's app task on core 0, its network tasks on core 1 (T5 spec DT17)`.

### Task 6: M7's drawing through the profile, the T5's PC layout (render stop)

**Files:**
- Modify: `components/ui/ui_visual.c`, `components/ui/ui_pc.c`, `components/ui/ui_status.c` (M7's marks), `components/ui/ui_profile_t547.c` (`k_pc`), `assets/icons/icons.txt` → `tools/gen_icons.sh` if M7's icons lack T5 classes, `test/host/CMakeLists.txt` and the T5 golden lists for the M7 fixtures (`vis_*`, `dash_pc_*`, `dash_*_visuals`)
- Test: `test/host/test_ui_visual_golden.c`, `test/host/test_ui_dashboard_golden.c` (their T5 halves), `test/host/test_ui_pc_layout.c`

- [ ] **Step 1: The failing tests.** Add the M7 fixtures to the T5 golden lists, and in `test_ui_pc_layout.c` a T5 case: `t5_pc_layout_slots` asserting the seven rects of T5 spec §7.7 and each slot inside {0, 35, 960, 505}.
Run: ctest. Expected: the new T5 cases fail (no T5 goldens, no T5 PC slots).
- [ ] **Step 2: Profile the drawing.** In `ui_visual.c` and `ui_pc.c`: `k_faces` and every `&gfx_font_*` become `UI_FONT(role)` (num_cb_48 → `UI_F_NUM_48`, sans_bold_28 → `UI_F_BOLD_28`, …); every pixel literal and threshold (`ui_visual_fits()`'s 80/60/40/16, insets, bar widths, ring sizes, the asleep box) becomes `UI_PX(n)` or fb-relative so the RLCD's values are unchanged (asleep box: `x = (fb->width - UI_PX(200)) / 2`, `y = (fb->height - UI_PX(60)) / 2 + UI_PX(20)`); lines through `ui_hline`/`ui_vline`/`ui_frame`. Fill `k_pc` in `ui_profile_t547.c` from §7.7.
- [ ] **Step 3: The page.** If `web/app.js` decides which visuals a cell offers from pixel sizes, scale them by the panel's size from `/api/layouts`, with a test in `test/web/test_app.mjs` (`show-as offers by the T5 cell's size`) against a 960×540 fake device; if the board decides, no change.
- [ ] **Step 4: Render for the owner.** Run the host build, `python3 tools/render.py`, and write the T5 goldens to a scratch dir only; show the owner the M7 renders (`vis_*`, `dash_pc_*`, `dash_*_visuals`) as PNGs. RLCD goldens: ctest passes and the `--diff-filter=MD` check exits 0.
- [ ] **Step 5: Commit the code (goldens wait).** `feat(ui): M7's visuals and PC layout at the T5's size (T5 spec §7.7)`.

### Task 7: The T5 goldens (after the owner's approval)

- [ ] **Step 1:** Apply the owner's changes, if any, re-render, and get their yes.
- [ ] **Step 2:** Write the approved T5 goldens into `test/host/golden/t5/`; ctest passes in full.
- [ ] **Step 3: Commit** `test: the T5's M7 goldens, the owner's look done (T5 spec §7.7)`.

### Task 8: T5b at the board (board stop)

- [ ] **Step 1: Ask the owner** to flash the T5 and, for Step 6, to log in in the CDP Chrome. Flash as Task 3 Step 1; boot log `captures/t5/boot-t5b.log`.
- [ ] **Step 2: Back up** the board's config (`GET /api/backup`, the owner logged in) before Step 6; restore and compare after.
- [ ] **Step 3: Cores.** `tasks`: `app` on core 0; `sync`, `adsb` (in sync mode `always`), `geocode` (after a place search) on core 1.
- [ ] **Step 4: Presets across deep sleep.** `preset list`, `sleep test deep 2`, `preset list`. Expected: identical lists, no `presets.json` read logged on the wakes.
- [ ] **Step 5: Core 0's load.** With the owner's OK, sync mode `always` and the Rain preset: `tasks` twice 10 s apart; `btn up short` and the log's update within 1 s.
- [ ] **Step 6: A PC session.** The PC app's demo on this Mac (`go build -o "$TMPDIR/rs/reflbo-stats" ./cmd/reflbo-stats` in `~/reflbo-stats-win`, run `-demo -no-tray`; the owner allows it in the firewall if asked, upstream gotcha 45); the PC set up on the board's PC page by the owner; `sync now`; expected: `pc status` shows the session, the PC preset on the panel with charts moving each second, `heap` internal minimum recorded; the owner watches a minute for ghosts and the clean; stop the demo, expected: the session ends within 15 s.
- [ ] **Step 7: Restore** the backup, compare, log out (`POST /api/auth/logout` with `Content-Type: application/json`).

### Task 9: As built, the final review, the tag

- [ ] **Step 1:** T5 spec r22 (§7.7 as built, §11 T5b done, the board check's numbers), `AGENTS.md`, `README.md`/`docs/guide.md`'s T5 notes; commit `docs: T5b done, M7 on the T5 (T5 spec r22)`.
- [ ] **Step 2:** The final review of `t5-merge` (the merges with `git show --remerge-diff`, the fork commits as diffs) by a fresh reviewer on the most capable model; one fix pass, each fix RED→GREEN.
- [ ] **Step 3:** `main` fast-forwards to `t5-merge`; tag `t5-stable-t5b` ("T5b: upstream's M7 in the fork"); `git push origin main t5-stable-t5b`; delete `t5-merge`.
