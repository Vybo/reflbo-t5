#include "rtcchip.h"

#include "pcf85063.h"

/* The RLCD board's PCF85063A (spec §7): rtcchip_* passes straight through. */

esp_err_t rtcchip_init(i2c_master_bus_handle_t bus)
{
    return pcf85063_init(bus);
}

esp_err_t rtcchip_read(time_t *utc, bool *valid)
{
    return pcf85063_read(utc, valid);
}

esp_err_t rtcchip_write(time_t utc)
{
    return pcf85063_write(utc);
}

esp_err_t rtcchip_set_alarm(time_t wake)
{
    return pcf85063_set_alarm(wake);
}

esp_err_t rtcchip_write_precise(int64_t *set_at_ms)
{
    return pcf85063_write_precise(set_at_ms);
}

esp_err_t rtcchip_error_ms(int64_t *error_ms)
{
    return pcf85063_error_ms(error_ms);
}

esp_err_t rtcchip_set_offset(int steps)
{
    return pcf85063_set_offset(steps);
}
