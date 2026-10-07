#include "display.h"

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "util_crc32.h"

/* T5-ePaper-S3 (T5 spec §5), T0: the API only. The framebuffer has the RLCD's 400×300 at 1 bpp
 * until T3 (T5 spec §11), and nothing reaches the panel until T1 brings epdiy and `epaper`. */

#define FB_W 400
#define FB_H 300

static const char *TAG = "display";

static gfx_fb_t s_fb; /* s_fb.buf stays NULL until display_init */
static uint32_t s_last_crc;
static bool s_pushed;

static esp_err_t alloc_fb(void)
{
    ESP_RETURN_ON_FALSE(s_fb.buf == NULL, ESP_ERR_INVALID_STATE, TAG, "already initialised");
    uint8_t *buf = heap_caps_calloc(1, gfx_fb_size(FB_W, FB_H), MALLOC_CAP_SPIRAM);
    ESP_RETURN_ON_FALSE(buf != NULL, ESP_ERR_NO_MEM, TAG, "framebuffer");
    gfx_fb_init(&s_fb, buf, FB_W, FB_H);
    return ESP_OK;
}

esp_err_t display_init(void)
{
    ESP_RETURN_ON_ERROR(alloc_fb(), TAG, "framebuffer");
    ESP_LOGW(TAG, "no panel driver yet (T1)");
    return ESP_OK;
}

esp_err_t display_init_warm(const display_state_t *state)
{
    ESP_RETURN_ON_ERROR(alloc_fb(), TAG, "framebuffer");
    s_last_crc = state->last_crc;
    s_pushed = state->pushed;
    return ESP_OK;
}

esp_err_t display_init_lost(void)
{
    return display_init();
}

void display_export(display_state_t *out)
{
    *out = (display_state_t){ .last_crc = s_last_crc, .pushed = s_pushed };
}

esp_err_t display_prepare_deep_sleep(void)
{
    return ESP_OK;
}

void display_cancel_deep_sleep(void)
{
}

gfx_fb_t *display_fb(void)
{
    return s_fb.buf != NULL ? &s_fb : NULL;
}

esp_err_t display_sleep(void)
{
    return ESP_OK;
}

esp_err_t display_wake(void)
{
    return ESP_OK;
}

bool display_asleep(void)
{
    return false;
}

esp_err_t display_commit(bool force)
{
    ESP_RETURN_ON_FALSE(s_fb.buf != NULL, ESP_ERR_INVALID_STATE, TAG, "not initialised");
    uint32_t crc = util_crc32(0, s_fb.buf, gfx_fb_size(FB_W, FB_H));
    if (!force && s_pushed && crc == s_last_crc) {
        return ESP_OK;
    }
    s_last_crc = crc; /* T1: the panel update goes here */
    s_pushed = true;
    return ESP_OK;
}

esp_err_t display_set_fast(bool fast)
{
    (void)fast;
    return ESP_OK;
}

esp_err_t display_clean(void)
{
    return ESP_OK;
}
