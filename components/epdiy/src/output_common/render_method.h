#pragma once

#include "sdkconfig.h"

/**
 * Rendering Method / Hardware to use.
 */
enum EpdRenderMethod {
    /// Use the I2S peripheral on ESP32 chips.
    RENDER_METHOD_I2S = 1,
    /// Use the CAM/LCD peripheral in ESP32-S3 chips.
    RENDER_METHOD_LCD = 2,
};

extern const enum EpdRenderMethod EPD_CURRENT_RENDER_METHOD;

/* reflbo patch P2 (PATCHES.md): an ESP32-S3 whose latch is on a shift register uses the I2S path. */
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_EPD_S3_SHIFT_REGISTER_LATCH)
#define RENDER_METHOD_I2S 1
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#define RENDER_METHOD_LCD 1
#else
#error "unknown chip, cannot choose render method!"
#endif

#ifdef __clang__
#define IRAM_ATTR
// define this if we're using clangd to make it accept the GCC builtin
void __assert_func(const char* file, int line, const char* func, const char* failedexpr);
#endif