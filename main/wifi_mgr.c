/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "mdns.h"
#include "sdkconfig.h"
#include "nvs_config.h"
#include "wifi_mgr.h"

static const char *TAG = "wifi";

#define TRIAL_MS          120000      /* pending settings must get an IP within this */
#define AP_AFTER_MS       120000      /* no IP this long -> setup AP */
#define AP_STOP_AFTER_MS  60000       /* STA had an IP this long -> stop the automatic AP */
#define MANUAL_AP_MS      600000
#define RETRY_MS          5000
#define RETRY_AP_MS       30000       /* fewer STA scans while the AP serves a client */

static esp_netif_t      *s_sta;
static net_cfg_t         s_net;       /* settings in use */
static net_cfg_t         s_last_good;
static bool              s_wifi_started;
static bool              s_mdns;
static volatile bool     s_has_ip;
static volatile bool     s_ap_on;
static volatile bool     s_trial;
static volatile bool     s_manual_ap_req;
static uint32_t          s_trial_start_ms;
static volatile uint32_t s_noip_since_ms;
static volatile uint32_t s_ip_since_ms;
static uint32_t          s_last_connect_ms;
static volatile uint32_t s_manual_ap_until_ms;   /* 0 = no manual AP */

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void set_dns(esp_netif_dns_type_t type, const char *addr)
{
    esp_netif_dns_info_t dns = { 0 };
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    if (esp_netif_str_to_ip4(addr, &dns.ip.u_addr.ip4) == ESP_OK)
        esp_netif_set_dns_info(s_sta, type, &dns);
}

static void apply_ip(const net_cfg_t *n)
{
    esp_err_t err = esp_netif_dhcpc_stop(s_sta);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED)
        ESP_LOGW(TAG, "DHCP stop: %s", esp_err_to_name(err));
    esp_netif_set_hostname(s_sta, n->hostname);
    if (n->dhcp) {
        esp_netif_dhcpc_start(s_sta);
        ESP_LOGI(TAG, "Using DHCP, hostname %s", n->hostname);
        return;
    }
    esp_netif_ip_info_t ip = { 0 };
    esp_netif_str_to_ip4(n->ip, &ip.ip);
    esp_netif_str_to_ip4(n->netmask, &ip.netmask);
    esp_netif_str_to_ip4(n->gateway, &ip.gw);
    esp_netif_set_ip_info(s_sta, &ip);
    set_dns(ESP_NETIF_DNS_MAIN, n->dns1[0] ? n->dns1 : n->gateway);
    if (n->dns2[0]) set_dns(ESP_NETIF_DNS_BACKUP, n->dns2);
    ESP_LOGI(TAG, "Static IP %s/%s gw %s dns %s", n->ip, n->netmask, n->gateway,
             n->dns1[0] ? n->dns1 : n->gateway);
}

static void use_settings(const net_cfg_t *n)
{
    s_net = *n;
    apply_ip(n);
    wifi_config_t wc = { 0 };
    memcpy(wc.sta.ssid, n->wifi_ssid, strnlen(n->wifi_ssid, sizeof(wc.sta.ssid)));
    memcpy(wc.sta.password, n->wifi_pass, strnlen(n->wifi_pass, sizeof(wc.sta.password)));
    wc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    /* Scan every channel and join the strongest AP, not the first one
     * heard (the default fast scan sticks to a distant AP). */
    wc.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wc.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    esp_wifi_set_config(WIFI_IF_STA, &wc);
    if (s_wifi_started) esp_wifi_set_max_tx_power((int8_t)n->tx_power_qdbm);
    if (s_mdns) mdns_hostname_set(n->hostname);
}

static void ap_start(void)
{
    char ssid[33], pass[64];
    nvs_config_get_ap_ssid(ssid, sizeof(ssid));
    nvs_config_get_ap_pass(pass, sizeof(pass));
    if (strlen(pass) < 8) {
        ESP_LOGE(TAG, "Setup AP password shorter than 8 characters; AP not started");
        return;
    }
    wifi_config_t ac = { 0 };
    memcpy(ac.ap.ssid, ssid, strnlen(ssid, sizeof(ac.ap.ssid)));     /* 32 chars fit without NUL */
    ac.ap.ssid_len = (uint8_t)strnlen(ssid, sizeof(ac.ap.ssid));
    strlcpy((char *)ac.ap.password, pass, sizeof(ac.ap.password));
    ac.ap.authmode = WIFI_AUTH_WPA2_PSK;
    ac.ap.max_connection = 2;
    ac.ap.channel = 1;
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_AP, &ac);
    s_ap_on = true;
    ESP_LOGW(TAG, "Setup AP '%s' started, web UI on 192.168.4.1", ssid);
}

static void ap_stop(void)
{
    esp_wifi_set_mode(WIFI_MODE_STA);
    s_ap_on = false;
    s_manual_ap_until_ms = 0;
    ESP_LOGI(TAG, "Setup AP stopped");
}

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        s_last_connect_ms = now_ms();
    } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *d = (wifi_event_sta_disconnected_t *)data;
        if (s_has_ip) s_noip_since_ms = now_ms();
        s_has_ip = false;
        ESP_LOGW(TAG, "Disconnected (reason %d)", d->reason);   /* wifi_task reconnects */
    }
}

static void on_got_ip(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
    ESP_LOGI(TAG, "Got IP " IPSTR, IP2STR(&e->ip_info.ip));
    s_ip_since_ms = now_ms();
    s_has_ip = true;
}

static void wifi_task(void *arg)
{
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        uint32_t now = now_ms();

        if (s_trial && s_has_ip) {
            novy_cfg_t c;
            nvs_config_copy(&c);
            c.net = s_net;
            if (nvs_config_save(&c) == ESP_OK) ESP_LOGI(TAG, "New network settings confirmed");
            s_last_good = s_net;
            s_trial = false;
        } else if (s_trial && now - s_trial_start_ms >= TRIAL_MS) {
            ESP_LOGW(TAG, "No IP with the new network settings within %d s, reverting", TRIAL_MS / 1000);
            s_trial = false;
            esp_wifi_disconnect();
            use_settings(&s_last_good);
            s_noip_since_ms = now;
            esp_wifi_connect();
            s_last_connect_ms = now;
        }

        if (s_manual_ap_req) {
            s_manual_ap_req = false;
            s_manual_ap_until_ms = now + MANUAL_AP_MS;
            if (!s_ap_on) ap_start();
        }
        if (!s_ap_on && !s_trial && !s_has_ip && now - s_noip_since_ms >= AP_AFTER_MS) ap_start();
        if (s_ap_on) {
            bool manual = s_manual_ap_until_ms && (int32_t)(s_manual_ap_until_ms - now) > 0;
            if (!manual && s_has_ip && now - s_ip_since_ms >= AP_STOP_AFTER_MS) ap_stop();
        }

        if (!s_has_ip && now - s_last_connect_ms >= (s_ap_on ? RETRY_AP_MS : RETRY_MS)) {
            esp_wifi_connect();
            s_last_connect_ms = now;
        }
    }
}

esp_err_t wifi_mgr_start(void)
{
    novy_cfg_t cfg;
    nvs_config_copy(&cfg);
    s_last_good = cfg.net;
    net_cfg_t pend;
    if (nvs_config_take_pending_net(&pend)) {
        s_trial = true;
        s_net = pend;
        ESP_LOGW(TAG, "Trying new network settings (%s)", pend.dhcp ? "DHCP" : pend.ip);
    } else {
        s_net = cfg.net;
    }

    s_sta = esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t wic = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wic));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_got_ip, NULL, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    use_settings(&s_net);

    uint32_t now = now_ms();
    s_trial_start_ms = now;
    s_noip_since_ms = now;
    s_last_connect_ms = now;
    ESP_ERROR_CHECK(esp_wifi_start());
    s_wifi_started = true;
    esp_wifi_set_ps(WIFI_PS_NONE);
    esp_wifi_set_max_tx_power((int8_t)s_net.tx_power_qdbm);
    ESP_LOGI(TAG, "Connecting to %s, TX power %.2f dBm", s_net.wifi_ssid, s_net.tx_power_qdbm / 4.0);

    if (mdns_init() == ESP_OK) {
        char upper[33];
        strlcpy(upper, CONFIG_NOVY_DEVICE_NAME, sizeof(upper));
        for (char *p = upper; *p; p++) *p = (char)toupper((unsigned char)*p);
        s_mdns = true;
        mdns_hostname_set(s_net.hostname);
        mdns_instance_name_set(upper);
        mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
        ESP_LOGI(TAG, "mDNS: http://%s.local/", s_net.hostname);
    } else {
        ESP_LOGW(TAG, "mDNS unavailable");
    }
    return xTaskCreate(wifi_task, "wifi_mgr", 4096, NULL, 4, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

bool wifi_mgr_sta_connected(void)
{
    return s_has_ip;
}

bool wifi_mgr_ap_active(void)
{
    return s_ap_on;
}

void wifi_mgr_start_setup_ap(void)
{
    s_manual_ap_req = true;
}

void wifi_mgr_get_info(wifi_info_t *out)
{
    memset(out, 0, sizeof(*out));
    out->sta_connected = s_has_ip;
    out->ap_active = s_ap_on;
    out->trial = s_trial;
    strlcpy(out->ssid, s_net.wifi_ssid, sizeof(out->ssid));
    esp_netif_ip_info_t ip;
    if (s_has_ip && esp_netif_get_ip_info(s_sta, &ip) == ESP_OK) {
        snprintf(out->ip, sizeof(out->ip), IPSTR, IP2STR(&ip.ip));
        snprintf(out->netmask, sizeof(out->netmask), IPSTR, IP2STR(&ip.netmask));
        snprintf(out->gateway, sizeof(out->gateway), IPSTR, IP2STR(&ip.gw));
        esp_netif_dns_info_t dns;
        if (esp_netif_get_dns_info(s_sta, ESP_NETIF_DNS_MAIN, &dns) == ESP_OK)
            snprintf(out->dns, sizeof(out->dns), IPSTR, IP2STR(&dns.ip.u_addr.ip4));
    }
    wifi_ap_record_t ap;
    if (s_has_ip && esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        out->have_rssi = true;
        out->rssi = ap.rssi;
    }
    uint32_t until = s_manual_ap_until_ms, now = now_ms();
    if (s_ap_on && until && (int32_t)(until - now) > 0) out->ap_manual_left_s = (until - now) / 1000;
}

uint32_t wifi_mgr_noip_ms(void)
{
    return s_has_ip ? 0 : now_ms() - s_noip_since_ms;
}

int wifi_mgr_rssi(void)
{
    wifi_ap_record_t ap;
    return (s_has_ip && esp_wifi_sta_get_ap_info(&ap) == ESP_OK) ? ap.rssi : 0;
}

bool wifi_mgr_ip(char *out, size_t len)
{
    esp_netif_ip_info_t ip;
    if (!s_has_ip || esp_netif_get_ip_info(s_sta, &ip) != ESP_OK) return false;
    snprintf(out, len, IPSTR, IP2STR(&ip.ip));
    return true;
}
