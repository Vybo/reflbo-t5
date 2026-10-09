#define _POSIX_C_SOURCE 200809L /* setenv() in fixture_zone() */

#include <stdio.h>
#include <string.h>

#include "dashboard_fixtures.h"
#include "gfx.h"
#include "golden_io.h"
#include "ui_profile.h"

/* build-host/render_dashboard [--board t5] <fixture> <out>: one dashboard fixture as the RLCD's PBM, or with
 * --board t5 as the T5's 4 bpp PGM, gzip-compressed when out ends ".gz" (tools/render.py). `--list` prints the
 * fixture names; `[--board t5] --layouts` each fixture's layout on that board. */
int main(int argc, char **argv)
{
    static uint8_t buf[960 * 540 / 2];
    static uint8_t img[960 * 540 + 32];
    if (argc == 2 && strcmp(argv[1], "--list") == 0) {
        for (size_t i = 0; i < sizeof(k_dashboard_fixtures) / sizeof(k_dashboard_fixtures[0]); i++) {
            printf("%s\n", k_dashboard_fixtures[i]);
        }
        return 0;
    }
    bool t5 = (argc == 5 || argc == 4) && strcmp(argv[1], "--board") == 0 && strcmp(argv[2], "t5") == 0;
    if (t5) {
        ui_profile_use(&ui_profile_t547);
    }
    if ((argc == 2 || (t5 && argc == 4)) && strcmp(argv[argc - 1], "--layouts") == 0) {
        for (size_t i = 0; i < sizeof(k_dashboard_fixtures) / sizeof(k_dashboard_fixtures[0]); i++) {
            ui_context_t ctx;
            ui_preset_t preset;
            if (fixture_dashboard(k_dashboard_fixtures[i], &ctx, &preset)) {
                printf("%s %s\n", k_dashboard_fixtures[i], ui_layout((ui_layout_id_t)preset.layout)->id);
            }
        }
        return 0;
    }
    if (argc == 4) {
        t5 = false; /* --board t5 with one more argument: only --layouts */
    }
    ui_context_t ctx;
    ui_preset_t preset;
    if ((argc != 3 && !t5) || !fixture_dashboard(argv[argc - 2], &ctx, &preset)) {
        fprintf(stderr, "usage: render_dashboard [--board t5] <fixture> <out> | --list\n");
        return 2;
    }
    const ui_profile_t *p = ui_profile();
    gfx_fb_t fb;
    gfx_fb_init_fmt(&fb, buf, p->width, p->height, p->format);
    ui_draw_dashboard(&fb, &ctx, &preset);
    size_t n = t5 ? gfx_pgm_encode(&fb, img, sizeof(img)) : gfx_pbm_encode(&fb, img, sizeof(img));
    return golden_write(argv[argc - 1], img, n) ? 0 : 1;
}
