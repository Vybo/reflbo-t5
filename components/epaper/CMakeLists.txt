# The T5-ePaper-S3's panel (T5 spec §5.2): the epdiy board definition for V2.3 and two pure helpers.
# Only the T5 builds its sources; on other boards it's headers only, and REQUIRES stay the same for both,
# as ESP-IDF reads them before Kconfig. epaper_sr.c and epaper_frame.c are pure C and also built on the host.
if(CONFIG_REFLBO_BOARD_T5S3)
    set(srcs "epaper_sr.c" "epaper_frame.c")
else()
    set(srcs "")
endif()
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       REQUIRES gfx)
