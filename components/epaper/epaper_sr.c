#include "epaper_sr.h"

uint8_t epaper_sr_word(const epaper_sr_t *s)
{
    return (uint8_t)((s->output_enable << 7) | (s->mode << 6) | (s->power_enable << 5) | (s->stv << 4) |
                     (s->neg_power << 3) | (s->pos_power << 2) | (s->power_disable << 1) | s->latch_enable);
}

epaper_sr_t epaper_sr_off(void)
{
    return (epaper_sr_t){ .power_disable = true };
}

int epaper_sr_poweron(epaper_sr_t s, epaper_sr_step_t out[EPAPER_SR_POWER_STEPS])
{
    s.power_enable = true;
    s.power_disable = false;
    out[0] = (epaper_sr_step_t){ s, 100 };
    s.neg_power = true;
    out[1] = (epaper_sr_step_t){ s, 500 };
    s.pos_power = true;
    out[2] = (epaper_sr_step_t){ s, 100 };
    s.stv = true;
    out[3] = (epaper_sr_step_t){ s, 0 };
    return 4;
}

int epaper_sr_poweroff(epaper_sr_t s, epaper_sr_step_t out[EPAPER_SR_POWER_STEPS])
{
    s.pos_power = false;
    out[0] = (epaper_sr_step_t){ s, 10 };
    s.neg_power = false;
    out[1] = (epaper_sr_step_t){ s, 100 };
    s.stv = false;
    s.output_enable = false;
    s.mode = false;
    s.power_disable = true;
    s.power_enable = false;
    out[2] = (epaper_sr_step_t){ s, 0 };
    return 3;
}
