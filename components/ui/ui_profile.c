#include "ui_profile.h"

#include <stddef.h>

const ui_profile_t ui_profile_rlcd42 = { "rlcd42", 400, 300, 20, UI_CAPS_RLCD42 };
/* T0-T2: the RLCD's geometry until T3 (T5 spec §11); only the board and its capabilities differ. */
const ui_profile_t ui_profile_t547 = { "t547", 400, 300, 20, UI_CAPS_T547 };

static const ui_profile_t *s_profile = &ui_profile_rlcd42;

const ui_profile_t *ui_profile(void)
{
    return s_profile;
}

void ui_profile_use(const ui_profile_t *profile)
{
    s_profile = profile != NULL ? profile : &ui_profile_rlcd42;
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
