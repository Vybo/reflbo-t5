#include "epaper_board.h"

#include "board_pins.h"
#include "driver/gpio.h"
#include "epaper_sr.h"
#include "esp_check.h"
#include "esp_rom_sys.h"
#include "hal/gpio_ll.h"
#include "output_i2s/i2s_data_bus.h"
#include "output_i2s/rmt_pulse.h"
#include "soc/gpio_struct.h"

#define DUMMY_PIXELS 32 /* zeros after each row: timing headroom, as epdiy's ESP32 T5 board sends */

static const char *TAG = "epaper";

static epaper_sr_t s_sr; /* what the shift register holds */

static i2s_bus_config s_bus = {
    .clock = BOARD_PIN_EPD_CKH,
    .start_pulse = BOARD_PIN_EPD_STH,
    .data_0 = BOARD_PIN_EPD_D0,
    .data_1 = BOARD_PIN_EPD_D1,
    .data_2 = BOARD_PIN_EPD_D2,
    .data_3 = BOARD_PIN_EPD_D3,
    .data_4 = BOARD_PIN_EPD_D4,
    .data_5 = BOARD_PIN_EPD_D5,
    .data_6 = BOARD_PIN_EPD_D6,
    .data_7 = BOARD_PIN_EPD_D7,
};

/* Shifts s_sr out, first bit first, and latches it on the strobe's rising edge. */
static void push(void)
{
    uint8_t word = epaper_sr_word(&s_sr);
    gpio_ll_set_level(&GPIO, BOARD_PIN_SR_STR, 0);
    for (int bit = 7; bit >= 0; bit--) {
        gpio_ll_set_level(&GPIO, BOARD_PIN_SR_CLK, 0);
        gpio_ll_set_level(&GPIO, BOARD_PIN_SR_DATA, (word >> bit) & 1);
        gpio_ll_set_level(&GPIO, BOARD_PIN_SR_CLK, 1);
    }
    gpio_ll_set_level(&GPIO, BOARD_PIN_SR_STR, 1);
}

static void run(const epaper_sr_step_t *steps, int count)
{
    for (int i = 0; i < count; i++) {
        s_sr = steps[i].state;
        push();
        if (steps[i].wait_us > 0) {
            esp_rom_delay_us(steps[i].wait_us);
        }
    }
}

static void sr_pins_out(void)
{
    gpio_config_t io = {
        .pin_bit_mask = BIT64(BOARD_PIN_SR_DATA) | BIT64(BOARD_PIN_SR_CLK) | BIT64(BOARD_PIN_SR_STR),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
}

static void board_init(uint32_t epd_row_width, const EpdInitConfig *config)
{
    (void)config;
    sr_pins_out();
    s_sr = epaper_sr_off();
    push();
    i2s_bus_init(&s_bus, epd_row_width + DUMMY_PIXELS);
    rmt_pulse_init(BOARD_PIN_EPD_CKV);
}

static void board_deinit(void)
{
    epaper_board_idle();
}

/* STH is the i80 bus's DC line (PATCHES.md P4): ep_sth needs nothing here. */
static void board_set_ctrl(epd_ctrl_state_t *state, const epd_ctrl_state_t *const mask)
{
    if (mask->ep_output_enable || mask->ep_mode || mask->ep_stv || mask->ep_latch_enable) {
        s_sr.output_enable = state->ep_output_enable;
        s_sr.mode = state->ep_mode;
        s_sr.stv = state->ep_stv;
        s_sr.latch_enable = state->ep_latch_enable;
        push();
    }
}

static void board_poweron(epd_ctrl_state_t *state)
{
    i2s_gpio_attach(&s_bus);
    epaper_sr_step_t steps[EPAPER_SR_POWER_STEPS];
    run(steps, epaper_sr_poweron(s_sr, steps));
    state->ep_stv = true;
    state->ep_sth = true;
}

static void board_poweroff(epd_ctrl_state_t *state)
{
    epaper_sr_step_t steps[EPAPER_SR_POWER_STEPS];
    run(steps, epaper_sr_poweroff(s_sr, steps));
    state->ep_stv = false;
    state->ep_output_enable = false;
    state->ep_mode = false;
    i2s_gpio_detach(&s_bus);
}

const EpdBoardDefinition epaper_board_t5s3 = {
    .init = board_init,
    .deinit = board_deinit,
    .set_ctrl = board_set_ctrl,
    .poweron = board_poweron,
    .poweroff = board_poweroff,
    .measure_vcom = NULL,
    .set_vcom = NULL,        /* VCOM is the trimpot on the board (T5 spec §2.5) */
    .get_temperature = NULL, /* no sensor; the ED047TC1 waveform has one range (T5 spec §2.5) */
};

void epaper_board_idle(void)
{
    sr_pins_out();
    s_sr = epaper_sr_off();
    push();
    gpio_set_level(BOARD_PIN_SR_CLK, 0);
    gpio_set_level(BOARD_PIN_SR_DATA, 0);
    gpio_sleep_sel_dis(BOARD_PIN_SR_CLK); /* light sleep keeps them driven (AGENTS.md gotcha 19) */
    gpio_sleep_sel_dis(BOARD_PIN_SR_DATA);
    gpio_set_direction(BOARD_PIN_SR_STR, GPIO_MODE_INPUT); /* its 10k pull-up keeps the strobe high */
}

esp_err_t epaper_board_prepare_deep_sleep(void)
{
    ESP_RETURN_ON_ERROR(gpio_hold_en(BOARD_PIN_SR_CLK), TAG, "hold clock");
    ESP_RETURN_ON_ERROR(gpio_hold_en(BOARD_PIN_SR_DATA), TAG, "hold data");
    return ESP_OK;
}

void epaper_board_release_hold(void)
{
    gpio_hold_dis(BOARD_PIN_SR_CLK);
    gpio_hold_dis(BOARD_PIN_SR_DATA);
}
