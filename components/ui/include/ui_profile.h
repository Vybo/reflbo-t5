#pragma once

#include <stdint.h>

#include "gfx.h"
#include "ui_icons.h"

/*
 * The board as the UI sees it (T5 spec §7.1): the panel's size and format, the status bar, the board's name and
 * what it has, and since T3a its fonts by role, its icons by size class, its pixel scale (UI_PX()), its fixed
 * layouts, the menu's rows and the split layout's limits. Each board's instance is its own source
 * (ui_profile_rlcd42.c, ui_profile_t547.c), built into that board's image only. Pure C, host-buildable.
 */

/* UI_CAP_* bits: hardware a board has, shown by the UI and the web page only where it exists. main
 * checks them against board_caps.h's BOARD_HAS_*. */
#define UI_CAP_ENV_SENSOR     (1u << 0) /* temperature and humidity (env.*) */
#define UI_CAP_AUDIO          (1u << 1)
#define UI_CAP_RTC_TRIM       (1u << 2)
#define UI_CAP_RTC_ALARM_WAKE (1u << 3)
#define UI_CAP_LPM_RATE       (1u << 4) /* the panel's refresh rate setting */

#define UI_CAPS_RLCD42 (UI_CAP_ENV_SENSOR | UI_CAP_AUDIO | UI_CAP_RTC_TRIM | UI_CAP_RTC_ALARM_WAKE | UI_CAP_LPM_RATE)
#define UI_CAPS_T547   0u

/* The fonts the UI draws with, by role: named after the RLCD's (T5 spec §7.2); the T5 maps each role to its own
 * 4-bit font at about 1.7× the size. */
typedef enum {
    UI_F_SANS_12,
    UI_F_SANS_16,
    UI_F_SANS_20,
    UI_F_BOLD_16,
    UI_F_BOLD_20,
    UI_F_BOLD_28,
    UI_F_NUM_48,
    UI_F_NUM_72,
    UI_F_NUM_110,
    UI_F_NUM_130,
    UI_F_COUNT,
} ui_font_id_t;

/* Icon size classes, named by the RLCD's pixels: 16, 24 and 48 there; 26, 40 and 80 on the T5. */
typedef enum {
    UI_IC16,
    UI_IC24,
    UI_IC48,
    UI_IC_CLASSES,
} ui_icon_class_t;

typedef struct {
    int16_t header_h, row_y0, row_h, rows; /* the menu's title bar, its first row, a row's pitch and how many */
} ui_menu_geometry_t;

typedef struct {
    int16_t min_w, min_h; /* the smallest split cell */
    int16_t narrow_w;     /* below it a cell is narrow (S beside, M6c) */
    int16_t inset;        /* a separator's ends stop this far from its cell's edges */
} ui_split_limits_t;

struct ui_layout; /* ui_layout.h */

typedef struct {
    const char *board; /* "rlcd42", "t547": as BOARD_NAME (board_caps.h) */
    int16_t width, height;
    int16_t status_h; /* the status bar; its line is the row below it */
    uint32_t caps;    /* UI_CAP_* */
    uint8_t format;   /* gfx_format_t: the panel's frame, 1 bpp on the RLCD, 4 bpp on the T5 */
    uint8_t px_num, px_den;                            /* UI_PX(): n × num / den rounded; 1/1 on the RLCD */
    const gfx_font_t *const *fonts;                    /* [UI_F_COUNT] */
    const gfx_bitmap_t *const (*icons)[UI_IC_CLASSES]; /* [UI_ICON_COUNT] */
    int16_t icon_px[UI_IC_CLASSES];                    /* each class's size in pixels */
    const struct ui_layout *layouts;                   /* [UI_LAYOUT_COUNT] */
    ui_menu_geometry_t menu;
    ui_split_limits_t split;
    /* The owner's tuning of the T5's renders (T3a review), the RLCD's as it was: */
    uint8_t sun_s_font; /* ui_font_id_t: the sun's times in an S cell, before smaller faces */
    uint8_t sun_s_icon; /* ui_icon_class_t: the sunrise and sunset icons beside them */
    bool sun_s_top;     /* in a narrow S cell the sun stacks from the top like the other S widgets; else centred */
} ui_profile_t;

extern const ui_profile_t ui_profile_rlcd42;
extern const ui_profile_t ui_profile_t547;

/* The profile in use: the board's (the RLCD's on the host) until ui_profile_use(). */
const ui_profile_t *ui_profile(void);
/* Sets the profile in use; NULL restores the default. The firmware calls it once at boot. */
void ui_profile_use(const ui_profile_t *profile);
/* "env_sensor", "audio", "rtc_trim", "rtc_alarm_wake", "lpm_rate"; NULL for anything but one known bit. */
const char *ui_cap_name(uint32_t cap);

/* A pixel size in the RLCD's units for this board: n itself on the RLCD, n × 1.7 rounded half away from zero on
 * the T5 (T5 spec §7.1). */
int ui_px(int n);
#define UI_PX(n) ui_px(n)
/* The font of a role (ui_font_id_t) for this board. */
#define UI_FONT(id) (ui_profile()->fonts[(id)])
/* An icon of a size class for this board; NULL out of range. */
const gfx_bitmap_t *ui_icon(ui_icon_id_t id, ui_icon_class_t cls);
/* The class an icon size in the RLCD's pixels picks: ≥ 48 UI_IC48, ≥ 24 UI_IC24, else UI_IC16. */
ui_icon_class_t ui_icon_class(int rlcd_px);
/* The pixels of the class rlcd_px picks, on this board. */
int ui_icon_px(int rlcd_px);
/* Whether fb is the frame the profile draws: its width, height and format (T3a); false for NULL. */
bool ui_profile_matches(const gfx_fb_t *fb);
