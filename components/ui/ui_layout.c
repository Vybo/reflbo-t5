#include "ui_layout.h"

#include <string.h>

#include "ui_profile.h"

/* The tables live in the board's profile (ui_profile_rlcd42.c, ui_profile_t547.c): its slots and separators. */

const ui_layout_t *ui_layout(ui_layout_id_t id)
{
    return (unsigned)id < UI_LAYOUT_COUNT ? &ui_profile()->layouts[id] : NULL;
}

int ui_layout_by_name(const char *id)
{
    for (int i = 0; id != NULL && i < UI_LAYOUT_COUNT; i++) {
        if (strcmp(ui_profile()->layouts[i].id, id) == 0) {
            return i;
        }
    }
    return -1;
}

int ui_slot_by_name(const ui_layout_t *layout, const char *name)
{
    for (int i = 0; layout != NULL && name != NULL && i < layout->slot_count; i++) {
        if (strcmp(layout->slots[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}
