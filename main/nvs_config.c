/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "nvs.h"
#include "sdkconfig.h"
#include "config_validate.h"
#include "nvs_config.h"

static const char *TAG = "nvs_cfg";

#define NVS_NS      NVS_CONFIG_NS
#define CFG_VERSION 1           /* bump when novy_cfg_t changes layout */

static novy_cfg_t        s_cfg;
static SemaphoreHandle_t s_mutex;

static void apply_kconfig(novy_cfg_t *c)
{
    strlcpy(c->net.wifi_ssid, CONFIG_NOVY_WIFI_SSID, sizeof(c->net.wifi_ssid));
    strlcpy(c->net.wifi_pass, CONFIG_NOVY_WIFI_PASSWORD, sizeof(c->net.wifi_pass));
    strlcpy(c->net.ip, CONFIG_NOVY_STATIC_IP, sizeof(c->net.ip));
    strlcpy(c->net.netmask, CONFIG_NOVY_STATIC_NETMASK, sizeof(c->net.netmask));
    strlcpy(c->net.gateway, CONFIG_NOVY_STATIC_GATEWAY, sizeof(c->net.gateway));
    strlcpy(c->net.dns1, CONFIG_NOVY_STATIC_DNS, sizeof(c->net.dns1));
    strlcpy(c->mqtt_uri, CONFIG_NOVY_MQTT_URI, sizeof(c->mqtt_uri));
    strlcpy(c->mqtt_user, CONFIG_NOVY_MQTT_USER, sizeof(c->mqtt_user));
    strlcpy(c->mqtt_pass, CONFIG_NOVY_MQTT_PASSWORD, sizeof(c->mqtt_pass));
    c->mqtt_en = c->mqtt_uri[0] != '\0';
}

esp_err_t nvs_config_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) return ESP_ERR_NO_MEM;
    cfg_set_defaults(&s_cfg, CONFIG_NOVY_DEVICE_NAME);
    apply_kconfig(&s_cfg);

    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) {
        ESP_LOGI(TAG, "No saved config, using defaults");
        return ESP_OK;
    }
    uint32_t   ver = 0;
    novy_cfg_t tmp;
    size_t     len = sizeof(tmp);
    esp_err_t err = nvs_get_u32(h, "cfg_ver", &ver);
    if (err == ESP_OK) err = nvs_get_blob(h, "cfg", &tmp, &len);
    nvs_close(h);

    if (err != ESP_OK || ver != CFG_VERSION || len != sizeof(tmp)) {
        ESP_LOGW(TAG, "Stored config missing or incompatible, using defaults");
        return ESP_OK;
    }
    const char *why = cfg_validate(&tmp);
    if (why) {
        ESP_LOGW(TAG, "Stored config invalid (%s), using defaults", why);
        return ESP_OK;
    }
    s_cfg = tmp;
    ESP_LOGI(TAG, "Config loaded from NVS");
    return ESP_OK;
}

void nvs_config_copy(novy_cfg_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_cfg;
    xSemaphoreGive(s_mutex);
}

esp_err_t nvs_config_save(const novy_cfg_t *cfg)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(h, "cfg", cfg, sizeof(*cfg));
    if (err == ESP_OK) err = nvs_set_u32(h, "cfg_ver", CFG_VERSION);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);

    if (err == ESP_OK) {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        s_cfg = *cfg;
        xSemaphoreGive(s_mutex);
        ESP_LOGI(TAG, "Config saved");
    } else {
        ESP_LOGE(TAG, "Config save failed: %s", esp_err_to_name(err));
    }
    return err;
}

static esp_err_t save_blob(const char *key, const void *buf, size_t len)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(h, key, buf, len);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) ESP_LOGE(TAG, "Write %s failed: %s", key, esp_err_to_name(err));
    return err;
}

esp_err_t nvs_config_save_pending_net(const net_cfg_t *net)
{
    return save_blob("net_pend", net, sizeof(*net));
}

bool nvs_config_take_pending_net(net_cfg_t *out)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return false;
    size_t len = sizeof(*out);
    bool ok = nvs_get_blob(h, "net_pend", out, &len) == ESP_OK && len == sizeof(*out);
    if (nvs_erase_key(h, "net_pend") == ESP_OK) nvs_commit(h);   /* tried at most once */
    nvs_close(h);
    return ok;
}

static void get_str(const char *key, char *out, size_t len, const char *fallback)
{
    nvs_handle_t h;
    size_t n = len;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        esp_err_t err = nvs_get_str(h, key, out, &n);
        nvs_close(h);
        if (err == ESP_OK) return;
    }
    strlcpy(out, fallback, len);
}

static esp_err_t set_str(const char *key, const char *val)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_str(h, key, val);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err == ESP_OK) ESP_LOGI(TAG, "%s changed", key);
    return err;
}

void nvs_config_get_ap_ssid(char *out, size_t len)   { get_str("ap_ssid", out, len, CONFIG_NOVY_AP_SSID); }
esp_err_t nvs_config_set_ap_ssid(const char *ssid)   { return set_str("ap_ssid", ssid); }
void nvs_config_get_ap_pass(char *out, size_t len)   { get_str("ap_pass", out, len, CONFIG_NOVY_AP_PASSWORD); }
esp_err_t nvs_config_set_ap_pass(const char *pass)   { return set_str("ap_pass", pass); }
