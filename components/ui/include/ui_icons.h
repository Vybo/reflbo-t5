#pragma once

/* The UI's icons by name (T5 spec §7.1): the names of assets/icons/icons.txt, in its order. Each board's profile
 * maps a name and a size class to its own bitmap (ui_profile.h: ui_icon()). */
#define UI_ICON_LIST(X) \
    X(thermometer) X(drop) X(dew) X(bolt) X(stale) X(clock) \
    X(calendar) X(person) X(celebration) X(cloud) X(web) X(sync) \
    X(sync_failed) X(wifi) X(wifi_off) X(air) X(particles) X(uv) \
    X(pollen) X(forecast) X(solar) X(house) X(grid) X(self_use) \
    X(wx_clear_day) X(wx_clear_night) X(wx_mainly_day) X(wx_mainly_night) X(wx_partly_day) X(wx_partly_night) \
    X(wx_overcast) X(wx_fog) X(wx_drizzle) X(wx_rain) X(wx_freezing) X(wx_snow) \
    X(wx_showers_day) X(wx_showers_night) X(wx_snow_showers_day) X(wx_snow_showers_night) X(wx_thunder) X(wx_unknown) \
    X(sunrise) X(sunset)

typedef enum {
#define UI_ICON_ID(name) UI_ICON_##name,
    UI_ICON_LIST(UI_ICON_ID)
#undef UI_ICON_ID
    UI_ICON_COUNT,
} ui_icon_id_t;
