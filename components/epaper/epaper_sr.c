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
