# ST7305 reflective LCD (spec §4.2): the RLCD board's panel. On the T5 the component is its headers only.
# st7305_frame.c is pure C and also built on the host.
if(CONFIG_REFLBO_BOARD_T547)
    set(srcs "")
else()
    set(srcs "st7305.c" "st7305_frame.c")
endif()
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES board esp_driver_gpio esp_driver_spi esp_lcd util)
