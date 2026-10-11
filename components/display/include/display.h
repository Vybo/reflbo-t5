#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "display_board.h"
#include "esp_err.h"
#include "gfx.h"

/* Display service (spec §4.1, T5 spec §4.4): owns the canonical framebuffer (in PSRAM) and puts it on
 * the board's panel. The API is the same on every board; display_board.h holds what isn't.
 *
 * Not thread-safe: the display and its panel belong to the app task (spec §3.2). Console commands
 * that draw or push run there through the diag executor. */

esp_err_t display_init(void); /* cold start: white frame, then idle */
/* Deep-sleep wake: the panel still shows the last frame; attach without reset or clear. */
esp_err_t display_init_warm(const display_state_t *state);
/* Deep-sleep wake without a valid snapshot: the panel kept running and may have slept for the night,
 * so attach without a reset and wake it. A failed wake is logged, not returned. */
esp_err_t display_init_lost(void);
void display_export(display_state_t *out);
/* Holds the panel's pins for deep sleep. The last display call before sleeping. */
esp_err_t display_prepare_deep_sleep(void);
void display_cancel_deep_sleep(void); /* after display_prepare_deep_sleep() failed */
gfx_fb_t *display_fb(void); /* NULL until display_init has allocated the framebuffer */
/* The image `screenshot` and the web's screenshot show: the frame as the panel last got it. The RLCD's is
 * display_fb(); the T5's is its 960×540 4 bpp panel frame (T5 spec §6.5). NULL before display_init. */
const gfx_fb_t *display_screenshot_fb(void);
/* Night sleep (spec §9.1). On the RLCD the panel stops scanning and its image fades; waking pushes the
 * frame again. */
esp_err_t display_sleep(void);
esp_err_t display_wake(void);
bool display_asleep(void);
/* Puts the framebuffer on the panel if it changed since the last commit (CRC32), or always when force
 * is set. */
esp_err_t display_commit(bool force);
/* Fast updates for interaction (the menu, config mode, the radar's loop): HPM on the RLCD; on the T5,
 * fast updates left out of the clean refresh's count (T5 spec §5.3). */
esp_err_t display_set_fast(bool fast);
/* A clean refresh (T5 spec §5.3): nothing to do on the RLCD. */
esp_err_t display_clean(void);
/* T5 spec §5.3: the next commit is clean (a preset switch, the end of night, a night peek). No-op on the RLCD. */
void display_clean_next(void);
/* Every Nth update clean, from display.clean_after (T5 spec §5.3). No-op on the RLCD. */
void display_set_clean_after(uint8_t n);
/* After a warm wake (T5 spec §5.5): true while the panel's frame isn't known but the snapshot says what it showed;
 * always false on the RLCD. */
bool display_wants_shown(void);
/* The frame in display_fb() is the one the panel shows if its CRC matches the snapshot's: true then, and fast
 * updates go on from it; false, and the next update is clean. True on the RLCD. */
bool display_adopt_shown(void);
