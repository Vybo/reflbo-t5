#include "epaper_board.h"

#include "board_pins.h"
#include "driver/gpio.h"
#include "epaper_sr.h"
#include "esp_check.h"

static const char *TAG = "epaper";

/* Shifts the word out, first bit first, and latches it on the strobe's rising edge. */
static void push(uint8_t word)
{
    gpio_set_level(BOARD_PIN_SR_STR, 0);
    for (int bit = 7; bit >= 0; bit--) {
        gpio_set_level(BOARD_PIN_SR_CLK, 0);
        gpio_set_level(BOARD_PIN_SR_DATA, (word >> bit) & 1);
        gpio_set_level(BOARD_PIN_SR_CLK, 1);
    }
    gpio_set_level(BOARD_PIN_SR_STR, 1);
}

/* The strobe back to an input on the chip's pull-up, kept through light sleep. The board has no pull-up of
 * its own on IO0: with USB in, the CH9102's RTS holds it high through the auto-reset transistor; on battery
 * nothing does, and a floating IO0 reads as Back presses (T5 spec §2.5 gotcha 23). */
static void release_strobe(void)
{
    gpio_set_direction(BOARD_PIN_SR_STR, GPIO_MODE_INPUT);
    gpio_pullup_en(BOARD_PIN_SR_STR);
    gpio_sleep_sel_dis(BOARD_PIN_SR_STR);
}

void epaper_board_idle(void)
{
    gpio_config_t io = {
        .pin_bit_mask = BIT64(BOARD_PIN_SR_DATA) | BIT64(BOARD_PIN_SR_CLK) | BIT64(BOARD_PIN_SR_STR),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    epaper_sr_t off = epaper_sr_off();
    push(epaper_sr_word(&off));
    gpio_set_level(BOARD_PIN_SR_CLK, 0);
    gpio_set_level(BOARD_PIN_SR_DATA, 0);
    gpio_sleep_sel_dis(BOARD_PIN_SR_CLK); /* light sleep keeps them driven (AGENTS.md gotcha 19) */
    gpio_sleep_sel_dis(BOARD_PIN_SR_DATA);
    release_strobe();
}

esp_err_t epaper_board_sense(bool on)
{
    ESP_RETURN_ON_FALSE(gpio_get_level(BOARD_PIN_SR_STR) == 1, ESP_ERR_INVALID_STATE, TAG,
                        "IO0 held low: the latch can't take a word");
    gpio_set_level(BOARD_PIN_SR_STR, 1);
    gpio_set_direction(BOARD_PIN_SR_STR, GPIO_MODE_OUTPUT);
    epaper_sr_t word = on ? epaper_sr_sense() : epaper_sr_off();
    push(epaper_sr_word(&word));
    gpio_set_level(BOARD_PIN_SR_CLK, 0);
    gpio_set_level(BOARD_PIN_SR_DATA, 0);
    release_strobe();
    return ESP_OK;
}

esp_err_t epaper_board_prepare_deep_sleep(void)
{
    ESP_RETURN_ON_ERROR(gpio_hold_en(BOARD_PIN_SR_CLK), TAG, "hold clock");
    ESP_RETURN_ON_ERROR(gpio_hold_en(BOARD_PIN_SR_DATA), TAG, "hold data");
    gpio_deep_sleep_hold_en();
    return ESP_OK;
}

void epaper_board_release_hold(void)
{
    gpio_deep_sleep_hold_dis();
    gpio_hold_dis(BOARD_PIN_SR_CLK);
    gpio_hold_dis(BOARD_PIN_SR_DATA);
}
