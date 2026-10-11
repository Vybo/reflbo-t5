#pragma once

#include "sdkconfig.h"

/*
 * What each board has (T5 spec §4.3), at compile time: code for hardware a board lacks is left out
 * with #if. The UI's profile states the same through its UI_CAP_* bits (ui_profile.h), and main
 * checks that the two agree.
 */
#if CONFIG_REFLBO_BOARD_T547
#define BOARD_NAME               "t547"
#define BOARD_HAS_ENV_SENSOR     0 /* no SHTC3 */
#define BOARD_HAS_AUDIO          0
#define BOARD_HAS_RTC_TRIM       0 /* no RTC chip (DT10) */
#define BOARD_HAS_RTC_PRECISE_SET 1 /* the system clock is the RTC (T5 spec §8.1) */
#define BOARD_HAS_RTC_CHIP       0 /* the ESP32's own clock keeps time (DT10) */
#define BOARD_HAS_RTC_ALARM_WAKE 0 /* no RTC: the ESP32's timer wakes the board */
#define BOARD_HAS_LPM_RATE       0 /* e-paper: no refresh rate to set */
#define BOARD_IS_EPAPER          1 /* the panel keeps its image unpowered; fast and clean updates (T4) */
#define BOARD_BUTTON_FOLLOW_MS   5000 /* awake after a press: Down and Back can't wake the board (DT13; 5 s, the owner) */
#define BOARD_RADAR_RV_KB        768 /* RainViewer's frames held and the one coming: 4 MB of PSRAM mapped (T3c) */
#else
#define BOARD_NAME               "rlcd42"
#define BOARD_HAS_ENV_SENSOR     1
#define BOARD_HAS_AUDIO          1
#define BOARD_HAS_RTC_TRIM       1
#define BOARD_HAS_RTC_PRECISE_SET 1
#define BOARD_HAS_RTC_CHIP       1
#define BOARD_HAS_RTC_ALARM_WAKE 1
#define BOARD_HAS_LPM_RATE       1
#define BOARD_IS_EPAPER          0
#define BOARD_BUTTON_FOLLOW_MS   0
#define BOARD_RADAR_RV_KB        2048 /* RainViewer's frames held and the one coming: its hour fits 8 MB of PSRAM */
#endif
