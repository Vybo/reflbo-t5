#pragma once

/* LilyGo T5-4.7, the ESP32 board, revision V2.3 (T5 spec §2.2: the vendor's ESP32 schematic, utilities.h
 * and ed047tc1.h; epdiy's epd_board_lilygo_t5_47.c, which drives the panel's own pins). */

/* The side buttons in the board's order after RESET: Back (IO0), Up (IO35), Enter (IO34), Down (IO39) (DT13). */
#define BOARD_PIN_KEY     34 /* Enter: input only, external 100k pull-up; ext0 wake */
#define BOARD_PIN_BOOT    35 /* Up: input only, external 100k pull-up; ext1 wake */
#define BOARD_PIN_DOWN    39 /* Down: input only, external 100k pull-up; can't wake the board */
#define BOARD_PIN_BACK    0  /* Back: the shift register's strobe too, read only between updates (BOARD_PIN_SR_STR) */
#define BOARD_PIN_I2C_SDA 15 /* the touch/Grove connector's bus; nothing on it in the fork */
#define BOARD_PIN_I2C_SCL 14
#define BOARD_PIN_BAT_ADC 36 /* ADC1_CH0 = VBAT x 1/2, switched with POWER_EN or VBUS (T5 spec §2.5 gotcha 24) */

/* The panel's 74HCT4094 (T5 spec §2.3): epdiy drives it during updates; the fork rests it at boot and
 * holds its lines through deep sleep (§5.4). */
#define BOARD_PIN_SR_DATA 23
#define BOARD_PIN_SR_CLK  18
#define BOARD_PIN_SR_STR  0 /* also the Back button (DT13) */

/* Battery: VBAT through a 100k/100k divider. ADC1, so Wi-Fi doesn't get in the way. */
#define BOARD_BAT_ADC_UNIT    ADC_UNIT_1
#define BOARD_BAT_ADC_CHANNEL ADC_CHANNEL_0 /* GPIO36 */
#define BOARD_BAT_DIVIDER     2
