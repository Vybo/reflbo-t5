#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "gesture.h"
#include "sdkconfig.h"

/*
 * KEY and BOOT buttons -> gestures (spec §5.6). Edges arrive by interrupt; a task times the
 * gestures and reports them through the callback, from that task.
 */

typedef enum {
    BOARD_BUTTON_KEY,  /* the T5's Enter (IO34) */
    BOARD_BUTTON_BOOT, /* the T5's Up (IO35) */
#if CONFIG_REFLBO_BOARD_T547
    BOARD_BUTTON_DOWN, /* IO39 (T5 spec §8.3, DT13) */
    BOARD_BUTTON_BACK, /* IO0, also the panel's strobe: read only between updates */
#endif
    BOARD_BUTTON_COUNT,
} board_button_t;

typedef void (*board_button_cb_t)(board_button_t button, gesture_t gesture);

esp_err_t board_buttons_start(board_button_cb_t cb, const gesture_config_t config[BOARD_BUTTON_COUNT]);
/* The `btn` console command: reports a gesture as if the button had made it. */
void board_buttons_inject(board_button_t button, gesture_t gesture);
/* A button woke the chip: time it from now if it is still held, else count a tap (spec §3.3). */
void board_buttons_woke(board_button_t button);
/* Re-reads both levels, e.g. after light sleep, when edge interrupts may have been missed. */
void board_buttons_resync(void);
/* A button held since before the last sleep (D16): no gestures until it has been released. */
void board_buttons_ignore_until_released(board_button_t button);
/* New gesture timings for the context (spec §5.6): the menu has no double press on KEY and a 1 s
 * BOOT long press. `config` must stay valid; it is applied on the buttons task. */
void board_buttons_set_config(const gesture_config_t config[BOARD_BUTTON_COUNT]);
/* True while a gesture is being timed or an edge is queued; sleeping then would lose it. */
bool board_buttons_busy(void);
bool board_buttons_pressed(board_button_t button);
/* The T5's Back is IO0, the panel's strobe while it updates (T5 spec §2.5 gotcha 2): the display pauses it around
 * each update and resumes it after; a held Back then starts a press. No-op on the RLCD. */
void board_buttons_back_pause(bool paused);
const char *board_button_name(board_button_t button);
const char *board_gesture_name(gesture_t gesture);
