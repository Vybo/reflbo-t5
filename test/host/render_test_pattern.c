#include <stdio.h>
#include <string.h>

#include "gfx.h"
#include "gfx_test_pattern.h"

/* Writes the test pattern: render_test_pattern OUT.pbm (the RLCD's, 400×300 1 bpp), or
 * render_test_pattern --board t5 OUT.pgm (the T5's, 960×540 4 bpp, T5 spec §6.5). */
int main(int argc, char **argv)
{
    bool t5 = argc == 4 && strcmp(argv[1], "--board") == 0 && strcmp(argv[2], "t5") == 0;
    if (argc != 2 && !t5) {
        fprintf(stderr, "usage: %s [--board t5] OUT\n", argv[0]);
        return 2;
    }
    static uint8_t buf[960 * 540 / 2];
    static uint8_t img[960 * 540 + 32];
    gfx_fb_t fb;
    size_t n;
    if (t5) {
        gfx_fb_init_fmt(&fb, buf, 960, 540, GFX_FMT_4BPP);
        gfx_draw_test_pattern_t5(&fb);
        n = gfx_pgm_encode(&fb, img, sizeof(img));
    } else {
        gfx_fb_init(&fb, buf, 400, 300);
        gfx_draw_test_pattern(&fb);
        n = gfx_pbm_encode(&fb, img, sizeof(img));
    }
    const char *path = argv[argc - 1];
    FILE *f = fopen(path, "wb");
    if (f == NULL || fwrite(img, 1, n, f) != n) {
        perror(path);
        return 1;
    }
    fclose(f);
    return 0;
}
