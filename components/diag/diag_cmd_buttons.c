#include <stdio.h>
#include <string.h>

#include "board_buttons.h"
#include "diag_internal.h"
#include "esp_console.h"
#include "sdkconfig.h"

static int usage(const char *text)
{
    printf("usage: %s\n", text);
    return 1;
}

/* btn <key|boot> <short|double|long>: runs on the console task; the buttons task reports it. */
static int cmd_btn(int argc, char **argv)
{
#if CONFIG_REFLBO_BOARD_T547
    static const char *const k_usage = "btn <up|enter|down|back|key|boot> <short|double|long>";
#else
    static const char *const k_usage = "btn <key|boot> <short|double|long>";
#endif
    if (argc != 3) {
        return usage(k_usage);
    }
    board_button_t button;
    if (strcmp(argv[1], "key") == 0 || strcmp(argv[1], "enter") == 0) {
        button = BOARD_BUTTON_KEY; /* the T5's Enter is KEY's pin (DT13) */
    } else if (strcmp(argv[1], "boot") == 0 || strcmp(argv[1], "up") == 0) {
        button = BOARD_BUTTON_BOOT;
#if CONFIG_REFLBO_BOARD_T547
    } else if (strcmp(argv[1], "down") == 0) {
        button = BOARD_BUTTON_DOWN;
    } else if (strcmp(argv[1], "back") == 0) {
        button = BOARD_BUTTON_BACK;
#endif
    } else {
        return usage(k_usage);
    }
    gesture_t gesture;
    if (strcmp(argv[2], "short") == 0) {
        gesture = GESTURE_SHORT;
    } else if (strcmp(argv[2], "double") == 0) {
        gesture = GESTURE_DOUBLE;
    } else if (strcmp(argv[2], "long") == 0) {
        gesture = GESTURE_LONG;
    } else {
        return usage(k_usage);
    }
    board_buttons_inject(button, gesture);
    printf("btn: %s %s\n", board_button_name(button), board_gesture_name(gesture));
    return 0;
}

esp_err_t diag_register_button_commands(void)
{
    const esp_console_cmd_t cmds[] = {
        { .command = "btn", .help = "btn <button> <short|double|long>: inject a button gesture (key, boot; the T5 also up, enter, down, back)", .func = &cmd_btn },
    };
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        esp_err_t err = esp_console_cmd_register(&cmds[i]);
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}
