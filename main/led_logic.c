/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include "led_logic.h"

static bool before(uint32_t now, uint32_t until)
{
    return (int32_t)(until - now) > 0;      /* wrap-safe */
}

void led_logic_init(led_logic_t *l)
{
    l->had_ip      = false;
    l->green_until = 0;
    l->flash_until = 0;
    l->ota         = LED_OTA_NONE;
    l->ota_dim     = false;
    l->ota_until   = 0;
}

void led_logic_flash(led_logic_t *l, uint32_t now_ms)
{
    l->flash_until = now_ms + LED_FLASH_MS;
}

void led_logic_ota_block(led_logic_t *l)
{
    l->ota     = LED_OTA_ACTIVE;
    l->ota_dim = !l->ota_dim;
}

void led_logic_ota_end(led_logic_t *l, bool ok, uint32_t now_ms)
{
    l->ota       = ok ? LED_OTA_DONE : LED_OTA_FAILED;
    l->ota_until = now_ms + (ok ? LED_OTA_OK_MS : LED_OTA_FAIL_MS);
}

uint32_t led_logic_color(led_logic_t *l, const led_inputs_t *in, uint32_t now)
{
    if (in->wifi_up && !l->had_ip) {
        l->had_ip      = true;
        l->green_until = now + LED_GREEN_MS;
    }
    if (l->ota == LED_OTA_ACTIVE) return l->ota_dim ? LED_BLUE_DIM : LED_BLUE;
    if (l->ota == LED_OTA_DONE || l->ota == LED_OTA_FAILED) {
        if (before(now, l->ota_until)) return l->ota == LED_OTA_DONE ? LED_GREEN : LED_RED;
        l->ota = LED_OTA_NONE;
    }
    if (before(now, l->flash_until)) return LED_FLASH_RED;
    if (!in->wifi_up && in->noip_ms >= LED_WIFI_FAIL_MS) return LED_RED;
    if (!l->had_ip) return LED_BLUE;
    if (before(now, l->green_until)) return LED_GREEN;
    if (in->mqtt_enabled && !in->mqtt_up) return LED_YELLOW;
    uint8_t r, g, b;
    light_output(&in->light, &r, &g, &b);
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}
