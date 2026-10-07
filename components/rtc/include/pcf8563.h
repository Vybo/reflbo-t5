#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

/*
 * PCF8563 real-time clock on the T5-ePaper-S3's I²C bus (T5 spec §8.1). Stores UTC. Its INT isn't
 * wired, so it only keeps time: no alarm, timer or CLKOUT. Call from the app task only.
 */

/* Configures the chip: the clock running, no interrupts, alarms, timer or CLKOUT. Keeps the time. */
esp_err_t pcf8563_init(i2c_master_bus_handle_t bus);
/* `valid` is false while the VL flag is set (the time may be wrong). */
esp_err_t pcf8563_read(time_t *utc, bool *valid);
/* Sets the time to the second and clears VL; fails if VL stays set (the oscillator doesn't run). */
esp_err_t pcf8563_write(time_t utc);
/* Sets the RTC to the system clock to the millisecond (spec §7, as pcf85063_write_precise()): STOP
 * holds the prescaler while the next second is written. Takes up to 1.6 s; `*set_at_ms` (may be NULL)
 * is that second, in UTC ms. */
esp_err_t pcf8563_write_precise(int64_t *set_at_ms);
/* How far the RTC is ahead of the system clock, in ms (negative: behind): waits up to 1.1 s for the
 * RTC's next second and times its start. */
esp_err_t pcf8563_error_ms(int64_t *error_ms);
