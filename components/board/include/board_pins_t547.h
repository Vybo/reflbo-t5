#pragma once

/* LilyGo T5-4.7, the ESP32 board, revision V2.3 (T5 spec §2.2: the vendor's ESP32 schematic, utilities.h
 * and ed047tc1.h; epdiy's epd_board_lilygo_t5_47.c, which drives the panel's own pins). */

#define BOARD_PIN_KEY     34 /* side button: input only, external 100k pull-up (DT7) */
#define BOARD_PIN_BOOT    35 /* side button in BOOT's role: input only, external 100k pull-up (DT7) */
#define BOARD_PIN_SPARE   39 /* the third side button: unused */
#define BOARD_PIN_I2C_SDA 15 /* the touch/Grove connector's bus; nothing on it in the fork */
#define BOARD_PIN_I2C_SCL 14
#define BOARD_PIN_BAT_ADC 36 /* ADC1_CH0 = VBAT x 1/2 */

/* The panel's 74HCT4094 (T5 spec §2.3): epdiy drives it during updates; the fork rests it at boot and
 * holds its lines through deep sleep (§5.4). */
#define BOARD_PIN_SR_DATA 23
#define BOARD_PIN_SR_CLK  18
#define BOARD_PIN_SR_STR  0 /* also the BOOT button, never read (DT7) */

/* Battery: VBAT through a 100k/100k divider. ADC1, so Wi-Fi doesn't get in the way. */
#define BOARD_BAT_ADC_UNIT    ADC_UNIT_1
#define BOARD_BAT_ADC_CHANNEL ADC_CHANNEL_0 /* GPIO36 */
#define BOARD_BAT_DIVIDER     2
