/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "led_strip.h"
#include "sdkconfig.h"
#include "led_logic.h"
#include "mqtt_pub.h"
#include "wifi_mgr.h"
#include "status_led.h"

static const char *TAG = "led";

static led_strip_handle_t s_strip;
static led_logic_t        s_logic;
static light_t            s_light;
static portMUX_TYPE       s_mux = portMUX_INITIALIZER_UNLOCKED;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void show(uint32_t rgb)
{
    led_strip_set_pixel(s_strip, 0, (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
    led_strip_refresh(s_strip);
}

static void led_task(void *arg)
{
    uint32_t shown = LED_BLUE;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(20));
        led_inputs_t in = {
            .wifi_up      = wifi_mgr_sta_connected(),
            .noip_ms      = wifi_mgr_noip_ms(),
            .mqtt_enabled = mqtt_pub_enabled(),
            .mqtt_up      = mqtt_pub_is_connected(),
        };
        taskENTER_CRITICAL(&s_mux);
        in.light = s_light;
        uint32_t rgb = led_logic_color(&s_logic, &in, now_ms());
        taskEXIT_CRITICAL(&s_mux);
        if (rgb != shown) {
            show(rgb);
            shown = rgb;
        }
    }
}

esp_err_t status_led_start(void)
{
    led_strip_config_t sc = {
        .strip_gpio_num         = CONFIG_NOVY_LED_GPIO,
        .max_leds               = 1,
        .led_model              = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,     /* NEO_GRB */
    };
    led_strip_rmt_config_t rc = {
        .clk_src           = RMT_CLK_SRC_DEFAULT,                         /* same group clock as RF */
        .resolution_hz     = 10 * 1000 * 1000,
        .mem_block_symbols = 48,
    };
    light_init(&s_light);
    led_logic_init(&s_logic);
    esp_err_t err = led_strip_new_rmt_device(&sc, &rc, &s_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LED strip on GPIO%d failed: %s", CONFIG_NOVY_LED_GPIO, esp_err_to_name(err));
        return err;
    }
    show(LED_BLUE);                                                       /* booting */
    return xTaskCreate(led_task, "led", 3072, NULL, 2, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

void status_led_flash(void)
{
    taskENTER_CRITICAL(&s_mux);
    led_logic_flash(&s_logic, now_ms());
    taskEXIT_CRITICAL(&s_mux);
}

void status_led_ota_block(void)
{
    taskENTER_CRITICAL(&s_mux);
    led_logic_ota_block(&s_logic);
    taskEXIT_CRITICAL(&s_mux);
}

void status_led_ota_end(bool ok)
{
    taskENTER_CRITICAL(&s_mux);
    led_logic_ota_end(&s_logic, ok, now_ms());
    taskEXIT_CRITICAL(&s_mux);
}

void status_led_set_light(const light_t *l)
{
    taskENTER_CRITICAL(&s_mux);
    s_light = *l;
    taskEXIT_CRITICAL(&s_mux);
}
