# epdiy 2.1.3 (LGPL-3.0-or-later, LICENSE), vendored with reflbo's patches (PATCHES.md): the T5-ePaper-S3's
# panel (T5 spec §5.1). Only the T5 builds sources, and only those of the I2S output path; on other boards
# the component is headers only. REQUIRES stay the same for both boards, as ESP-IDF reads them before Kconfig.
if(CONFIG_REFLBO_BOARD_T5S3)
    set(srcs "src/epdiy.c" "src/render.c" "src/highlevel.c" "src/displays.c" "src/builtin_waveforms.c"
             "src/board/epd_board.c"
             "src/output_i2s/render_i2s.c" "src/output_i2s/rmt_pulse.c" "src/output_i2s/i2s_data_bus_s3.c"
             "src/output_common/lut.c" "src/output_common/line_queue.c" "src/output_common/render_context.c"
             "src/output_common/render_method.c" "src/output_common/rmt_compat.c")
else()
    set(srcs "")
endif()
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "src"
                       REQUIRES driver esp_timer esp_adc esp_lcd esp_driver_i2c
                       PRIV_REQUIRES hal soc esp_hw_support esp_rom)
if(CONFIG_REFLBO_BOARD_T5S3)
    # As upstream's CMakeLists: format specifiers differ between ESP-IDF versions; the LUT code must not use
    # the ESP32's PSRAM cache workaround.
    target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=format= -Wno-format)
    set_source_files_properties("src/output_common/lut.c" PROPERTIES COMPILE_OPTIONS -mno-fix-esp32-psram-cache-issue)
endif()
