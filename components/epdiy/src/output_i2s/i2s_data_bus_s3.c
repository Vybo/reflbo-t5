/*
 * reflbo patch P4 (PATCHES.md): the I2S output path's data bus on the ESP32-S3, for boards whose panel
 * latch sits on a shift register (LilyGo T5-ePaper-S3 V2.3). Implements i2s_data_bus.h with LCD_CAM in
 * i80 mode through esp_lcd: one DMA transfer a row, double-buffered. The start pulse (STH) is the i80
 * bus's DC line: high for two command cycles, then low while the row is clocked out. The pixel clock,
 * the data pins' order and the STH scheme are those of the vendor's working ESP32-S3 support; the pins'
 * order is also epdiy's ESP32 bus's.
 *
 * Part of epdiy, licensed under the GNU Lesser General Public License v3.0 or later (LICENSE).
 */
#include "../output_common/render_method.h"

#ifdef CONFIG_EPD_S3_SHIFT_REGISTER_LATCH

#include "i2s_data_bus.h"

#include <assert.h>
#include <esp_heap_caps.h>
#include <esp_lcd_io_i80.h>
#include <esp_lcd_panel_io.h>
#include <esp_log.h>
#include <esp_rom_gpio.h>
#include <soc/lcd_periph.h>

#define PIXEL_CLOCK_HZ (10 * 1000 * 1000)
#define ROW_ALIGN 64     /* each row buffer's address and length: DMA alignment with room to spare */
#define STH_CMD_BITS 16  /* two bus cycles with STH (DC) high before each row's data */
#define I80_BUS_ID 0     /* the S3 has one i80 bus */

static const char* TAG = "epdiy_s3_bus";

static esp_lcd_i80_bus_handle_t bus;
static esp_lcd_panel_io_handle_t io;
static uint8_t* bufs[2];
static size_t row_bytes;
static int current;
static volatile bool row_done = true;

static bool IRAM_ATTR on_row_done(
    esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t* edata, void* user_ctx
) {
    (void)panel_io;
    (void)edata;
    (void)user_ctx;
    row_done = true;
    return false;
}

/* The data pins in the bus's bit order: each 2-bit pixel's pins swapped pairwise from the end, as
 * epdiy's ESP32 bus maps them to its I2S signals. */
static void bus_pins(const i2s_bus_config* cfg, int out[8]) {
    const int order[8] = { cfg->data_6, cfg->data_7, cfg->data_4, cfg->data_5,
                           cfg->data_2, cfg->data_3, cfg->data_0, cfg->data_1 };
    for (int i = 0; i < 8; i++) {
        out[i] = order[i];
    }
}

void i2s_bus_init(i2s_bus_config* cfg, uint32_t epd_row_width) {
    row_bytes = ((epd_row_width / 4) + ROW_ALIGN - 1) / ROW_ALIGN * ROW_ALIGN;
    for (int i = 0; i < 2; i++) {
        bufs[i] = heap_caps_aligned_calloc(ROW_ALIGN, 1, row_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        assert(bufs[i] != NULL);
    }
    int data[8];
    bus_pins(cfg, data);
    esp_lcd_i80_bus_config_t bus_config = {
        .dc_gpio_num = cfg->start_pulse,
        .wr_gpio_num = cfg->clock,
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .data_gpio_nums = { data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7] },
        .bus_width = 8,
        .max_transfer_bytes = row_bytes,
        .dma_burst_size = ROW_ALIGN,
    };
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &bus));
    esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = -1,
        .pclk_hz = PIXEL_CLOCK_HZ,
        .trans_queue_depth = 2,
        .on_color_trans_done = on_row_done,
        .user_ctx = NULL,
        .lcd_cmd_bits = STH_CMD_BITS,
        .lcd_param_bits = 0,
        .dc_levels = {
            .dc_idle_level = 0,
            .dc_cmd_level = 1,
            .dc_dummy_level = 0,
            .dc_data_level = 0,
        },
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(bus, &io_config, &io));
    current = 0;
    row_done = true;
    ESP_LOGI(TAG, "i80 bus up: %u bytes a row", (unsigned)row_bytes);
}

void i2s_gpio_attach(i2s_bus_config* cfg) {
    int data[8];
    bus_pins(cfg, data);
    for (int i = 0; i < 8; i++) {
        gpio_set_direction(data[i], GPIO_MODE_OUTPUT);
        esp_rom_gpio_connect_out_signal(data[i], lcd_periph_i80_signals.buses[I80_BUS_ID].data_sigs[i], false, false);
    }
    gpio_set_direction(cfg->clock, GPIO_MODE_OUTPUT);
    esp_rom_gpio_connect_out_signal(cfg->clock, lcd_periph_i80_signals.buses[I80_BUS_ID].wr_sig, false, false);
    gpio_set_direction(cfg->start_pulse, GPIO_MODE_OUTPUT);
    esp_rom_gpio_connect_out_signal(cfg->start_pulse, lcd_periph_i80_signals.buses[I80_BUS_ID].dc_sig, false, false);
}

void i2s_gpio_detach(i2s_bus_config* cfg) {
    int data[8];
    bus_pins(cfg, data);
    for (int i = 0; i < 8; i++) {
        gpio_set_direction(data[i], GPIO_MODE_INPUT);
    }
    gpio_set_direction(cfg->clock, GPIO_MODE_INPUT);
    gpio_set_direction(cfg->start_pulse, GPIO_MODE_INPUT);
}

uint8_t* IRAM_ATTR i2s_get_current_buffer() {
    return bufs[current];
}

bool IRAM_ATTR i2s_is_busy() {
    return !row_done;
}

/* One row is in flight at most (render_i2s.c waits for i2s_is_busy() before the next), so the other
 * buffer is free as soon as this one starts. */
void IRAM_ATTR i2s_switch_buffer() {
    current = !current;
}

void i2s_start_line_output() {
    row_done = false;
    esp_err_t err = esp_lcd_panel_io_tx_color(io, 0, bufs[current], row_bytes);
    if (err != ESP_OK) {
        row_done = true;
        ESP_LOGE(TAG, "row: %s", esp_err_to_name(err));
    }
}

void i2s_bus_deinit() {
    if (io != NULL) {
        esp_lcd_panel_io_del(io);
        io = NULL;
    }
    if (bus != NULL) {
        esp_lcd_del_i80_bus(bus);
        bus = NULL;
    }
    for (int i = 0; i < 2; i++) {
        heap_caps_free(bufs[i]);
        bufs[i] = NULL;
    }
}

#endif
