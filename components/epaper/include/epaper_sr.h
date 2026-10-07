#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
 * The T5-ePaper-S3's 74HCT4094 (T5 spec §2.3): eight control bits for the panel and its rails, shifted
 * in on IO13 (data) and IO12 (clock) and latched by IO0 (strobe). The bit order and the rails' timings
 * are facts from the vendor's board support and epdiy's ESP32 T5 board, which call power_enable "scan
 * direction". Pure C, host-buildable.
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

typedef struct {
    epaper_sr_t state; /* push this */
    uint16_t wait_us;  /* then wait this long */
} epaper_sr_step_t;

#define EPAPER_SR_POWER_STEPS 4

/* Power-on from `from`: the supply, then the negative rails, then the positive ones, then STV. Returns
 * the number of steps. The row signals (LE, OE, MODE) stay as `from` has them. */
int epaper_sr_poweron(epaper_sr_t from, epaper_sr_step_t out[EPAPER_SR_POWER_STEPS]);
/* Power-off from `from`: the positive rails, then the negative ones, then everything else off. Returns
 * the number of steps; the last state is epaper_sr_off()'s, LE apart. */
int epaper_sr_poweroff(epaper_sr_t from, epaper_sr_step_t out[EPAPER_SR_POWER_STEPS]);
