/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
/* Status LED colour from the device state, highest priority first:
 *   OTA running (blue, dimmed on alternate blocks) > OTA result (green 500 ms / red 3 s) >
 *   press flash (red 100 ms) > no IP for 80 s (red) > boot before the first IP (blue) >
 *   first IP (green 1 s) > MQTT down (yellow) > Home Assistant light > off.
 * Colours are the 2.3 values after Adafruit_NeoPixel brightness scaling. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "light.h"

#define LED_OFF        0x000000u
#define LED_BLUE       0x000050u        /* (0,0,255) at setBrightness(80) */
#define LED_BLUE_DIM   0x000014u        /* OTA progress at setBrightness(20) */
#define LED_GREEN      0x005000u
#define LED_RED        0x500000u        /* WiFi failed / OTA error, setBrightness(80) */
#define LED_FLASH_RED  0xFF0000u        /* flashRed(): setBrightness(255) */
#define LED_YELLOW     0x281C00u        /* (255,180,0) at setBrightness(40) */

#define LED_FLASH_MS      100
#define LED_GREEN_MS      1000
#define LED_WIFI_FAIL_MS  80000          /* 2.3: 5 connect attempts of 16 s */
#define LED_OTA_OK_MS     500
#define LED_OTA_FAIL_MS   3000

typedef enum { LED_OTA_NONE, LED_OTA_ACTIVE, LED_OTA_DONE, LED_OTA_FAILED } led_ota_t;

typedef struct {
    bool      had_ip;
    uint32_t  green_until;
    uint32_t  flash_until;
    led_ota_t ota;
    bool      ota_dim;
    uint32_t  ota_until;
} led_logic_t;

typedef struct {
    bool     wifi_up;                   /* STA has an IP */
    uint32_t noip_ms;                   /* time without an IP (ignored while wifi_up) */
    bool     mqtt_enabled;
    bool     mqtt_up;
    light_t  light;
} led_inputs_t;

void     led_logic_init(led_logic_t *l);
void     led_logic_flash(led_logic_t *l, uint32_t now_ms);
void     led_logic_ota_block(led_logic_t *l);                         /* toggles the OTA blue */
void     led_logic_ota_end(led_logic_t *l, bool ok, uint32_t now_ms);
uint32_t led_logic_color(led_logic_t *l, const led_inputs_t *in, uint32_t now_ms);   /* 0xRRGGBB */
