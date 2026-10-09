# Drawing at 1 and 4 bpp, fonts and icons (spec §4.3–4.5, T5 spec §6). Pure C: also built on the host by test/host.
# Sources are listed, not globbed: ESP-IDF globs SRC_DIRS only at configure time.
set(srcs "gfx.c" "gfx_bmp.c" "gfx_pbm.c" "gfx_qr.c" "gfx_test_pattern.c" "gfx_text.c"
         "qrcodegen/qrcodegen.c"
         "fonts/gfx_font_sans_12.c" "fonts/gfx_font_sans_16.c"
         "fonts/gfx_font_sans_20.c" "fonts/gfx_font_sans_bold_20.c"
         "fonts/gfx_font_sans_bold_16.c" "fonts/gfx_font_sans_bold_28.c"
         "fonts/gfx_font_num_cb_48.c" "fonts/gfx_font_num_cb_72.c" "fonts/gfx_font_num_cb_110.c"
         "fonts/gfx_font_num_cb_130.c"
         "icons/gfx_icons.c")
if(CONFIG_REFLBO_BOARD_T547) # the T5's 4-bit assets: only its image carries them (T5 spec §4.1)
    list(APPEND srcs "gfx_test_pattern_t5.c" "icons/gfx_icons_t5.c"
                     "fonts/gfx_font_t5_sans_20.c" "fonts/gfx_font_t5_sans_26.c" "fonts/gfx_font_t5_sans_34.c"
                     "fonts/gfx_font_t5_bold_26.c" "fonts/gfx_font_t5_bold_34.c" "fonts/gfx_font_t5_bold_46.c"
                     "fonts/gfx_font_t5_num_80.c" "fonts/gfx_font_t5_num_120.c" "fonts/gfx_font_t5_num_180.c"
                     "fonts/gfx_font_t5_num_220.c")
endif()
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "qrcodegen")
