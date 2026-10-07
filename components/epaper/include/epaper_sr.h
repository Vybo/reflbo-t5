#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
 * The LilyGo T5-4.7's 74HCT4094 (T5 spec §2.3): eight control bits for the panel and its rails, shifted
 * in on IO23 (data) and IO18 (clock) and latched by IO0 (strobe). The bit order is epdiy's
 * lilygo_t5_47 board's, which calls power_enable "scan direction". epdiy's board definition drives the
 * register during updates; the fork uses this word for the rest state outside them (T5 spec §5.2). Pure
 * C, host-buildable.
 */
typedef struct {
    bool latch_enable;  /* LE: the source drivers latch the row just shifted in */
    bool power_disable; /* the rails' converter off (true at rest) */
    bool pos_power;     /* the positive rails, +22 V and +15 V */
    bool neg_power;     /* the negative rails, -20 V and -15 V */
    bool stv;           /* STV: the gate driver's start */
    bool power_enable;  /* PWR_EN: the panel's and the Molex connector's supply */
    bool mode;          /* MODE */
    bool output_enable; /* OE */
} epaper_sr_t;

/* The eight bits as a word, the first to shift in as the MSB: OE, MODE, PWR_EN, STV, NEG, POS,
 * power_disable, LE. */
uint8_t epaper_sr_word(const epaper_sr_t *s);
/* Rest: everything off, the converter disabled. At boot and in deep sleep (T5 spec §5.4). */
epaper_sr_t epaper_sr_off(void);
