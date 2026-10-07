# epdiy 2.1.3 (LGPL-3.0-or-later, LICENSE), vendored with reflbo's patches (PATCHES.md): the T5-4.7's
# panel (T5 spec §5.1). Only the T5 builds sources, and only those of the I2S output path; on other boards
# the component is headers only. REQUIRES stay the same for both boards, as ESP-IDF reads them before Kconfig.
if(CONFIG_REFLBO_BOARD_T547)
    set(srcs "") # Task 3 of T1 lists the ESP32's sources
else()
    set(srcs "")
endif()
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "src"
                       REQUIRES driver esp_timer esp_adc esp_lcd esp_driver_i2c
                       PRIV_REQUIRES hal soc esp_hw_support esp_rom)
if(CONFIG_REFLBO_BOARD_T547 AND srcs)
    # As upstream's CMakeLists: format specifiers differ between ESP-IDF versions; the LUT code must not use
    # the ESP32's PSRAM cache workaround.
    target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=format= -Wno-format)
    set_source_files_properties("src/output_common/lut.c" PROPERTIES COMPILE_OPTIONS -mno-fix-esp32-psram-cache-issue)
endif()
