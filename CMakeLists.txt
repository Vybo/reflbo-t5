cmake_minimum_required(VERSION 3.16)

# The board (T5 spec §4.1). tools/idf.sh passes -D REFLBO_BOARD=t5 for the LilyGo T5-ePaper-S3,
# which builds in build-t5/ with its own sdkconfig.t5 and sdkconfig.defaults.t5 on top of the
# shared defaults. Unset or rlcd42: the Waveshare RLCD-4.2, in build/ with sdkconfig.
set(SDKCONFIG_DEFAULTS "sdkconfig.defaults")
if(REFLBO_BOARD STREQUAL "t5")
    set(SDKCONFIG "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.t5")
    list(APPEND SDKCONFIG_DEFAULTS "sdkconfig.defaults.t5")
elseif(DEFINED REFLBO_BOARD AND NOT REFLBO_BOARD STREQUAL "rlcd42")
    message(FATAL_ERROR "REFLBO_BOARD must be rlcd42 or t5, not '${REFLBO_BOARD}'")
endif()
# Personal overrides (e.g. dev Wi-Fi) go in the gitignored sdkconfig.defaults.local.
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults.local")
    list(APPEND SDKCONFIG_DEFAULTS "sdkconfig.defaults.local")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(reflbo)
