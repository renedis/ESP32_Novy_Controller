/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
/* Boot guard under QEMU. CONFIG_BGTEST_CRASH: abort after the guard, so every boot is fast.
 * Otherwise run the 1 s tick like main.c, with an IP from 2 s on (or never: CONFIG_BGTEST_NO_IP).
 * Lines start with "BGTEST". */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "sdkconfig.h"
#include "boot_guard_idf.h"

/* The firmware's applog() and restart_schedule(), reduced to what the guard needs. */
void applog(const char *fmt, ...)
{
    va_list ap;
    printf("BGTEST log: ");
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

void restart_schedule(const char *reason)
{
    printf("BGTEST restart: %s\n", reason);
    boot_guard_restart_hook();
    esp_restart();
}

static void report(const char *when)
{
    boot_guard_info_t bi;
    esp_ota_img_states_t st = ESP_OTA_IMG_UNDEFINED;
    boot_guard_get_info(&bi);
    esp_ota_get_state_partition(esp_ota_get_running_partition(), &st);
    printf("BGTEST %s: running=%s fast_boots=%u proven=%d pending_verify=%d otadata_state=0x%x other=%s:%s\n", when,
           bi.running, bi.fast_boots, bi.proven, bi.pending_verify, (unsigned)st, bi.other,
           bi.other_project[0] ? bi.other_project : "-");
}

void app_main(void)
{
    boot_guard_early();
    report("boot");
#if CONFIG_BGTEST_CRASH
    printf("BGTEST crash\n");
    abort();
#else
    for (uint32_t s = 1; s <= CONFIG_BGTEST_SECONDS; s++) {
        vTaskDelay(pdMS_TO_TICKS(1000));
#if CONFIG_BGTEST_NO_IP
        boot_guard_tick(s, false);
#else
        boot_guard_tick(s, s >= 2);
#endif
        if (s == 59 || s == 61) report(s == 59 ? "t59" : "t61");
    }
    printf("BGTEST done\n");
#endif
}
