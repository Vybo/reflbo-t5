#pragma once

#include <stdint.h>

/*
 * The board as the UI sees it (T5 spec §7.1): the panel's size, the status bar, the board's name and
 * what it has. Pure C, host-buildable. T0 gives both boards the RLCD's geometry; T3 gives the T5 its
 * own (960×540) and moves the layouts, fonts and screen geometry in here.
 */

/* UI_CAP_* bits: hardware a board has, shown by the UI and the web page only where it exists. main
 * checks them against board_caps.h's BOARD_HAS_*. */
#define UI_CAP_ENV_SENSOR     (1u << 0) /* temperature and humidity (env.*) */
#define UI_CAP_AUDIO          (1u << 1)
#define UI_CAP_RTC_TRIM       (1u << 2)
#define UI_CAP_RTC_ALARM_WAKE (1u << 3)
#define UI_CAP_LPM_RATE       (1u << 4) /* the panel's refresh rate setting */

#define UI_CAPS_RLCD42 (UI_CAP_ENV_SENSOR | UI_CAP_AUDIO | UI_CAP_RTC_TRIM | UI_CAP_RTC_ALARM_WAKE | UI_CAP_LPM_RATE)
#define UI_CAPS_T547   0u

typedef struct {
    const char *board; /* "rlcd42", "t547": as BOARD_NAME (board_caps.h) */
    int16_t width, height;
    int16_t status_h; /* the status bar; its line is the row below it */
    uint32_t caps;    /* UI_CAP_* */
} ui_profile_t;

extern const ui_profile_t ui_profile_rlcd42;
extern const ui_profile_t ui_profile_t547;

/* The profile in use: ui_profile_rlcd42 until ui_profile_use(). */
const ui_profile_t *ui_profile(void);
/* Sets the profile in use; NULL restores the RLCD's. The firmware calls it once at boot. */
void ui_profile_use(const ui_profile_t *profile);
/* "env_sensor", "audio", "rtc_trim", "rtc_alarm_wake", "lpm_rate"; NULL for anything but one known bit. */
const char *ui_cap_name(uint32_t cap);
