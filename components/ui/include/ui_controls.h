#pragma once

#include "ui_menu.h"

/* What a button's gesture does on a screen (upstream §5.6, T5 spec §8.3): one table per board, by the profile's
 * buttons. The app executes the action. */

typedef enum {
    UI_BTN_KEY,  /* the RLCD's KEY; the T5's Enter (IO34, KEY's pin) */
    UI_BTN_BOOT, /* the RLCD's BOOT; the T5's Up (IO35, BOOT's pin) */
    UI_BTN_DOWN, /* the T5's Down (IO39) */
    UI_BTN_BACK, /* the T5's Back (IO0) */
} ui_button_t;

typedef enum { UI_GESTURE_SHORT, UI_GESTURE_DOUBLE, UI_GESTURE_LONG } ui_gesture_t;

typedef enum {
    UI_SCREEN_DASHBOARD,
    UI_SCREEN_MENU,
    UI_SCREEN_CONFIG,
    UI_SCREEN_FIRST_RUN,
    UI_SCREEN_CRITICAL,
} ui_screen_t;

typedef enum {
    UI_DO_NOTHING,
    UI_DO_PRESET_NEXT,
    UI_DO_PRESET_PREV,
    UI_DO_CYCLE_TOGGLE,
    UI_DO_MENU_OPEN,
    UI_DO_REFRESH, /* the sensors, or on the Radar layout its loop or a sync for a frame */
    UI_DO_ALWAYS_TOGGLE,
    UI_DO_CONFIG_ENTER,
    UI_DO_MENU_KEY,
    UI_DO_CONFIG_KEY,
    UI_DO_CONFIG_EXIT,
    UI_DO_FIRST_RUN_CONTINUE,
    UI_DO_FIRST_RUN_MENU,
    UI_DO_RESAMPLE, /* the critical screen: a battery check */
} ui_do_t;

typedef struct {
    ui_do_t what;
    ui_menu_key_t key; /* for UI_DO_MENU_KEY */
} ui_control_t;

ui_control_t ui_controls(ui_screen_t screen, ui_button_t button, ui_gesture_t gesture);
