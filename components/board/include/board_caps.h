#pragma once

#include "sdkconfig.h"

/*
 * What each board has (T5 spec §4.3), at compile time: code for hardware a board lacks is left out
 * with #if. The UI's profile states the same through its UI_CAP_* bits (ui_profile.h), and main
 * checks that the two agree.
 */
#if CONFIG_REFLBO_BOARD_T5S3
#define BOARD_NAME               "t5s3"
#define BOARD_HAS_ENV_SENSOR     0 /* no SHTC3 */
#define BOARD_HAS_AUDIO          0
#define BOARD_HAS_RTC_TRIM       0 /* PCF8563: no Offset register */
#define BOARD_HAS_RTC_ALARM_WAKE 0 /* the PCF8563's INT isn't wired: the ESP32's timer wakes the board */
#define BOARD_HAS_LPM_RATE       0 /* e-paper: no refresh rate to set */
#else
#define BOARD_NAME               "rlcd42"
#define BOARD_HAS_ENV_SENSOR     1
#define BOARD_HAS_AUDIO          1
#define BOARD_HAS_RTC_TRIM       1
#define BOARD_HAS_RTC_ALARM_WAKE 1
#define BOARD_HAS_LPM_RATE       1
#endif
