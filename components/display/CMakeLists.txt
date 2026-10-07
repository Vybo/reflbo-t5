# Display service: canonical framebuffer + panel pushes (spec §4.1, T5 spec §4.4). One source per board;
# REQUIRES stay the same for both, as ESP-IDF reads them before Kconfig.
if(CONFIG_REFLBO_BOARD_T5S3)
    set(srcs "display_t5s3.c")
else()
    set(srcs "display_rlcd42.c")
endif()
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       REQUIRES gfx st7305
                       PRIV_REQUIRES board epaper epdiy esp_timer util)
