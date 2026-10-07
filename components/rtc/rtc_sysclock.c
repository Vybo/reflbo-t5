#include "rtc_sysclock.h"

bool rtc_sysclock_valid(time_t utc)
{
    return utc >= RTC_SYSCLOCK_MIN_VALID;
}
