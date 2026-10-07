#include "rtcchip.h"

/* LilyGo T5-4.7: a stub until T1's Task 4 makes it the ESP32's system clock (DT10, T5 spec §8.1). Reads
 * fail, so the clock stays invalid; there's no alarm to arm. */

esp_err_t rtcchip_init(i2c_master_bus_handle_t bus)
{
    (void)bus;
    return ESP_OK;
}

esp_err_t rtcchip_read(time_t *utc, bool *valid)
{
    *utc = 0;
    *valid = false;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t rtcchip_write(time_t utc)
{
    (void)utc;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t rtcchip_set_alarm(time_t wake)
{
    (void)wake;
    return ESP_OK; /* nothing to arm: the ESP32's timer wakes the board (T5 spec §8.2) */
}

esp_err_t rtcchip_write_precise(int64_t *set_at_ms)
{
    (void)set_at_ms;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t rtcchip_error_ms(int64_t *error_ms)
{
    *error_ms = 0;
    return ESP_ERR_NOT_SUPPORTED;
}
