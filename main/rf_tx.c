/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "applog.h"
#include "rf_wave.h"
#include "status_led.h"
#include "rf_tx.h"

static const char *TAG = "rf";

#define RF_MAX_SYMBOLS 2048          /* longest command: Brightness-low, 100 frames = 1900 symbols */

_Static_assert(sizeof(rf_sym_t) == sizeof(rmt_symbol_word_t), "rf_sym_t must match rmt_symbol_word_t");

typedef struct {
    uint8_t cmd;
    uint8_t src;
} rf_job_t;

static rmt_channel_handle_t s_chan;
static rmt_encoder_handle_t s_copy;
static QueueHandle_t        s_queue;
static SemaphoreHandle_t    s_lock;            /* status snapshot vs worker */
static SemaphoreHandle_t    s_busy;            /* held while a job transmits */
static volatile bool        s_stopping;
static rf_press_fn          s_press;

/* Worker-owned; copied out under s_lock */
static rf_sym_t       s_sym[RF_MAX_SYMBOLS];   /* internal RAM: the RMT ISR reads it with cache off */
static rf_wave_t      s_wave;
static rf_cmd_state_t s_rf;
static devstate_t     s_dev;
static uint32_t       s_sent, s_dropped, s_failed;
static int            s_last_cmd = -1;
static uint32_t       s_last_s;
static bool           s_ready;

static uint32_t uptime_s(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000000);
}

/* The symbol layout must equal the RMT word layout bit for bit. */
static bool layout_ok(void)
{
    rf_sym_t          a = { .val = 0 };
    rmt_symbol_word_t b = { .val = 0 };
    a.d0 = 11520; a.l0 = 0; a.d1 = 320; a.l1 = 1;
    b.duration0 = 11520; b.level0 = 0; b.duration1 = 320; b.level1 = 1;
    return a.val == b.val;
}

static bool transmit(void)
{
    size_t n = rf_wave_symbols(&s_wave);
    if (n == 0) return false;
    rmt_transmit_config_t tc = {
        .loop_count     = 0,
        .flags.eot_level = 0,                  /* the line idles low, as after digitalWrite(LOW) */
    };
    int timeout_ms = (int)(s_wave.total_us / 1000) + 500;
    esp_err_t err = rmt_transmit(s_chan, s_copy, s_sym, n * sizeof(rf_sym_t), &tc);
    if (err == ESP_OK) err = rmt_tx_wait_all_done(s_chan, timeout_ms);
    if (err != ESP_OK) ESP_LOGE(TAG, "RMT transmit failed: %s", esp_err_to_name(err));
    return err == ESP_OK;
}

static void worker(void *arg)
{
    char log[160];
    esp_task_wdt_add(NULL);
    for (;;) {
        rf_job_t job;
        esp_task_wdt_reset();
        if (xQueueReceive(s_queue, &job, pdMS_TO_TICKS(1000)) != pdTRUE) continue;
        esp_task_wdt_reset();
        if (s_stopping) continue;
        xSemaphoreTake(s_busy, portMAX_DELAY);
        status_led_flash();

        xSemaphoreTake(s_lock, portMAX_DELAY);
        rf_wave_reset(&s_wave);
        rf_cmd_send(&s_rf, job.cmd, log, sizeof(log));        /* the sketch's send sequence */
        devstate_apply(&s_dev, job.cmd);
        devstate_t snap = s_dev;
        xSemaphoreGive(s_lock);

        bool ok = false;
        if (s_wave.overflow) applog("RF waveform too long for %s, not sent", rf_cmd_info(job.cmd)->name);
        else if (!(ok = transmit())) applog("RF transmit failed for %s", rf_cmd_info(job.cmd)->name);
        else applog("%s", log);                                /* as 2.3: after the transmission */
        xSemaphoreTake(s_lock, portMAX_DELAY);
        if (ok) s_sent++;
        else    s_failed++;
        s_last_cmd = job.cmd;
        s_last_s   = uptime_s();
        xSemaphoreGive(s_lock);
        if (s_press) s_press(job.cmd, &snap);
        xSemaphoreGive(s_busy);
    }
}

esp_err_t rf_tx_start(void)
{
    s_lock  = xSemaphoreCreateMutex();
    s_busy  = xSemaphoreCreateMutex();
    s_queue = xQueueCreate(RF_QUEUE_LEN, sizeof(rf_job_t));
    if (!s_lock || !s_busy || !s_queue) return ESP_ERR_NO_MEM;
    rf_wave_init(&s_wave, s_sym, RF_MAX_SYMBOLS);
    rf_cmd_init(&s_rf, rf_wave_out, &s_wave);
    devstate_init(&s_dev);

    if (!layout_ok()) {
        ESP_LOGE(TAG, "RMT symbol layout differs; RF disabled");
        return ESP_ERR_INVALID_STATE;
    }
    rmt_tx_channel_config_t cfg = {
        .gpio_num          = rf_tx_gpio,
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .resolution_hz     = 1000000,          /* 1 µs per tick, as delayMicroseconds() */
        .mem_block_symbols = 48,               /* one block: the LED strip needs the other channel */
        .trans_queue_depth = 2,
        .flags.init_level  = 0,                /* idle low */
    };
    esp_err_t err = rmt_new_tx_channel(&cfg, &s_chan);
    if (err == ESP_OK) {
        rmt_copy_encoder_config_t ec = {};
        err = rmt_new_copy_encoder(&ec, &s_copy);
    }
    if (err == ESP_OK) err = rmt_enable(s_chan);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "RMT TX on GPIO%d failed: %s; RF disabled", rf_tx_gpio, esp_err_to_name(err));
        return err;
    }
    if (xTaskCreate(worker, "rf_tx", 4096, NULL, 6, NULL) != pdPASS) return ESP_ERR_NO_MEM;
    s_ready = true;
    ESP_LOGI(TAG, "433 MHz transmitter on GPIO%d (RMT, 1 us ticks), Novy channel %d", rf_tx_gpio,
             rf_cmd_novy_channel());
    if (rf_codes_example)
        ESP_LOGW(TAG, "Example RF codes (main/rf_codes_example.h): the ceiling fan will not respond, see README");
    return ESP_OK;
}

void rf_tx_set_press_handler(rf_press_fn fn)
{
    s_press = fn;
}

bool rf_tx_submit(int cmd, rf_src_t src)
{
    rf_job_t job = { .cmd = (uint8_t)cmd, .src = (uint8_t)src };
    if (!rf_cmd_info(cmd) || !s_ready || s_stopping) return false;
    if (xQueueSend(s_queue, &job, 0) != pdTRUE) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_dropped++;
        xSemaphoreGive(s_lock);
        applog("RF queue full, %s dropped", rf_cmd_info(cmd)->name);
        return false;
    }
    return true;
}

void rf_tx_get_status(rf_tx_status_t *out)
{
    memset(out, 0, sizeof(*out));
    out->ready = s_ready;
    out->gpio  = rf_tx_gpio;
    if (!s_lock) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    out->sent            = s_sent;
    out->dropped         = s_dropped;
    out->failed          = s_failed;
    out->repeat_transmit = s_rf.rcs.repeat_transmit;
    memcpy(out->fan_cycle, s_rf.fan_cycle, sizeof(out->fan_cycle));
    out->state         = s_dev;
    out->last_cmd      = s_last_cmd;
    out->last_uptime_s = s_last_s;
    xSemaphoreGive(s_lock);
    out->queued = s_queue ? (int)uxQueueMessagesWaiting(s_queue) : 0;
}

void rf_tx_restart_hook(void)
{
    s_stopping = true;
    if (s_busy && xSemaphoreTake(s_busy, pdMS_TO_TICKS(4000)) == pdTRUE) xSemaphoreGive(s_busy);
}
