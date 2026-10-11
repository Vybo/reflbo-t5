#include "ui_controls.h"

#include <stdbool.h>

#include "ui_profile.h"

static ui_control_t act(ui_do_t what)
{
    return (ui_control_t){ .what = what };
}

static ui_control_t menu_key(ui_menu_key_t key)
{
    return (ui_control_t){ .what = UI_DO_MENU_KEY, .key = key };
}

/* The RLCD's KEY and BOOT (upstream §5.6). */
static ui_control_t two_buttons(ui_screen_t screen, ui_button_t button, ui_gesture_t gesture)
{
    bool key = button == UI_BTN_KEY, boot = button == UI_BTN_BOOT, held = gesture == UI_GESTURE_LONG;
    if (!key && !boot) {
        return act(UI_DO_NOTHING);
    }
    switch (screen) {
    case UI_SCREEN_MENU:
        return menu_key(key ? (held ? UI_MENU_KEY_SELECT : UI_MENU_KEY_NEXT) : (held ? UI_MENU_KEY_EXIT : UI_MENU_KEY_BACK));
    case UI_SCREEN_CONFIG:
        return act(key && gesture == UI_GESTURE_SHORT ? UI_DO_CONFIG_KEY : boot && held ? UI_DO_CONFIG_EXIT : UI_DO_NOTHING);
    case UI_SCREEN_CRITICAL:
        return act(UI_DO_RESAMPLE);
    case UI_SCREEN_FIRST_RUN:
        if (boot) {
            return act(held ? UI_DO_CONFIG_ENTER : UI_DO_NOTHING);
        }
        return act(gesture == UI_GESTURE_SHORT ? UI_DO_FIRST_RUN_CONTINUE : held ? UI_DO_FIRST_RUN_MENU : UI_DO_NOTHING);
    case UI_SCREEN_DASHBOARD:
    default:
        if (key) {
            return act(gesture == UI_GESTURE_SHORT    ? UI_DO_PRESET_NEXT
                       : gesture == UI_GESTURE_DOUBLE ? UI_DO_CYCLE_TOGGLE
                                                      : UI_DO_MENU_OPEN);
        }
        return act(gesture == UI_GESTURE_SHORT    ? UI_DO_REFRESH
                   : gesture == UI_GESTURE_DOUBLE ? UI_DO_ALWAYS_TOGGLE
                                                  : UI_DO_CONFIG_ENTER);
    }
}

/* The T5's Back, Up, Enter and Down (T5 spec §8.3, DT13): Enter on KEY's pin, Up on BOOT's. */
static ui_control_t four_buttons(ui_screen_t screen, ui_button_t button, ui_gesture_t gesture)
{
    bool enter = button == UI_BTN_KEY, up = button == UI_BTN_BOOT, down = button == UI_BTN_DOWN;
    bool back = button == UI_BTN_BACK, held = gesture == UI_GESTURE_LONG;
    switch (screen) {
    case UI_SCREEN_MENU:
        if (back || (enter && held)) {
            return menu_key(UI_MENU_KEY_ESCAPE);
        }
        if (enter) {
            return menu_key(UI_MENU_KEY_SELECT);
        }
        if (held) {
            return act(UI_DO_NOTHING);
        }
        return menu_key(up ? UI_MENU_KEY_UP : UI_MENU_KEY_DOWN);
    case UI_SCREEN_CONFIG:
        if (back || (enter && held)) {
            return act(UI_DO_CONFIG_EXIT);
        }
        return act(enter && gesture == UI_GESTURE_SHORT ? UI_DO_CONFIG_KEY : UI_DO_NOTHING);
    case UI_SCREEN_CRITICAL:
        return act(UI_DO_RESAMPLE);
    case UI_SCREEN_FIRST_RUN:
        if (up && held) {
            return act(UI_DO_CONFIG_ENTER);
        }
        if (enter) {
            return act(gesture == UI_GESTURE_SHORT ? UI_DO_FIRST_RUN_CONTINUE : held ? UI_DO_FIRST_RUN_MENU : UI_DO_NOTHING);
        }
        return act(UI_DO_NOTHING);
    case UI_SCREEN_DASHBOARD:
    default:
        if (enter) {
            return act(gesture == UI_GESTURE_SHORT    ? UI_DO_REFRESH
                       : gesture == UI_GESTURE_DOUBLE ? UI_DO_ALWAYS_TOGGLE
                                                      : UI_DO_MENU_OPEN);
        }
        if (up) {
            return act(gesture == UI_GESTURE_SHORT ? UI_DO_PRESET_PREV : held ? UI_DO_CONFIG_ENTER : UI_DO_NOTHING);
        }
        if (down) {
            return act(gesture == UI_GESTURE_SHORT    ? UI_DO_PRESET_NEXT
                       : gesture == UI_GESTURE_DOUBLE ? UI_DO_CYCLE_TOGGLE
                                                      : UI_DO_NOTHING);
        }
        return act(UI_DO_NOTHING); /* Back: nothing here, though any press stops the radar's loop */
    }
}

ui_control_t ui_controls(ui_screen_t screen, ui_button_t button, ui_gesture_t gesture)
{
    return ui_profile()->buttons >= 4 ? four_buttons(screen, button, gesture) : two_buttons(screen, button, gesture);
}

lang_str_t ui_hint(lang_str_t two_button)
{
    if (ui_profile()->buttons < 4) {
        return two_button;
    }
    switch (two_button) {
    case LS_HINT_BROWSE:
        return LS_HINT4_BROWSE;
    case LS_HINT_EDIT:
        return LS_HINT4_EDIT;
    case LS_HINT_DATETIME:
        return LS_HINT4_DATETIME;
    case LS_HINT_CONFIRM:
        return LS_HINT4_CONFIRM;
    case LS_HINT_CONFIG:
        return LS_HINT4_CONFIG;
    case LS_HINT_CONFIG_SWITCH:
        return LS_HINT4_CONFIG_SWITCH;
    case LS_HINT_CONFIG_BACK:
        return LS_HINT4_CONFIG_BACK;
    case LS_F_WIFI:
        return LS_F4_WIFI;
    case LS_F_MENU:
        return LS_F4_MENU;
    case LS_F_CONTINUE:
        return LS_F4_CONTINUE;
    default:
        return two_button;
    }
}
