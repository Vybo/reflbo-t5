# The T5's panel outside epdiy (T5 spec §5.2): the shift register's rest word, the 1 bpp blit, epdiy's
# RAM budget and the update policy (T4). Only the T5 builds its sources; on other boards it's headers
# only, and REQUIRES stay the same for both, as ESP-IDF reads them before Kconfig. epaper_sr.c,
# epaper_frame.c, epaper_budget.c and epaper_update.c are pure C and also built on the host.
if(CONFIG_REFLBO_BOARD_T547)
    set(srcs "epaper_board.c" "epaper_budget.c" "epaper_sr.c" "epaper_frame.c" "epaper_update.c")
else()
    set(srcs "")
endif()
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       REQUIRES gfx
                       PRIV_REQUIRES board esp_driver_gpio)
