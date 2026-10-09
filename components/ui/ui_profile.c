#include "ui_profile.h"

#include <stddef.h>

/* The instances live in ui_profile_rlcd42.c and ui_profile_t547.c, one built into each board's image; the
 * component's CMake names the image's as UI_PROFILE_DEFAULT. The host builds both and defaults to the RLCD's. */
#ifndef UI_PROFILE_DEFAULT
#define UI_PROFILE_DEFAULT ui_profile_rlcd42
#endif

static const ui_profile_t *s_profile = &UI_PROFILE_DEFAULT;

const ui_profile_t *ui_profile(void)
{
    return s_profile;
}

void ui_profile_use(const ui_profile_t *profile)
{
    s_profile = profile != NULL ? profile : &UI_PROFILE_DEFAULT;
}

const char *ui_cap_name(uint32_t cap)
{
    switch (cap) {
    case UI_CAP_ENV_SENSOR:
        return "env_sensor";
    case UI_CAP_AUDIO:
        return "audio";
    case UI_CAP_RTC_TRIM:
        return "rtc_trim";
    case UI_CAP_RTC_ALARM_WAKE:
        return "rtc_alarm_wake";
    case UI_CAP_LPM_RATE:
        return "lpm_rate";
    default:
        return NULL;
    }
}

int ui_px(int n)
{
    const ui_profile_t *p = ui_profile();
    if (p->px_num == p->px_den) {
        return n;
    }
    int num = n * p->px_num, den = p->px_den;
    return num >= 0 ? (num + den / 2) / den : -((-num + den / 2) / den);
}

const gfx_bitmap_t *ui_icon(ui_icon_id_t id, ui_icon_class_t cls)
{
    return (unsigned)id < UI_ICON_COUNT && (unsigned)cls < UI_IC_CLASSES ? ui_profile()->icons[id][cls] : NULL;
}

ui_icon_class_t ui_icon_class(int rlcd_px)
{
    return rlcd_px >= 48 ? UI_IC48 : rlcd_px >= 24 ? UI_IC24 : UI_IC16;
}

int ui_icon_px(int rlcd_px)
{
    return ui_profile()->icon_px[ui_icon_class(rlcd_px)];
}
