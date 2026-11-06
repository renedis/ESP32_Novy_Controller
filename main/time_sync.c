/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "time_sync.h"

static const char *TAG = "time";

static char              s_servers[2][64];     /* lwIP keeps the pointers */
static bool              s_started;
static volatile bool     s_synced;
static volatile uint32_t s_last_sync;

static void on_sync(struct timeval *tv)
{
    s_last_sync = (uint32_t)tv->tv_sec;
    if (!s_synced) ESP_LOGI(TAG, "Time synced");
    s_synced = true;
}

void time_sync_start(const novy_cfg_t *cfg)
{
    setenv("TZ", cfg->tz, 1);
    tzset();
    if (s_started) {
        esp_netif_sntp_deinit();
        s_started = false;
    }
    int n = 0;
    if (cfg->ntp1[0]) strlcpy(s_servers[n++], cfg->ntp1, sizeof(s_servers[0]));
    if (cfg->ntp2[0]) strlcpy(s_servers[n++], cfg->ntp2, sizeof(s_servers[0]));
    if (n == 0) {
        ESP_LOGW(TAG, "No NTP server configured; log times stay on uptime");
        return;
    }
    esp_sntp_config_t c = ESP_NETIF_SNTP_DEFAULT_CONFIG(s_servers[0]);
    if (n == 2) {
        c.num_of_servers = 2;
        c.servers[1] = s_servers[1];
    }
    c.sync_cb = on_sync;
    if (esp_netif_sntp_init(&c) == ESP_OK) {
        s_started = true;
        ESP_LOGI(TAG, "SNTP started (%s%s%s), TZ %s", s_servers[0], n == 2 ? ", " : "",
                 n == 2 ? s_servers[1] : "", cfg->tz);
    } else {
        ESP_LOGE(TAG, "SNTP init failed");
    }
}

bool time_sync_is_synced(void)
{
    return s_synced;
}

uint32_t time_sync_last_sync(void)
{
    return s_last_sync;
}
