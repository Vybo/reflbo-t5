#include "pcf8563_regs.h"

#include "util_time.h"

#define VL_BIT 0x80

static uint8_t to_bcd(int v)
{
    return (uint8_t)(((v / 10) << 4) | (v % 10));
}

static int from_bcd(uint8_t b, uint8_t mask, int max)
{
    b &= mask;
    int hi = b >> 4;
    int lo = b & 0x0F;
    if (lo > 9) {
        return -1;
    }
    int v = hi * 10 + lo;
    return v > max ? -1 : v;
}

static int days_in_month(int y, int m)
{
    static const int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    bool leap = y % 4 == 0; /* 2000..2099: every fourth year, 2000 included */
    return m == 2 && leap ? 29 : days[m - 1];
}

bool pcf8563_decode_time(const uint8_t regs[PCF8563_TIME_LEN], time_t *utc, bool *low)
{
    int sec = from_bcd(regs[0], 0x7F, 59);
    int min = from_bcd(regs[1], 0x7F, 59);
    int hour = from_bcd(regs[2], 0x3F, 23);
    int day = from_bcd(regs[3], 0x3F, 31);
    int month = from_bcd(regs[5], 0x1F, 12); /* bit 7 is the century, which 2000-2099 doesn't need */
    int year = from_bcd(regs[6], 0xFF, 99);
    if (sec < 0 || min < 0 || hour < 0 || day < 1 || month < 1 || year < 0 ||
        day > days_in_month(2000 + year, month)) {
        return false;
    }
    *low = (regs[0] & VL_BIT) != 0;
    int64_t days = util_days_from_civil(2000 + year, month, day);
    *utc = (time_t)(((days * 24 + hour) * 60 + min) * 60 + sec);
    return true;
}

void pcf8563_encode_time(time_t utc, uint8_t regs[PCF8563_TIME_LEN])
{
    if (utc < PCF8563_MIN_TIME) {
        utc = PCF8563_MIN_TIME;
    } else if (utc > PCF8563_MAX_TIME) {
        utc = PCF8563_MAX_TIME;
    }
    int64_t days = utc / 86400;
    int secs = (int)(utc % 86400);
    int y, m, d;
    util_civil_from_days(days, &y, &m, &d);
    regs[0] = to_bcd(secs % 60); /* VL = 0 */
    regs[1] = to_bcd(secs / 60 % 60);
    regs[2] = to_bcd(secs / 3600);
    regs[3] = to_bcd(d);
    regs[4] = (uint8_t)((days + 4) % 7); /* 1970-01-01 was a Thursday; 0 = Sunday */
    regs[5] = to_bcd(m);                 /* C = 0 */
    regs[6] = to_bcd(y - 2000);
}
