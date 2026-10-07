#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/*
 * PCF8563 register codec (NXP datasheet Rev. 11.1, §8): the time registers <-> UTC seconds. The RTC
 * stores UTC (spec §7). Pure C, host-buildable; the I²C transfers live in pcf8563.c (T5 spec §8.1).
 */

#define PCF8563_ADDR              0x51
#define PCF8563_REG_CONTROL_1     0x00
#define PCF8563_REG_CONTROL_2     0x01
#define PCF8563_REG_SECONDS       0x02 /* 02h-08h: VL_seconds .. years, read and written in one transfer (§8.5) */
#define PCF8563_REG_ALARM         0x09 /* 09h-0Ch: minute, hour, day and weekday alarms */
#define PCF8563_REG_CLKOUT        0x0D
#define PCF8563_REG_TIMER_CONTROL 0x0E

#define PCF8563_TIME_LEN  7
#define PCF8563_ALARM_LEN 4

/* Control_status_1: normal mode, the clock running, the POR override off (Table 5). */
#define PCF8563_CONTROL_1_RUN 0x00
/* STOP: the prescaler's F2-F14 are held in reset, so the time can be written exactly (§8.10). The first
 * tick comes 0.507813-0.507935 s after the release (Table 26); this is the middle. */
#define PCF8563_CONTROL_1_STOP     0x20
#define PCF8563_STOP_FIRST_TICK_US 507874
/* Control_status_2: no interrupts, AF and TF cleared. The T5 doesn't wire INT (T5 spec §2.1). */
#define PCF8563_CONTROL_2_OFF 0x00
/* An alarm register with its AE bit set: that alarm is off (§8.6). */
#define PCF8563_ALARM_OFF 0x80
/* CLKOUT off (FE = 0): it runs at 32.768 kHz after power-on, from the backup cell too (§8.7). */
#define PCF8563_CLKOUT_OFF 0x00
/* The timer off, its source at 1/60 Hz, the setting that draws least (§8.8.1). */
#define PCF8563_TIMER_OFF 0x03

/* 2000-01-01T00:00:00Z .. 2099-12-31T23:59:59Z */
#define PCF8563_MIN_TIME ((time_t)946684800)
#define PCF8563_MAX_TIME ((time_t)4102444799)

/* Decodes 02h-08h. `low` reports the VL flag: the supply dropped or the oscillator stopped since the
 * time was set, so it may be wrong. The century bit is ignored. False if a register holds an impossible
 * value. */
bool pcf8563_decode_time(const uint8_t regs[PCF8563_TIME_LEN], time_t *utc, bool *low);
/* Encodes 02h-08h with VL and the century bit cleared. `utc` is clamped to the RTC's range. */
void pcf8563_encode_time(time_t utc, uint8_t regs[PCF8563_TIME_LEN]);
