#include "rtcchip.h"

#include "pcf8563.h"

/* The T5-ePaper-S3's PCF8563 (T5 spec §8.1): rtcchip_* passes straight through. Its INT isn't wired, so
 * there is no alarm to arm: the ESP32's timer wakes the board (T5 spec §8.2). */

esp_err_t rtcchip_init(i2c_master_bus_handle_t bus)
{
    return pcf8563_init(bus);
}

esp_err_t rtcchip_read(time_t *utc, bool *valid)
{
    return pcf8563_read(utc, valid);
}

esp_err_t rtcchip_write(time_t utc)
{
    return pcf8563_write(utc);
}

esp_err_t rtcchip_set_alarm(time_t wake)
{
    (void)wake;
    return ESP_OK;
}

esp_err_t rtcchip_write_precise(int64_t *set_at_ms)
{
    return pcf8563_write_precise(set_at_ms);
}

esp_err_t rtcchip_error_ms(int64_t *error_ms)
{
    return pcf8563_error_ms(error_ms);
}
