#include "display.h"

#include <string.h>

#include "epaper_board.h"
#include "epaper_frame.h"
#include "epd_board.h"
#include "epd_highlevel.h"
#include "epdiy.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "gfx_test_pattern.h"
#include "util_crc32.h"

/* LilyGo T5-4.7 (T5 spec §5), T1: every update is a clean one (clear, then GC16; T5 spec §11), through
 * epdiy's own board definition for this board. The UI still draws the RLCD's 400×300 at 1 bpp until T3;
 * it goes in the middle of the 960×540 panel. */

#define FB_W          400
#define FB_H          300
#define PANEL_W       960
#define PANEL_H       540
#define FB_X          ((PANEL_W - FB_W) / 2)
#define FB_Y          ((PANEL_H - FB_H) / 2)
#define TEMPERATURE_C 25 /* the ED047TC1 waveform has one range, 20-30 °C (T5 spec §2.5) */
#define EPD_OPTIONS   (EPD_LUT_64K | EPD_FEED_QUEUE_8)

static const char *TAG = "display";

static gfx_fb_t s_fb; /* s_fb.buf stays NULL until display_init */
static uint32_t s_last_crc;
static bool s_pushed;
static EpdiyHighlevelState s_hl;
static bool s_hl_ready;
static display_t5_stats_t s_stats;

static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static esp_err_t alloc_fb(void)
{
    ESP_RETURN_ON_FALSE(s_fb.buf == NULL, ESP_ERR_INVALID_STATE, TAG, "already initialised");
    uint8_t *buf = heap_caps_calloc(1, gfx_fb_size(FB_W, FB_H), MALLOC_CAP_SPIRAM);
    ESP_RETURN_ON_FALSE(buf != NULL, ESP_ERR_NO_MEM, TAG, "framebuffer");
    gfx_fb_init(&s_fb, buf, FB_W, FB_H);
    return ESP_OK;
}

/* epdiy is up only while the panel updates: its LUT and line queues want ~80 KB of internal RAM, which
 * Wi-Fi needs the rest of the time (T5 spec §5.2). */
static void panel_up(void)
{
    /* epd_init() sets the board each time and warns from the second on; routine wakes print warnings. P5's
     * LUT fallback logs as "epd" and stays. */
    esp_log_level_set("epdiy", ESP_LOG_ERROR);
    epd_init(&epd_board_lilygo_t5_47, &ED047TC1, EPD_OPTIONS);
    if (!s_hl_ready) {
        s_hl = epd_hl_init(NULL); /* the display's own waveform: epdiy_ED047TC1 */
        s_hl_ready = true;
    }
    uint32_t internal = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    if (s_stats.min_internal == 0 || internal < s_stats.min_internal) {
        s_stats.min_internal = internal;
    }
    epd_poweron();
}

static void panel_down(void)
{
    epd_poweroff();
    epd_deinit();
    epaper_board_idle();
}

/* The panel to white from whatever it shows, flashing, with epdiy's buffers to match. */
static void clear_to_white(void)
{
    epd_clear();
    memset(s_hl.back_fb, 0xFF, PANEL_W / 2 * PANEL_H);
    epd_hl_set_all_white(&s_hl);
}

static esp_err_t clean_update(const gfx_fb_t *fb)
{
    int64_t start = now_ms();
    panel_up();
    int64_t up = now_ms();
    clear_to_white();
    int64_t cleared = now_ms();
    epaper_frame_blit_1bpp(fb, epd_hl_get_framebuffer(&s_hl), PANEL_W, PANEL_H, FB_X, FB_Y);
    enum EpdDrawError err = epd_hl_update_screen(&s_hl, MODE_GC16, TEMPERATURE_C);
    int64_t drawn = now_ms();
    panel_down();
    s_stats.updates++;
    s_stats.last_ms = (uint32_t)(now_ms() - start);
    ESP_LOGI(TAG, "clean update: up %lld ms, clear %lld ms, GC16 %lld ms, %lu ms in all; internal RAM %lu free",
             (long long)(up - start), (long long)(cleared - up), (long long)(drawn - cleared),
             (unsigned long)s_stats.last_ms, (unsigned long)s_stats.min_internal);
    ESP_RETURN_ON_FALSE(err == EPD_DRAW_SUCCESS, ESP_FAIL, TAG, "epdiy draw error 0x%x", (unsigned)err);
    return ESP_OK;
}

esp_err_t display_init(void)
{
    ESP_RETURN_ON_ERROR(alloc_fb(), TAG, "framebuffer");
    epaper_board_idle(); /* the shift register powers up random: all off before anything else */
    return display_commit(true); /* a cold boot starts clean (T5 spec §5.3) */
}

esp_err_t display_init_warm(const display_state_t *state)
{
    ESP_RETURN_ON_ERROR(alloc_fb(), TAG, "framebuffer");
    epaper_board_release_hold();
    epaper_board_idle();
    s_last_crc = state->last_crc;
    s_pushed = state->pushed;
    return ESP_OK;
}

esp_err_t display_init_lost(void)
{
    epaper_board_release_hold();
    return display_init();
}

void display_export(display_state_t *out)
{
    *out = (display_state_t){ .last_crc = s_last_crc, .pushed = s_pushed };
}

esp_err_t display_prepare_deep_sleep(void)
{
    return epaper_board_prepare_deep_sleep();
}

void display_cancel_deep_sleep(void)
{
    epaper_board_release_hold();
}

gfx_fb_t *display_fb(void)
{
    return s_fb.buf != NULL ? &s_fb : NULL;
}

/* Night sleep keeps the dashboard on the panel as it is; T4 adds the moon (T5 spec §5.6). */
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
    ESP_RETURN_ON_ERROR(clean_update(&s_fb), TAG, "update");
    s_last_crc = crc;
    s_pushed = true;
    return ESP_OK;
}

esp_err_t display_set_fast(bool fast)
{
    (void)fast; /* T4: fast updates (T5 spec §5.3); every update is clean until then */
    return ESP_OK;
}

esp_err_t display_clean(void)
{
    return display_commit(true);
}

void display_t5_stats(display_t5_stats_t *out)
{
    *out = s_stats;
}

esp_err_t display_t5_bench(display_t5_bench_t *out)
{
    ESP_RETURN_ON_FALSE(s_fb.buf != NULL, ESP_ERR_INVALID_STATE, TAG, "not initialised");
    uint8_t *buf = heap_caps_calloc(1, gfx_fb_size(FB_W, FB_H), MALLOC_CAP_SPIRAM);
    ESP_RETURN_ON_FALSE(buf != NULL, ESP_ERR_NO_MEM, TAG, "pattern");
    gfx_fb_t pattern;
    gfx_fb_init(&pattern, buf, FB_W, FB_H);
    gfx_draw_test_pattern(&pattern);
    uint8_t *front;

    panel_up();
    int64_t t0 = now_ms();
    clear_to_white();
    front = epd_hl_get_framebuffer(&s_hl);
    epaper_frame_blit_1bpp(&pattern, front, PANEL_W, PANEL_H, FB_X, FB_Y);
    int err = (int)epd_hl_update_screen(&s_hl, MODE_GC16, TEMPERATURE_C);
    int64_t t1 = now_ms();
    for (size_t i = 0; i < PANEL_W / 2 * PANEL_H; i++) {
        front[i] = (uint8_t)~front[i];
    }
    err |= (int)epd_hl_update_screen(&s_hl, MODE_GL16, TEMPERATURE_C);
    int64_t t2 = now_ms();
    epaper_frame_blit_1bpp(&pattern, front, PANEL_W, PANEL_H, FB_X, FB_Y);
    err |= (int)epd_hl_update_screen(&s_hl, MODE_DU, TEMPERATURE_C);
    int64_t t3 = now_ms();
    panel_down();

    heap_caps_free(buf);
    s_pushed = false; /* the panel shows the pattern: the next commit puts the frame back */
    *out = (display_t5_bench_t){ (uint32_t)(t1 - t0), (uint32_t)(t2 - t1), (uint32_t)(t3 - t2) };
    ESP_LOGI(TAG, "bench: clean %lu ms, GL16 %lu ms, DU %lu ms", (unsigned long)out->clean_ms,
             (unsigned long)out->gl16_ms, (unsigned long)out->du_ms);
    ESP_RETURN_ON_FALSE(err == EPD_DRAW_SUCCESS, ESP_FAIL, TAG, "epdiy draw error 0x%x", (unsigned)err);
    return ESP_OK;
}
