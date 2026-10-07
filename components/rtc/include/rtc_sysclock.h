#pragma once

#include <stdbool.h>
#include <time.h>

/*
 * The T5-4.7 has no RTC chip (DT10): its time is the ESP32's system clock, which counts through deep sleep
 * and software resets and starts at 1970 after a power-on or an EN reset (T5 spec §8.1). Pure C,
 * host-buildable.
 */

#define RTC_SYSCLOCK_MIN_VALID ((time_t)1767225600) /* 2026-01-01T00:00:00Z */

/* Whether the system clock holds a real time: one a sync or a manual set gave it, not a count from power-on. */
bool rtc_sysclock_valid(time_t utc);
