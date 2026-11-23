#pragma once

#include <stdbool.h>
#include <stdint.h>

/* The T5's update policy (T5 spec §5.3), pure: what changed between the panel's frame and the next, and how to put
 * the next one on the panel. Frames are 4 bpp, two pixels a byte, the even pixel in the low nibble (gfx's and
 * epdiy's layout), 0 black and 15 white. */

typedef struct {
    bool any;           /* some pixel differs */
    bool bw_only;       /* every differing pixel is black (0) or white (15) before and after */
    int16_t x, y, w, h; /* the differing pixels' bounding box; all 0 when !any */
} epaper_change_t;

void epaper_change(const uint8_t *before, const uint8_t *after, int w, int h, epaper_change_t *out);

typedef enum { EPAPER_NONE, EPAPER_CLEAN, EPAPER_DU, EPAPER_GL16 } epaper_mode_t;

typedef struct {
    bool known;           /* the panel's frame is known (epdiy's back buffer) */
    bool clean_next;      /* a preset switch, leaving the menu, the end of a night, `panel clean` */
    bool clean_again;     /* asked for while an update ran (the T5's Back held through it): clean_next after it */
    bool fast;            /* display_set_fast(true): fast and not counted */
    bool session;         /* a PC session (T5 spec DT18): fast and not counted, clean every session_clean_ms */
    uint16_t since_clean; /* counted updates since the last clean one */
    uint8_t clean_after;  /* every Nth update is clean, 1..240 */
    uint32_t session_clean_ms; /* a session's clean this long after the last clean one */
    uint32_t clean_ms;         /* when the last clean one ran, on now_ms's clock */
    uint32_t now_ms;           /* the caller's monotonic clock at this update, in ms; it may wrap */
} epaper_policy_t;

/* An unknown frame or a clean request: clean, even unchanged. Otherwise nothing for no change; in a session (not in
 * the menu), clean once session_clean_ms have passed since the last clean one; outside it, clean for the
 * clean_after-th counted update; DU for a black-and-white change, GL16 for any other. */
epaper_mode_t epaper_choose(const epaper_policy_t *p, const epaper_change_t *c);
typedef enum {
    EPAPER_DONE,    /* it ran */
    EPAPER_FAILED,  /* it started and failed: the panel may show part of it */
    EPAPER_REFUSED, /* refused before it drew (epdiy's RAM budget): the panel still shows its frame */
} epaper_result_t;

/* After an update of `mode`: a clean one restarts the count, a counted fast one adds to it; a failed one leaves the
 * panel's frame unknown (the next is clean) and the count as it was; a refused one changes nothing. */
void epaper_after(epaper_policy_t *p, epaper_mode_t mode, epaper_result_t result);

/* A PC session starts or ends (T5 spec DT18), its clean every `clean_ms`; its end flashes clean, as the menu's does,
 * unless it ends in fast mode (the menu), whose own end flashes. */
void epaper_session(epaper_policy_t *p, bool on, uint32_t clean_ms);
