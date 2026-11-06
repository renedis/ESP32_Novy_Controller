/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_app_desc.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "applog.h"
#include "boot_guard.h"
#include "boot_guard_idf.h"
#include "mqtt_pub.h"
#include "nvs_config.h"
#include "restart.h"
#include "rf_tx.h"
#include "status_led.h"
#include "time_sync.h"
#include "web_server.h"
#include "wifi_mgr.h"

static const char *TAG = "main";

#define HEAP_LOG_S       300            /* 2.3: "Heap - Current ..." every 5 minutes */
#define WIFI_FAIL_LOG_S  80             /* 2.3: "WiFi failed after 5 attempts" (5 x 16 s) */

static uint32_t uptime_s(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000000);
}

void app_main(void)
{
    /* Before anything else, so that no later init can crash ahead of the rollback check. */
    boot_guard_early();
    uint32_t heap_initial = esp_get_free_heap_size();

    ESP_LOGI(TAG, "%s %s starting (reset reason: %s)", CONFIG_NOVY_DEVICE_NAME, esp_app_get_description()->version,
             reset_reason_str());
    esp_task_wdt_config_t wdt_cfg = {
        .timeout_ms     = 30000,
        .idle_core_mask = 0,
        .trigger_panic  = true,
    };
    if (esp_task_wdt_reconfigure(&wdt_cfg) == ESP_ERR_INVALID_STATE) esp_task_wdt_init(&wdt_cfg);

    applog_init();
    status_led_start();                                    /* blue while connecting */
    ESP_ERROR_CHECK(nvs_config_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(wifi_mgr_start());
    novy_cfg_t cfg;
    nvs_config_copy(&cfg);
    time_sync_start(&cfg);

    /* Registration order = restart hook order: finish RF, stop httpd, MQTT offline, boot guard. */
    if (rf_tx_start() != ESP_OK) applog("RF transmitter unavailable");
    restart_add_hook(rf_tx_restart_hook);
    ESP_ERROR_CHECK(web_server_start(heap_initial));
    ESP_ERROR_CHECK(mqtt_pub_init());
    mqtt_pub_reconfigure();
    xTaskCreate(mqtt_pub_task, "mqtt_pub", 6144, NULL, 3, NULL);
    restart_add_hook(boot_guard_restart_hook);
    applog("System initialized successfully");

    bool     had_ip = false, was_up = false, fail_logged = false;
    uint32_t next_heap_log = HEAP_LOG_S;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        uint32_t up = uptime_s();
        bool     ip = wifi_mgr_sta_connected();
        char     addr[16] = "";
        boot_guard_tick(up, ip);

        if (ip && !was_up) {
            wifi_mgr_ip(addr, sizeof(addr));
            if (!had_ip) applog("IP Address: %s", addr);
            else         applog("WiFi reconnected: %s", addr);
            had_ip = true;
        } else if (!ip && was_up) {
            applog("WiFi lost");
        }
        was_up = ip;
        if (!had_ip && !fail_logged && up >= WIFI_FAIL_LOG_S) {
            fail_logged = true;
            applog("WiFi failed: no IP after %d s, still trying", WIFI_FAIL_LOG_S);
        }
        if (up >= next_heap_log) {
            next_heap_log = up + HEAP_LOG_S;
            applog("Heap - Current: %lu Min: %lu Initial: %lu", (unsigned long)esp_get_free_heap_size(),
                   (unsigned long)esp_get_minimum_free_heap_size(), (unsigned long)heap_initial);
        }
    }
}
