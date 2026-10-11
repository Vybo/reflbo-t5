#include <stdio.h>

#include "gfx.h"
#include "ui_controls.h"
#include "ui_internal.h"
#include "ui_profile.h"
#include "unity.h"

/* T4b: each board's buttons, gesture by gesture and screen by screen (upstream §5.6, T5 spec §8.3). */

void setUp(void) {}

void tearDown(void)
{
    ui_profile_use(NULL);
}

typedef struct {
    ui_screen_t screen;
    ui_button_t button;
    ui_gesture_t gesture;
    ui_do_t what;
    ui_menu_key_t key;
} row_t;

#define S UI_GESTURE_SHORT
#define D UI_GESTURE_DOUBLE
#define L UI_GESTURE_LONG

/* Every combination not listed does nothing. */
static void check(const row_t *rows, int count, int buttons)
{
    const ui_button_t all[] = { UI_BTN_KEY, UI_BTN_BOOT, UI_BTN_DOWN, UI_BTN_BACK };
    for (int screen = UI_SCREEN_DASHBOARD; screen <= UI_SCREEN_CRITICAL; screen++) {
        for (int b = 0; b < 4; b++) {
            for (int g = UI_GESTURE_SHORT; g <= UI_GESTURE_LONG; g++) {
                const row_t *want = NULL;
                for (int i = 0; i < count; i++) {
                    if (rows[i].screen == (ui_screen_t)screen && rows[i].button == all[b] && rows[i].gesture == (ui_gesture_t)g) {
                        want = &rows[i];
                    }
                }
                ui_control_t got = ui_controls((ui_screen_t)screen, all[b], (ui_gesture_t)g);
                char msg[64];
                snprintf(msg, sizeof(msg), "%d buttons: screen %d, button %d, gesture %d", buttons, screen, b, g);
                TEST_ASSERT_EQUAL_INT_MESSAGE(want ? want->what : UI_DO_NOTHING, got.what, msg);
                if (want && want->what == UI_DO_MENU_KEY) {
                    TEST_ASSERT_EQUAL_INT_MESSAGE(want->key, got.key, msg);
                }
            }
        }
    }
}

/* main/app.c's handler before T4b, row by row: the RLCD keeps it. */
static void test_the_rlcd_controls_are_upstreams(void)
{
    static const row_t rows[] = {
        { UI_SCREEN_DASHBOARD, UI_BTN_KEY, S, UI_DO_PRESET_NEXT, 0 },
        { UI_SCREEN_DASHBOARD, UI_BTN_KEY, D, UI_DO_CYCLE_TOGGLE, 0 },
        { UI_SCREEN_DASHBOARD, UI_BTN_KEY, L, UI_DO_MENU_OPEN, 0 },
        { UI_SCREEN_DASHBOARD, UI_BTN_BOOT, S, UI_DO_REFRESH, 0 },
        { UI_SCREEN_DASHBOARD, UI_BTN_BOOT, D, UI_DO_ALWAYS_TOGGLE, 0 },
        { UI_SCREEN_DASHBOARD, UI_BTN_BOOT, L, UI_DO_CONFIG_ENTER, 0 },
        { UI_SCREEN_MENU, UI_BTN_KEY, S, UI_DO_MENU_KEY, UI_MENU_KEY_NEXT },
        { UI_SCREEN_MENU, UI_BTN_KEY, D, UI_DO_MENU_KEY, UI_MENU_KEY_NEXT },
        { UI_SCREEN_MENU, UI_BTN_KEY, L, UI_DO_MENU_KEY, UI_MENU_KEY_SELECT },
        { UI_SCREEN_MENU, UI_BTN_BOOT, S, UI_DO_MENU_KEY, UI_MENU_KEY_BACK },
        { UI_SCREEN_MENU, UI_BTN_BOOT, D, UI_DO_MENU_KEY, UI_MENU_KEY_BACK },
        { UI_SCREEN_MENU, UI_BTN_BOOT, L, UI_DO_MENU_KEY, UI_MENU_KEY_EXIT },
        { UI_SCREEN_CONFIG, UI_BTN_KEY, S, UI_DO_CONFIG_KEY, 0 },
        { UI_SCREEN_CONFIG, UI_BTN_BOOT, L, UI_DO_CONFIG_EXIT, 0 },
        { UI_SCREEN_FIRST_RUN, UI_BTN_KEY, S, UI_DO_FIRST_RUN_CONTINUE, 0 },
        { UI_SCREEN_FIRST_RUN, UI_BTN_KEY, L, UI_DO_FIRST_RUN_MENU, 0 },
        { UI_SCREEN_FIRST_RUN, UI_BTN_BOOT, L, UI_DO_CONFIG_ENTER, 0 },
        { UI_SCREEN_CRITICAL, UI_BTN_KEY, S, UI_DO_RESAMPLE, 0 },
        { UI_SCREEN_CRITICAL, UI_BTN_KEY, D, UI_DO_RESAMPLE, 0 },
        { UI_SCREEN_CRITICAL, UI_BTN_KEY, L, UI_DO_RESAMPLE, 0 },
        { UI_SCREEN_CRITICAL, UI_BTN_BOOT, S, UI_DO_RESAMPLE, 0 },
        { UI_SCREEN_CRITICAL, UI_BTN_BOOT, D, UI_DO_RESAMPLE, 0 },
        { UI_SCREEN_CRITICAL, UI_BTN_BOOT, L, UI_DO_RESAMPLE, 0 },
    };
    check(rows, (int)(sizeof(rows) / sizeof(rows[0])), 2);
}

/* T5 spec §8.3 (DT13): Back, Up, Enter, Down in the board's order. */
static void test_the_t5_controls_follow_dt13(void)
{
    ui_profile_use(&ui_profile_t547);
    static const row_t rows[] = {
        { UI_SCREEN_DASHBOARD, UI_BTN_BOOT, S, UI_DO_PRESET_PREV, 0 },
        { UI_SCREEN_DASHBOARD, UI_BTN_DOWN, S, UI_DO_PRESET_NEXT, 0 },
        { UI_SCREEN_DASHBOARD, UI_BTN_KEY, S, UI_DO_REFRESH, 0 },
        { UI_SCREEN_DASHBOARD, UI_BTN_KEY, L, UI_DO_MENU_OPEN, 0 },
        { UI_SCREEN_DASHBOARD, UI_BTN_BOOT, L, UI_DO_CONFIG_ENTER, 0 },
        { UI_SCREEN_DASHBOARD, UI_BTN_KEY, D, UI_DO_ALWAYS_TOGGLE, 0 },
        { UI_SCREEN_DASHBOARD, UI_BTN_DOWN, D, UI_DO_CYCLE_TOGGLE, 0 },
        { UI_SCREEN_MENU, UI_BTN_BOOT, S, UI_DO_MENU_KEY, UI_MENU_KEY_UP },
        { UI_SCREEN_MENU, UI_BTN_BOOT, D, UI_DO_MENU_KEY, UI_MENU_KEY_UP },
        { UI_SCREEN_MENU, UI_BTN_DOWN, S, UI_DO_MENU_KEY, UI_MENU_KEY_DOWN },
        { UI_SCREEN_MENU, UI_BTN_DOWN, D, UI_DO_MENU_KEY, UI_MENU_KEY_DOWN },
        { UI_SCREEN_MENU, UI_BTN_KEY, S, UI_DO_MENU_KEY, UI_MENU_KEY_SELECT },
        { UI_SCREEN_MENU, UI_BTN_KEY, D, UI_DO_MENU_KEY, UI_MENU_KEY_SELECT },
        { UI_SCREEN_MENU, UI_BTN_KEY, L, UI_DO_MENU_KEY, UI_MENU_KEY_ESCAPE },
        { UI_SCREEN_MENU, UI_BTN_BACK, S, UI_DO_MENU_KEY, UI_MENU_KEY_ESCAPE },
        { UI_SCREEN_MENU, UI_BTN_BACK, D, UI_DO_MENU_KEY, UI_MENU_KEY_ESCAPE },
        { UI_SCREEN_MENU, UI_BTN_BACK, L, UI_DO_MENU_KEY, UI_MENU_KEY_ESCAPE },
        { UI_SCREEN_CONFIG, UI_BTN_KEY, S, UI_DO_CONFIG_KEY, 0 },
        { UI_SCREEN_CONFIG, UI_BTN_KEY, L, UI_DO_CONFIG_EXIT, 0 },
        { UI_SCREEN_CONFIG, UI_BTN_BACK, S, UI_DO_CONFIG_EXIT, 0 },
        { UI_SCREEN_CONFIG, UI_BTN_BACK, D, UI_DO_CONFIG_EXIT, 0 },
        { UI_SCREEN_CONFIG, UI_BTN_BACK, L, UI_DO_CONFIG_EXIT, 0 },
        { UI_SCREEN_FIRST_RUN, UI_BTN_KEY, S, UI_DO_FIRST_RUN_CONTINUE, 0 },
        { UI_SCREEN_FIRST_RUN, UI_BTN_KEY, L, UI_DO_FIRST_RUN_MENU, 0 },
        { UI_SCREEN_FIRST_RUN, UI_BTN_BOOT, L, UI_DO_CONFIG_ENTER, 0 },
        { UI_SCREEN_CRITICAL, UI_BTN_KEY, S, UI_DO_RESAMPLE, 0 },
        { UI_SCREEN_CRITICAL, UI_BTN_KEY, D, UI_DO_RESAMPLE, 0 },
        { UI_SCREEN_CRITICAL, UI_BTN_KEY, L, UI_DO_RESAMPLE, 0 },
        { UI_SCREEN_CRITICAL, UI_BTN_BOOT, S, UI_DO_RESAMPLE, 0 },
        { UI_SCREEN_CRITICAL, UI_BTN_BOOT, D, UI_DO_RESAMPLE, 0 },
        { UI_SCREEN_CRITICAL, UI_BTN_BOOT, L, UI_DO_RESAMPLE, 0 },
        { UI_SCREEN_CRITICAL, UI_BTN_DOWN, S, UI_DO_RESAMPLE, 0 },
        { UI_SCREEN_CRITICAL, UI_BTN_DOWN, D, UI_DO_RESAMPLE, 0 },
        { UI_SCREEN_CRITICAL, UI_BTN_DOWN, L, UI_DO_RESAMPLE, 0 },
        { UI_SCREEN_CRITICAL, UI_BTN_BACK, S, UI_DO_RESAMPLE, 0 },
        { UI_SCREEN_CRITICAL, UI_BTN_BACK, D, UI_DO_RESAMPLE, 0 },
        { UI_SCREEN_CRITICAL, UI_BTN_BACK, L, UI_DO_RESAMPLE, 0 },
    };
    check(rows, (int)(sizeof(rows) / sizeof(rows[0])), 4);
}

/* The hint lines name the board's own buttons, and the T5's fit its hint line. */
static void test_the_t5_hints_name_its_four_buttons(void)
{
    static const lang_str_t two[] = { LS_HINT_BROWSE, LS_HINT_EDIT, LS_HINT_DATETIME, LS_HINT_CONFIRM, LS_HINT_CONFIG,
                                      LS_HINT_CONFIG_SWITCH, LS_HINT_CONFIG_BACK, LS_F_WIFI, LS_F_MENU, LS_F_CONTINUE };
    static const lang_str_t four[] = { LS_HINT4_BROWSE, LS_HINT4_EDIT, LS_HINT4_DATETIME, LS_HINT4_CONFIRM,
                                       LS_HINT4_CONFIG, LS_HINT4_CONFIG_SWITCH, LS_HINT4_CONFIG_BACK, LS_F4_WIFI,
                                       LS_F4_MENU, LS_F4_CONTINUE };
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL_INT(two[i], ui_hint(two[i])); /* the RLCD */
    }
    ui_profile_use(&ui_profile_t547);
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL_INT(four[i], ui_hint(two[i]));
    }
    TEST_ASSERT_EQUAL_INT(LS_MENU, ui_hint(LS_MENU)); /* not a hint: as it is */
    const char *packs[] = { "en", "cs" };
    for (int l = 0; l < 2; l++) {
        for (int i = 0; i < 7; i++) { /* the menu's and config mode's hint lines: one line on the T5 */
            const char *t = lang_str(lang_get(packs[l]), four[i]);
            TEST_ASSERT_NOT_NULL(t);
            TEST_ASSERT_TRUE_MESSAGE(gfx_text_width(UI_FONT(UI_F_SANS_12), t) <= 960 - UI_PX(12), t);
        }
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_rlcd_controls_are_upstreams);
    RUN_TEST(test_the_t5_controls_follow_dt13);
    RUN_TEST(test_the_t5_hints_name_its_four_buttons);
    return UNITY_END();
}
