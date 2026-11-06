/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
/* WS2812 status LED on CONFIG_NOVY_LED_GPIO (espressif/led_strip, RMT), driven by led_logic. */
#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "light.h"

esp_err_t status_led_start(void);               /* blue at once, then the 50 Hz LED task */
void      status_led_flash(void);               /* button press / POST /flashRed: red 100 ms */
void      status_led_ota_block(void);
void      status_led_ota_end(bool ok);
void      status_led_set_light(const light_t *l);   /* Home Assistant light state */
