#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "board_caps.h"
#include "driver/i2c_master.h"
#include "esp_err.h"

/*
 * The board's RTC chip (T5 spec §4.5): the PCF85063A on the RLCD board (pcf85063.h), the PCF8563 on
 * the T5. Stores UTC. What a chip does beyond keeping time is in board_caps.h:
 * BOARD_HAS_RTC_ALARM_WAKE and BOARD_HAS_RTC_TRIM. Call from the app task only.
 */

/* Configures the chip and keeps its time. */
esp_err_t rtcchip_init(i2c_master_bus_handle_t bus);
/* `valid` is false while the chip says its oscillator stopped (the time was lost). */
esp_err_t rtcchip_read(time_t *utc, bool *valid);
/* Sets the time to the second and clears the oscillator flag. */
esp_err_t rtcchip_write(time_t utc);
/* Arms the alarm for `wake` (a whole minute) and clears a pending one. Without
 * BOARD_HAS_RTC_ALARM_WAKE there is nothing to arm: ESP_OK, and the ESP32's timer wakes the board. */
esp_err_t rtcchip_set_alarm(time_t wake);

#if BOARD_HAS_RTC_PRECISE_SET
/* The precise set (STOP holds the prescaler while the next second is written, spec §7) and the RTC's
 * error against the system clock, timed at its next second. */
esp_err_t rtcchip_write_precise(int64_t *set_at_ms);
esp_err_t rtcchip_error_ms(int64_t *error_ms);
#endif

#if BOARD_HAS_RTC_TRIM
/* The PCF85063's Offset register (spec §7, D25; pcf85063.h). */
esp_err_t rtcchip_set_offset(int steps);
#endif
