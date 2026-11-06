/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "evlog.h"
#include "applog.h"

static const char *TAG = "log";

static evlog_t           s_log;
static SemaphoreHandle_t s_mutex;

void applog_init(void)
{
    evlog_init(&s_log);
    s_mutex = xSemaphoreCreateMutex();
}

void applog(const char *fmt, ...)
{
    char    msg[EVLOG_LINE_MAX];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    ESP_LOGI(TAG, "%s", msg);
    if (!s_mutex) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    evlog_add(&s_log, (uint32_t)(esp_timer_get_time() / 1000000), msg);
    xSemaphoreGive(s_mutex);
}

void applog_clear(void)
{
    if (!s_mutex) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    evlog_clear(&s_log);
    xSemaphoreGive(s_mutex);
}

char *applog_dump(size_t *len)
{
    size_t cap = EVLOG_LINES * (EVLOG_LINE_MAX + 1) + 1;
    char  *buf = malloc(cap);
    if (!buf || !s_mutex) {
        free(buf);
        return NULL;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *len = evlog_dump(&s_log, buf, cap);
    xSemaphoreGive(s_mutex);
    return buf;
}
