#pragma once

/* LilyGo T5-ePaper-S3, revision V2.3 (T5 spec §2.2: the vendor's README pin table and schematic). */

#define BOARD_PIN_BOOT    0  /* BOOT button: active low, external 10k pull-up; also the 74HCT4094's strobe */
#define BOARD_PIN_KEY     21 /* KEY button: active low, external 10k pull-up */
#define BOARD_PIN_I2C_SDA 18 /* the PCF8563; 10k pull-ups */
#define BOARD_PIN_I2C_SCL 17
#define BOARD_PIN_BAT_ADC 14 /* ADC2_CH3 = VBAT x 1/2 */

/* The panel (T1): 8-bit parallel data and its clocks; the 74HCT4094 holds its control lines and rails. */
#define BOARD_PIN_EPD_D0  8
#define BOARD_PIN_EPD_D1  1
#define BOARD_PIN_EPD_D2  2
#define BOARD_PIN_EPD_D3  3
#define BOARD_PIN_EPD_D4  4
#define BOARD_PIN_EPD_D5  5
#define BOARD_PIN_EPD_D6  6
#define BOARD_PIN_EPD_D7  7
#define BOARD_PIN_EPD_CKV 38
#define BOARD_PIN_EPD_STH 40
#define BOARD_PIN_EPD_CKH 41
#define BOARD_PIN_SR_DATA 13
#define BOARD_PIN_SR_CLK  12
#define BOARD_PIN_SR_STR  BOARD_PIN_BOOT

/* Battery: VBAT through a 100k/100k divider. ADC2 is shared with Wi-Fi (T5 spec §2.5). */
#define BOARD_BAT_ADC_UNIT    ADC_UNIT_2
#define BOARD_BAT_ADC_CHANNEL ADC_CHANNEL_3 /* GPIO14 */
#define BOARD_BAT_DIVIDER     2
