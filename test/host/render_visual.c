#define _POSIX_C_SOURCE 200809L /* setenv() in fixture_zone() */

#include <stdio.h>
#include <string.h>

#include "gfx.h"
#include "golden_io.h"
#include "ui_profile.h"
#include "visual_fixtures.h"

/* build-host/render_visual [--board t5] <sheet> <out>: one of the visuals' sheets as the RLCD's PBM, or with
 * `--board t5` as the T5's 4-bit PGM (gzipped when <out> ends in .gz; T5 spec §7.7) (tools/render.py).
 * `--list` prints the sheets' names. */
int main(int argc, char **argv)
{
    static uint8_t buf[960 * 540 / 2];
    static uint8_t img[960 * 540 + 32];
    if (argc == 2 && strcmp(argv[1], "--list") == 0) {
        for (size_t i = 0; i < sizeof(k_visual_fixtures) / sizeof(k_visual_fixtures[0]); i++) {
            printf("%s\n", k_visual_fixtures[i]);
        }
        return 0;
    }
    bool t5 = argc == 5 && strcmp(argv[1], "--board") == 0 && strcmp(argv[2], "t5") == 0;
    if (t5) {
        ui_profile_use(&ui_profile_t547);
    }
    const ui_profile_t *p = ui_profile();
    gfx_fb_t fb;
    gfx_fb_init_fmt(&fb, buf, p->width, p->height, p->format);
    if ((argc != 3 && !t5) || !fixture_visual(argv[argc - 2], &fb)) {
        fprintf(stderr, "usage: render_visual [--board t5] <sheet> <out> | --list\n");
        return 2;
    }
    size_t n = t5 ? gfx_pgm_encode(&fb, img, sizeof(img)) : gfx_pbm_encode(&fb, img, sizeof(img));
    return golden_write(argv[argc - 1], img, n) ? 0 : 1;
}
