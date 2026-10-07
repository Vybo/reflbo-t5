#include "render_method.h"
#include "sdkconfig.h"

/* reflbo patch P2 (PATCHES.md) */
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_EPD_S3_SHIFT_REGISTER_LATCH)
const enum EpdRenderMethod EPD_CURRENT_RENDER_METHOD = RENDER_METHOD_I2S;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
const enum EpdRenderMethod EPD_CURRENT_RENDER_METHOD = RENDER_METHOD_LCD;
#else
#error "unknown chip, cannot choose render method!"
#endif