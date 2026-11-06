/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "restart.h"

static const char *TAG = "restart";

#define MAX_HOOKS 8

static void        (*s_hooks[MAX_HOOKS])(void);
static int           s_nhooks;
static volatile bool s_scheduled;

void restart_add_hook(void (*fn)(void))
{
    if (s_nhooks < MAX_HOOKS) s_hooks[s_nhooks++] = fn;
}

static void restart_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(500));          /* let httpd finish the response */
    for (int i = 0; i < s_nhooks; i++) s_hooks[i]();
    vTaskDelay(pdMS_TO_TICKS(500));          /* let lwIP close all sockets */
    ESP_LOGW(TAG, "Restart: now");
    esp_restart();
}

const char *reset_reason_str(void)
{
    switch (esp_reset_reason()) {
    case ESP_RST_POWERON:    return "power-on";
    case ESP_RST_EXT:        return "external";
    case ESP_RST_SW:         return "software";
    case ESP_RST_PANIC:      return "panic";
    case ESP_RST_INT_WDT:    return "interrupt watchdog";
    case ESP_RST_TASK_WDT:   return "task watchdog";
    case ESP_RST_WDT:        return "watchdog";
    case ESP_RST_DEEPSLEEP:  return "deep sleep";
    case ESP_RST_BROWNOUT:   return "brownout";
    case ESP_RST_USB:        return "usb";
    case ESP_RST_JTAG:       return "jtag";
    case ESP_RST_PWR_GLITCH: return "power glitch";
    case ESP_RST_CPU_LOCKUP: return "cpu lockup";
    default:                 return "unknown";
    }
}

void restart_schedule(const char *reason)
{
    if (s_scheduled) return;
    s_scheduled = true;
    ESP_LOGW(TAG, "Restart scheduled: %s", reason);
    xTaskCreate(restart_task, "restart", 4096, NULL, 5, NULL);
}
