#include "rtcchip.h"

/* T5-ePaper-S3, T0: no PCF8563 driver yet; T1 brings it (T5 spec §8.1). Reads fail, so the clock stays
 * invalid; there's no alarm to arm, as the chip's INT isn't wired. */

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
