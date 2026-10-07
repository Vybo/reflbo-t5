#include "rtcchip.h"

#include <sys/time.h>

#include "rtc_sysclock.h"

/* The LilyGo T5-4.7 has no RTC chip (DT10): rtcchip_* is the ESP32's system clock, so timekeeping.c works
 * on it unchanged (T5 spec §8.1). Writing is settimeofday(); there's no alarm, and the "precise set" is the
 * system clock's own. */

esp_err_t rtcchip_init(i2c_master_bus_handle_t bus)
{
    (void)bus;
    return ESP_OK;
}

esp_err_t rtcchip_read(time_t *utc, bool *valid)
{
    *utc = time(NULL);
    *valid = rtc_sysclock_valid(*utc);
    return ESP_OK;
}

esp_err_t rtcchip_write(time_t utc)
{
    struct timeval tv = { .tv_sec = utc };
    return settimeofday(&tv, NULL) == 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t rtcchip_set_alarm(time_t wake)
{
    (void)wake;
    return ESP_OK; /* nothing to arm: the ESP32's timer wakes the board (T5 spec §8.2) */
}

esp_err_t rtcchip_write_precise(int64_t *set_at_ms)
{
    /* timekeeping.c has set the system clock to the millisecond just before: that is the RTC here */
    if (set_at_ms != NULL) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        *set_at_ms = (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
    }
    return ESP_OK;
}

esp_err_t rtcchip_error_ms(int64_t *error_ms)
{
    *error_ms = 0; /* the RTC is the system clock */
    return ESP_OK;
}
