#pragma once

#include <stdint.h>

#include "gfx.h"
#include "ui_fields.h"
#include "ui_profile.h"

/* Layouts (spec §5.2): fixed slot rectangles below the status bar, the board's own (T5 spec §7.3: in its
 * profile). Pure C, host-buildable. */

#define UI_STATUS_H (ui_profile()->status_h) /* 20 on the RLCD, 34 on the T5 (T5 spec §7.1) */
#define UI_SLOT_MAX 24 /* the split layout's cells (ui_split.h, M6c); the fixed layouts use up to 8 */

typedef enum {
    UI_LAYOUT_CLASSIC,
    UI_LAYOUT_WEATHER,
    UI_LAYOUT_GRID,
    UI_LAYOUT_FOCUS,
    UI_LAYOUT_RADAR,   /* M6: the weather radar's map, without slots (spec §5.2) */
    UI_LAYOUT_FLIGHTS, /* M6: the flight radar's map and panel, without slots */
    UI_LAYOUT_SPLIT,   /* M6b: cells from the preset's own tree (ui_split.h, D31), without fixed slots */
    UI_LAYOUT_SOLAR,   /* M6d: today's PV forecast, its chart and the next two days (spec §11.5), without slots */
    UI_LAYOUT_ENERGY,  /* M6d: the house's energy now and today's totals (spec §11.6), without slots */
    UI_LAYOUT_COUNT,
} ui_layout_id_t;

typedef enum {
    UI_SIZE_XS, /* M6c (D34): split cells from 40×20, drawn like the status bar (spec §5.3) */
    UI_SIZE_S,
    UI_SIZE_M,
    UI_SIZE_L,
    UI_SIZE_XL,
} ui_size_t;

/* The field kinds each size takes (spec §5.1): the fixed layouts' slots and the split layout's cells. */
#define UI_KINDS_S                                                                                                   \
    (UI_KIND(UI_FK_TIME) | UI_KIND(UI_FK_DATE) | UI_KIND(UI_FK_NUMBER) | UI_KIND(UI_FK_BATTERY) |                    \
     UI_KIND(UI_FK_MOON) | UI_KIND(UI_FK_TEXT) | UI_KIND(UI_FK_WEATHER_NOW) | UI_KIND(UI_FK_WEATHER_DAY) |           \
     UI_KIND(UI_FK_SUN) | UI_KIND(UI_FK_LEVEL) | UI_KIND(UI_FK_POLLEN))
#define UI_KINDS_XS UI_KINDS_S
#define UI_KINDS_M                                                                                                   \
    (UI_KINDS_S | UI_KIND(UI_FK_SERIES) | UI_KIND(UI_FK_RAIN_MAP) | UI_KIND(UI_FK_CHART) | UI_KIND(UI_FK_FLOW))
#define UI_KINDS_L (UI_KINDS_S | UI_KIND(UI_FK_RAIN_MAP) | UI_KIND(UI_FK_CHART) | UI_KIND(UI_FK_FLOW))
#define UI_KINDS_XL (UI_KIND(UI_FK_TIME) | UI_KIND(UI_FK_NUMBER))

typedef struct {
    const char *name; /* as in presets.json: "main", "s1" */
    gfx_rect_t rect;
    ui_size_t size;
    uint32_t kinds; /* UI_KIND() bits of the field kinds the slot accepts */
} ui_slot_t;

/* A separator line (spec §5.2): from (x, y), `len` pixels long, across or down. */
typedef struct {
    int16_t x, y, len;
    uint8_t vertical;
} ui_sep_t;

typedef struct ui_layout {
    const char *id; /* "classic" */
    const ui_slot_t *slots;
    int slot_count;
    const ui_sep_t *seps; /* the fixed layouts' separators (T3a: from the board's profile) */
    int sep_count;
} ui_layout_t;

const ui_layout_t *ui_layout(ui_layout_id_t id); /* NULL if out of range */
int ui_layout_by_name(const char *id);           /* -1 if unknown */
int ui_slot_by_name(const ui_layout_t *layout, const char *name);
