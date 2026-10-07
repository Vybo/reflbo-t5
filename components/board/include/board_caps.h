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
#else
#define BOARD_NAME               "rlcd42"
#define BOARD_HAS_ENV_SENSOR     1
#define BOARD_HAS_AUDIO          1
#define BOARD_HAS_RTC_TRIM       1
#define BOARD_HAS_RTC_PRECISE_SET 1
#define BOARD_HAS_RTC_CHIP       1
#define BOARD_HAS_RTC_ALARM_WAKE 1
#define BOARD_HAS_LPM_RATE       1
#endif
