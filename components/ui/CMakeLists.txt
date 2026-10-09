# Dashboard UI (spec §5): fields, widgets, layouts, presets. Pure C: also built on the host.
set(srcs "ui_fields.c" "ui_layout.c" "ui_profile.c" "ui_preset.c" "ui_preset_json.c" "ui_widget.c"
         "ui_status.c" "ui_dashboard.c" "ui_schedule.c" "ui_menu.c" "ui_menu_draw.c"
         "ui_screens.c" "ui_config.c" "ui_catalog.c" "ui_forecast.c" "ui_radar.c"
         "ui_flights.c" "ui_split.c" "ui_solar.c")
# One board's profile per image (T5 spec §7.1): its fonts, icons and layouts, never the other board's.
if(CONFIG_REFLBO_BOARD_T547)
    list(APPEND srcs "ui_profile_t547.c")
    set(profile ui_profile_t547)
else()
    list(APPEND srcs "ui_profile_rlcd42.c")
    set(profile ui_profile_rlcd42)
endif()
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       REQUIRES gfx locale datastore util map radar adsb solar energy
                       PRIV_REQUIRES json scheduler weather astro)
target_compile_definitions(${COMPONENT_LIB} PRIVATE UI_PROFILE_DEFAULT=${profile})
