#include "diag.h"

#include <fcntl.h>
#include <stdio.h>

#include "diag_internal.h"
#include "sdkconfig.h"
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#else
#include "driver/uart.h"
#include "driver/uart_vfs.h"
#endif
#include "power.h"
#include "esp_check.h"
#include "esp_console.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "linenoise/linenoise.h"

#define DIAG_MAX_CMDLINE_LEN 256
#define DIAG_REPL_STACK_SIZE 4096
#define DIAG_REPL_PRIORITY   1
#define DIAG_REPL_CORE       tskNO_AFFINITY
#define DIAG_INPUT_AWAKE_MS  300000 /* a UART console can't see the PC: input keeps the board awake (T5 spec §9) */

static const char *TAG = "diag";

/*
 * A hand-built REPL instead of esp_console_new_repl_usb_serial_jtag(). That one probes the
 * terminal for escape-sequence support at start-up. With no terminal attached, nobody answers,
 * which stalls boot for about 1 s. Once a real terminal has answered, the console keeps sending
 * cursor-position queries that a script never answers, so tools/devlog.py wedges it.
 * Plain line mode ("dumb mode") gives every client, human or script, the same behaviour.
 */
static diag_executor_t s_executor;

void diag_set_executor(diag_executor_t executor)
{
    s_executor = executor;
}

typedef struct {
    int (*body)(int argc, char **argv);
    int argc;
    char **argv;
    int ret;
} diag_call_t;

static void call_body(void *arg)
{
    diag_call_t *call = arg;
    call->ret = call->body(call->argc, call->argv);
}

int diag_on_owner(int (*body)(int argc, char **argv), int argc, char **argv)
{
    diag_call_t call = { .body = body, .argc = argc, .argv = argv, .ret = 1 };
    if (s_executor == NULL) {
        call_body(&call);
        return call.ret;
    }
    esp_err_t err = s_executor(call_body, &call);
    if (err != ESP_OK) {
        printf("%s: %s\n", argv[0], esp_err_to_name(err));
        return 1;
    }
    return call.ret;
}

#if !CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
static int hold_awake_body(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    power_hold_awake_ms(DIAG_INPUT_AWAKE_MS);
    return 0;
}
#endif

static void diag_repl_task(void *arg)
{
    (void)arg;
    setvbuf(stdin, NULL, _IONBF, 0); /* stdin buffering is per task */

    for (;;) {
        char *line = linenoise(DIAG_PROMPT);
        if (line == NULL) {
            vTaskDelay(pdMS_TO_TICKS(10)); /* input unavailable, e.g. the USB host went away */
            continue;
        }
#if !CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
        static char *const k_hold_argv[] = { "console", NULL };
        diag_on_owner(hold_awake_body, 1, (char **)k_hold_argv); /* on the app task, as the power module wants */
#endif
        int ret = 0;
        esp_err_t err = esp_console_run(line, &ret);
        if (err == ESP_ERR_NOT_FOUND) {
            printf("unknown command: %s\n", line);
        } else if (err == ESP_OK && ret != 0) {
            printf("command returned %d\n", ret);
        } else if (err != ESP_OK && err != ESP_ERR_INVALID_ARG) { /* INVALID_ARG: empty line */
            printf("console error: %s\n", esp_err_to_name(err));
        }
        linenoiseFree(line);
    }
}

esp_err_t diag_start(void)
{
    /* Enter sends CR; print CRLF for '\n'. Blocking stdin and stdout. */
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    usb_serial_jtag_vfs_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
    usb_serial_jtag_vfs_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);
#else
    uart_vfs_dev_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CR);
    uart_vfs_dev_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CRLF);
#endif
    fcntl(fileno(stdout), F_SETFL, 0);
    fcntl(fileno(stdin), F_SETFL, 0);

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    usb_serial_jtag_driver_config_t usj_config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(usb_serial_jtag_driver_install(&usj_config), TAG, "USB-Serial-JTAG driver install failed");
    usb_serial_jtag_vfs_use_driver();
#else
    const uart_config_t uart_config = {
        .baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_RETURN_ON_ERROR(uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0), TAG, "UART driver");
    ESP_RETURN_ON_ERROR(uart_param_config(CONFIG_ESP_CONSOLE_UART_NUM, &uart_config), TAG, "UART config");
    uart_vfs_dev_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
#endif

    esp_console_config_t console_config = ESP_CONSOLE_CONFIG_DEFAULT();
    console_config.max_cmdline_length = DIAG_MAX_CMDLINE_LEN;
    ESP_RETURN_ON_ERROR(esp_console_init(&console_config), TAG, "console init failed");
    linenoiseSetDumbMode(1);
    linenoiseSetMaxLineLen(DIAG_MAX_CMDLINE_LEN);

    ESP_RETURN_ON_ERROR(esp_console_register_help_command(), TAG, "help command");
    ESP_RETURN_ON_ERROR(diag_register_system_commands(), TAG, "system commands");
    ESP_RETURN_ON_ERROR(diag_register_display_commands(), TAG, "display commands");
    ESP_RETURN_ON_ERROR(diag_register_button_commands(), TAG, "button commands");
    ESP_RETURN_ON_ERROR(diag_register_sensor_commands(), TAG, "sensor commands");
    ESP_RETURN_ON_ERROR(diag_register_power_commands(), TAG, "power commands");

    BaseType_t created = xTaskCreatePinnedToCore(diag_repl_task, "diag_repl", DIAG_REPL_STACK_SIZE, NULL,
                                                 DIAG_REPL_PRIORITY, NULL, DIAG_REPL_CORE);
    ESP_RETURN_ON_FALSE(created == pdPASS, ESP_ERR_NO_MEM, TAG, "REPL task create failed");
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    ESP_LOGI(TAG, "console ready on USB-Serial-JTAG");
#else
    ESP_LOGI(TAG, "console ready on UART%d", CONFIG_ESP_CONSOLE_UART_NUM);
#endif
    return ESP_OK;
}
