# Drawing at 1 and 4 bpp, fonts and icons (spec §4.3–4.5, T5 spec §6). Pure C: also built on the host by test/host.
# Sources are listed, not globbed: ESP-IDF globs SRC_DIRS only at configure time.
idf_component_register(SRCS "gfx.c" "gfx_bmp.c" "gfx_pbm.c" "gfx_qr.c" "gfx_test_pattern.c" "gfx_test_pattern_t5.c" "gfx_text.c"
                            "qrcodegen/qrcodegen.c"
                            "fonts/gfx_font_sans_12.c" "fonts/gfx_font_sans_16.c"
                            "fonts/gfx_font_sans_20.c" "fonts/gfx_font_sans_bold_20.c"
                            "fonts/gfx_font_sans_bold_16.c" "fonts/gfx_font_sans_bold_28.c"
                            "fonts/gfx_font_num_cb_48.c" "fonts/gfx_font_num_cb_72.c" "fonts/gfx_font_num_cb_110.c"
                            "fonts/gfx_font_num_cb_130.c" "fonts/gfx_font_t5_sans_26.c"
                            "icons/gfx_icons.c" "icons/gfx_icons_t5.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "qrcodegen")
