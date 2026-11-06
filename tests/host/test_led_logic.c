/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include <stdio.h>
#include "check.h"
#include "led_logic.h"

static led_inputs_t up(void)
{
    led_inputs_t in = { .wifi_up = true, .noip_ms = 0, .mqtt_enabled = true, .mqtt_up = true };
    light_init(&in.light);
    return in;
}

static void test_scaling_constants(void)
{
    /* Adafruit_NeoPixel: setBrightness(b) stores b + 1, channels scale by (c * (b + 1)) >> 8 */
    CHECK(LED_BLUE == ((255u * 81) >> 8));
    CHECK(LED_GREEN == (((255u * 81) >> 8) << 8));
    CHECK(LED_RED == (((255u * 81) >> 8) << 16));
    CHECK(LED_YELLOW == ((((255u * 41) >> 8) << 16) | (((180u * 41) >> 8) << 8)));
}

static void test_boot_sequence(void)
{
    led_logic_t  l;
    led_inputs_t in = up();
    led_logic_init(&l);
    in.wifi_up = false;
    in.noip_ms = 3000;
    CHECK(led_logic_color(&l, &in, 3000) == LED_BLUE);           /* connecting */
    in.wifi_up = true;
    CHECK(led_logic_color(&l, &in, 4000) == LED_GREEN);          /* first IP: green 1 s */
    CHECK(led_logic_color(&l, &in, 4999) == LED_GREEN);
    in.mqtt_up = false;
    CHECK(led_logic_color(&l, &in, 5000) == LED_YELLOW);         /* then status */
    in.mqtt_up = true;
    CHECK(led_logic_color(&l, &in, 5001) == LED_OFF);
    in.wifi_up = false;                                         /* later loss: no blue, no green again */
    in.noip_ms = 1000;
    in.mqtt_up = false;
    CHECK(led_logic_color(&l, &in, 9000) == LED_YELLOW);
    in.wifi_up = true;
    in.mqtt_up = true;
    CHECK(led_logic_color(&l, &in, 9500) == LED_OFF);
}

static void test_wifi_failed(void)
{
    led_logic_t  l;
    led_inputs_t in = up();
    led_logic_init(&l);
    in.wifi_up = false;
    in.mqtt_up = false;
    in.noip_ms = LED_WIFI_FAIL_MS - 1;
    CHECK(led_logic_color(&l, &in, 79999) == LED_BLUE);
    in.noip_ms = LED_WIFI_FAIL_MS;
    CHECK(led_logic_color(&l, &in, 80000) == LED_RED);           /* boot without WiFi */
    in.wifi_up = true;
    CHECK(led_logic_color(&l, &in, 90000) == LED_GREEN);         /* IP after all: green once */
    in.wifi_up = false;
    in.noip_ms = 100000;
    CHECK(led_logic_color(&l, &in, 300000) == LED_RED);          /* lost for 100 s */
}

static void test_flash_and_light(void)
{
    led_logic_t  l;
    led_inputs_t in = up();
    led_logic_init(&l);
    led_logic_color(&l, &in, 1000);                             /* first IP */
    CHECK(led_logic_color(&l, &in, 2500) == LED_OFF);
    led_logic_flash(&l, 3000);
    CHECK(led_logic_color(&l, &in, 3000) == LED_FLASH_RED);
    CHECK(led_logic_color(&l, &in, 3099) == LED_FLASH_RED);
    CHECK(led_logic_color(&l, &in, 3100) == LED_OFF);
    in.mqtt_up = false;
    led_logic_flash(&l, 4000);
    CHECK(led_logic_color(&l, &in, 4050) == LED_FLASH_RED);
    CHECK(led_logic_color(&l, &in, 4100) == LED_YELLOW);         /* back to status (2.3: stayed off) */
    in.mqtt_up = true;
    light_apply(&in.light, LIGHT_TOPIC_STATE, "ON");
    light_apply(&in.light, LIGHT_TOPIC_RGB, "0,255,0");
    light_apply(&in.light, LIGHT_TOPIC_BRIGHTNESS, "51");
    CHECK(led_logic_color(&l, &in, 5000) == 0x003300u);           /* the HA light */
    in.mqtt_up = false;
    CHECK(led_logic_color(&l, &in, 5001) == LED_YELLOW);          /* status wins */
    in.mqtt_enabled = false;
    CHECK(led_logic_color(&l, &in, 5002) == 0x003300u);           /* MQTT off: no yellow */
}

static void test_ota(void)
{
    led_logic_t  l;
    led_inputs_t in = up();
    led_logic_init(&l);
    led_logic_color(&l, &in, 1000);
    led_logic_flash(&l, 2000);
    led_logic_ota_block(&l);
    CHECK(led_logic_color(&l, &in, 2001) == LED_BLUE_DIM || led_logic_color(&l, &in, 2001) == LED_BLUE);
    uint32_t a = led_logic_color(&l, &in, 2002);
    led_logic_ota_block(&l);
    uint32_t b = led_logic_color(&l, &in, 2003);
    CHECK(a != b && (a == LED_BLUE || a == LED_BLUE_DIM) && (b == LED_BLUE || b == LED_BLUE_DIM));
    in.wifi_up = false;
    in.noip_ms = 100000;
    CHECK(led_logic_color(&l, &in, 2004) == b);                   /* OTA beats everything */
    in.wifi_up = true;
    led_logic_ota_end(&l, true, 3000);
    CHECK(led_logic_color(&l, &in, 3000) == LED_GREEN && led_logic_color(&l, &in, 3499) == LED_GREEN);
    CHECK(led_logic_color(&l, &in, 3500) == LED_OFF);
    led_logic_ota_block(&l);
    led_logic_ota_end(&l, false, 4000);
    CHECK(led_logic_color(&l, &in, 4000) == LED_RED && led_logic_color(&l, &in, 6999) == LED_RED);
    CHECK(led_logic_color(&l, &in, 7000) == LED_OFF);
}

static void test_wrap(void)
{
    led_logic_t  l;
    led_inputs_t in = up();
    led_logic_init(&l);
    led_logic_color(&l, &in, 0xFFFFF000u);                       /* uptime near the 49.7-day wrap */
    led_logic_flash(&l, 0xFFFFFFF0u);
    CHECK(led_logic_color(&l, &in, 0x00000010u) == LED_FLASH_RED);
    CHECK(led_logic_color(&l, &in, 0x00000060u) == LED_OFF);
}

int main(void)
{
    test_scaling_constants();
    test_boot_sequence();
    test_wifi_failed();
    test_flash_and_light();
    test_ota();
    test_wrap();
    CHECK_DONE();
}
