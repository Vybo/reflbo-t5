# The board's RTC chip (spec §7, T5 spec §4.5): the PCF85063A on the RLCD board, the PCF8563 on the
# T5 (T1). pcf85063_regs.c is pure C and also built on the host. REQUIRES stay the same for both
# boards: ESP-IDF reads them before Kconfig.
if(CONFIG_REFLBO_BOARD_T5S3)
    set(srcs "rtcchip_t5s3.c")
else()
    set(srcs "pcf85063.c" "pcf85063_regs.c" "rtcchip_rlcd42.c")
endif()
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       REQUIRES board esp_driver_i2c
                       PRIV_REQUIRES util)
