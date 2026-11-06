/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */

#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_image_format.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "cJSON.h"
#include "sdkconfig.h"
#include "applog.h"
#include "boot_guard_idf.h"
#include "config_validate.h"
#include "mqtt_pub.h"
#include "net_config.h"
#include "nvs_config.h"
#include "restart.h"
#include "rf_tx.h"
#include "status_fmt.h"
#include "status_led.h"
#include "time_sync.h"
#include "wifi_mgr.h"
#include "peers_web.h"
#include "web_server.h"

static const char *TAG = "web";

extern const char index_html_start[]  asm("_binary_index_html_start");
extern const char index_html_end[]    asm("_binary_index_html_end");
extern const char config_html_start[] asm("_binary_config_html_start");
extern const char config_html_end[]   asm("_binary_config_html_end");

static httpd_handle_t s_server;
static uint32_t       s_heap_initial;

/* ── Console ring for the Serial Console tab ───────────────────────────── */
#define LOG_RING_SIZE (16 * 1024)
static char           s_log_ring[LOG_RING_SIZE];
static size_t         s_log_head;
static size_t         s_log_len;
static vprintf_like_t s_orig_vprintf;
static volatile bool  s_log_capture;

static int log_vprintf(const char *fmt, va_list args)
{
    static char tmp[256];
    if (s_log_capture) {
        va_list copy;
        va_copy(copy, args);
        int n = vsnprintf(tmp, sizeof(tmp), fmt, copy);
        va_end(copy);
        if (n > 0) {
            if (n > (int)sizeof(tmp) - 1) n = sizeof(tmp) - 1;
            for (int i = 0; i < n; i++) {
                s_log_ring[s_log_head] = tmp[i];
                s_log_head = (s_log_head + 1) % LOG_RING_SIZE;
            }
            s_log_len += n;
            if (s_log_len > LOG_RING_SIZE) s_log_len = LOG_RING_SIZE;
        }
    }
    return s_orig_vprintf(fmt, args);
}

/* ── Helpers ──────────────────────────────────────────────────────────── */
static esp_err_t send_text(httpd_req_t *req, const char *status, const char *type, const char *body)
{
    if (status) httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, type);
    return httpd_resp_sendstr(req, body);
}

static esp_err_t send_json(httpd_req_t *req, cJSON *root)
{
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    esp_err_t err = httpd_resp_sendstr(req, json);
    free(json);
    return err;
}

static esp_err_t send_ok(httpd_req_t *req)
{
    return send_text(req, NULL, "application/json", "{\"ok\":true}");
}

/* Reads a body of at most max bytes; sends the error response itself. */
static char *recv_body(httpd_req_t *req, size_t max)
{
    if (req->content_len == 0 || req->content_len > max) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid body size");
        return NULL;
    }
    char *buf = malloc(req->content_len + 1);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return NULL;
    }
    size_t total = 0;
    while (total < req->content_len) {
        int r = httpd_req_recv(req, buf + total, req->content_len - total);
        if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (r <= 0) {
            free(buf);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Receive error");
            return NULL;
        }
        total += r;
    }
    buf[total] = '\0';
    return buf;
}

static cJSON *recv_json(httpd_req_t *req, size_t max)
{
    char *body = recv_body(req, max);
    if (!body) return NULL;
    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad JSON");
    return root;
}

static void field_str(cJSON *o, const char *key, char *dst, size_t len)
{
    cJSON *v = cJSON_GetObjectItem(o, key);
    if (cJSON_IsString(v)) strlcpy(dst, v->valuestring, len);
}

/* Passwords: an empty or missing value keeps the stored one. */
static void field_secret(cJSON *o, const char *key, char *dst, size_t len)
{
    cJSON *v = cJSON_GetObjectItem(o, key);
    if (cJSON_IsString(v) && v->valuestring[0]) strlcpy(dst, v->valuestring, len);
}

static bool field_bool(cJSON *o, const char *key, bool *dst)
{
    cJSON *v = cJSON_GetObjectItem(o, key);
    if (!cJSON_IsBool(v)) return false;
    *dst = cJSON_IsTrue(v);
    return true;
}

static bool field_num(cJSON *o, const char *key, double *dst)
{
    cJSON *v = cJSON_GetObjectItem(o, key);
    if (!cJSON_IsNumber(v)) return false;
    *dst = v->valuedouble;
    return true;
}

static uint32_t uptime_s(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000000);
}

static esp_err_t send_page(httpd_req_t *req, const char *start, const char *end)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    const char *p = start;
    size_t remaining = end - start - 1;
    while (remaining > 0) {
        size_t chunk = remaining > 4096 ? 4096 : remaining;
        if (httpd_resp_send_chunk(req, p, chunk) != ESP_OK) return ESP_FAIL;
        p += chunk;
        remaining -= chunk;
    }
    return httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t handle_index(httpd_req_t *req)       { return send_page(req, index_html_start, index_html_end); }
static esp_err_t handle_config_page(httpd_req_t *req) { return send_page(req, config_html_start, config_html_end); }

/* ── Endpoints of firmware 2.3 ────────────────────────────────────────── */
static esp_err_t handle_cmd(httpd_req_t *req)
{
    int cmd = (int)(intptr_t)req->user_ctx;
    if (!rf_tx_submit(cmd, RF_SRC_WEB))
        return send_text(req, "503 Service Unavailable", "text/plain", "RF queue full");
    return send_text(req, NULL, "text/plain", rf_cmd_info(cmd)->reply);
}

static esp_err_t handle_flash_red(httpd_req_t *req)
{
    esp_err_t err = send_text(req, NULL, "text/plain", "Flashing red");
    status_led_flash();
    return err;
}

static esp_err_t handle_log(httpd_req_t *req)
{
    size_t len = 0;
    char  *text = applog_dump(&len);
    if (!text) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
    httpd_resp_set_type(req, "text/plain");
    esp_err_t err = httpd_resp_send(req, text, len);
    free(text);
    return err;
}

static esp_err_t handle_log_clear(httpd_req_t *req)
{
    applog_clear();
    return send_text(req, NULL, "text/plain", "OK");
}

static esp_err_t handle_heap(httpd_req_t *req)
{
    char js[160];
    sf_heap_json(js, sizeof(js), esp_get_free_heap_size(), s_heap_initial, esp_get_minimum_free_heap_size());
    return send_text(req, NULL, "application/json", js);
}

static esp_err_t handle_rssi(httpd_req_t *req)
{
    char js[96];
    sf_rssi_json(js, sizeof(js), wifi_mgr_rssi());
    return send_text(req, NULL, "application/json", js);
}

static esp_err_t handle_mqtt_status(httpd_req_t *req)
{
    char js[32];
    sf_mqtt_json(js, sizeof(js), mqtt_pub_is_connected());
    return send_text(req, NULL, "application/json", js);
}

static esp_err_t handle_favicon(httpd_req_t *req)
{
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t handle_reset(httpd_req_t *req)
{
    send_text(req, NULL, "text/plain", "Resetting device...");
    applog("Device reset requested via web interface");
    boot_guard_confirm("restart from the web UI");
    restart_schedule("reset via web");
    return ESP_OK;
}

/* Unknown path, or a known path with the wrong method: Arduino WebServer's onNotFound. */
static esp_err_t handle_not_found(httpd_req_t *req, httpd_err_code_t err)
{
    send_text(req, "404 Not Found", "text/plain", "Not found");
    return ESP_OK;
}

/* ── GET /api/status ──────────────────────────────────────────────────── */
static esp_err_t handle_api_status(httpd_req_t *req)
{
    wifi_info_t       wi;
    novy_cfg_t        cfg;
    rf_tx_status_t    rf;
    boot_guard_info_t bg;
    light_t           li;
    wifi_mgr_get_info(&wi);
    nvs_config_copy(&cfg);
    rf_tx_get_status(&rf);
    boot_guard_get_info(&bg);
    mqtt_pub_get_light(&li);
    uint32_t up = uptime_s(), heap = esp_get_free_heap_size();

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device", CONFIG_NOVY_DEVICE_NAME);
    cJSON_AddStringToObject(root, "hostname", cfg.net.hostname);
    cJSON_AddStringToObject(root, "version", esp_app_get_description()->version);
    cJSON_AddStringToObject(root, "idf", esp_app_get_description()->idf_ver);
    cJSON_AddStringToObject(root, "partition", bg.running);
    cJSON_AddNumberToObject(root, "uptime", up);
    cJSON_AddStringToObject(root, "reset_reason", reset_reason_str());
    cJSON_AddBoolToObject(root, "mqtt_enabled", mqtt_pub_enabled());
    cJSON_AddBoolToObject(root, "mqtt_connected", mqtt_pub_is_connected());
    cJSON_AddStringToObject(root, "mqtt_client_id", mqtt_pub_client_id());

    cJSON *h = cJSON_AddObjectToObject(root, "heap");
    cJSON_AddNumberToObject(h, "current", heap);
    cJSON_AddNumberToObject(h, "initial", s_heap_initial);
    cJSON_AddNumberToObject(h, "minimum", esp_get_minimum_free_heap_size());
    cJSON_AddNumberToObject(h, "percentage", sf_heap_percentage(heap, s_heap_initial));

    cJSON *w = cJSON_AddObjectToObject(root, "wifi");
    cJSON_AddBoolToObject(w, "connected", wi.sta_connected);
    cJSON_AddStringToObject(w, "ssid", wi.ssid);
    cJSON_AddStringToObject(w, "ip", wi.ip);
    cJSON_AddStringToObject(w, "netmask", wi.netmask);
    cJSON_AddStringToObject(w, "gateway", wi.gateway);
    cJSON_AddStringToObject(w, "dns", wi.dns);
    if (wi.have_rssi) cJSON_AddNumberToObject(w, "rssi", wi.rssi);
    else              cJSON_AddNullToObject(w, "rssi");
    cJSON_AddBoolToObject(w, "ap", wi.ap_active);
    cJSON_AddNumberToObject(w, "ap_manual_left_s", wi.ap_manual_left_s);
    cJSON_AddBoolToObject(w, "trial", wi.trial);

    cJSON *t = cJSON_AddObjectToObject(root, "time");
    bool synced = time_sync_is_synced();
    cJSON_AddBoolToObject(t, "synced", synced);
    cJSON_AddNumberToObject(t, "epoch", synced ? (double)time(NULL) : 0);
    cJSON_AddNumberToObject(t, "last_sync", time_sync_last_sync());

    cJSON *s = cJSON_AddObjectToObject(root, "state");
    cJSON_AddBoolToObject(s, "novy_light", rf.state.novy_light);
    cJSON_AddNumberToObject(s, "fan_speed", rf.state.fan_speed);
    cJSON_AddBoolToObject(s, "fan_light", rf.state.fan_light);
    cJSON_AddStringToObject(s, "fan_direction", devstate_direction(&rf.state));

    cJSON *l = cJSON_AddObjectToObject(root, "light");
    cJSON_AddBoolToObject(l, "on", li.on);
    cJSON_AddNumberToObject(l, "brightness", li.brightness);
    cJSON *rgb = cJSON_AddArrayToObject(l, "rgb");
    cJSON_AddItemToArray(rgb, cJSON_CreateNumber(li.r));
    cJSON_AddItemToArray(rgb, cJSON_CreateNumber(li.g));
    cJSON_AddItemToArray(rgb, cJSON_CreateNumber(li.b));

    cJSON *r = cJSON_AddObjectToObject(root, "rf");
    cJSON_AddBoolToObject(r, "ready", rf.ready);
    cJSON_AddNumberToObject(r, "gpio", rf.gpio);
    cJSON_AddStringToObject(r, "codes", rf_codes_example ? "example" : "own");
    cJSON_AddNumberToObject(r, "novy_channel", rf_cmd_novy_channel());
    cJSON_AddNumberToObject(r, "sent", rf.sent);
    cJSON_AddNumberToObject(r, "queued", rf.queued);
    cJSON_AddNumberToObject(r, "dropped", rf.dropped);
    cJSON_AddNumberToObject(r, "failed", rf.failed);
    cJSON_AddNumberToObject(r, "novy_frames_per_send", rf.repeat_transmit);
    if (rf.last_cmd >= 0) {
        cJSON_AddStringToObject(r, "last", rf_cmd_info(rf.last_cmd)->name);
        cJSON_AddNumberToObject(r, "last_age_s", up - rf.last_uptime_s);
    } else {
        cJSON_AddNullToObject(r, "last");
        cJSON_AddNullToObject(r, "last_age_s");
    }
    cJSON *fc = cJSON_AddObjectToObject(r, "fan_cycle");
    for (int i = 0; i < RF_FAN_COUNT; i++) cJSON_AddNumberToObject(fc, rf_cmd_info(RF_FAN_FIRST + i)->id, rf.fan_cycle[i]);

    cJSON *b = cJSON_AddObjectToObject(root, "boot");
    cJSON_AddBoolToObject(b, "guard", bg.nvs_ok);
    cJSON_AddNumberToObject(b, "fast_boots", bg.fast_boots);
    cJSON_AddBoolToObject(b, "proven", bg.proven);
    cJSON_AddBoolToObject(b, "pending_verify", bg.pending_verify);
    cJSON_AddStringToObject(b, "other", bg.other);
    cJSON_AddStringToObject(b, "other_project", bg.other_project);
    cJSON_AddStringToObject(b, "other_version", bg.other_version);
    return send_json(req, root);
}

/* ── GET/POST /api/config ─────────────────────────────────────────────── */
static esp_err_t handle_api_config_get(httpd_req_t *req)
{
    novy_cfg_t c;
    char       ap_ssid[33];
    nvs_config_copy(&c);
    cJSON *root = cJSON_CreateObject();
    cJSON *n = cJSON_AddObjectToObject(root, "net");
    cJSON_AddStringToObject(n, "wifi_ssid", c.net.wifi_ssid);
    cJSON_AddBoolToObject(n, "wifi_pass_set", c.net.wifi_pass[0] != '\0');
    cJSON_AddBoolToObject(n, "dhcp", c.net.dhcp);
    cJSON_AddStringToObject(n, "ip", c.net.ip);
    cJSON_AddStringToObject(n, "netmask", c.net.netmask);
    cJSON_AddStringToObject(n, "gateway", c.net.gateway);
    cJSON_AddStringToObject(n, "dns1", c.net.dns1);
    cJSON_AddStringToObject(n, "dns2", c.net.dns2);
    cJSON_AddStringToObject(n, "hostname", c.net.hostname);
    cJSON_AddNumberToObject(n, "tx_power_dbm", c.net.tx_power_qdbm / 4.0);
    cJSON_AddStringToObject(root, "ntp1", c.ntp1);
    cJSON_AddStringToObject(root, "ntp2", c.ntp2);
    cJSON_AddStringToObject(root, "tz", c.tz);
    cJSON_AddBoolToObject(root, "mqtt_en", c.mqtt_en);
    cJSON_AddStringToObject(root, "mqtt_uri", c.mqtt_uri);
    cJSON_AddStringToObject(root, "mqtt_user", c.mqtt_user);
    cJSON_AddBoolToObject(root, "mqtt_pass_set", c.mqtt_pass[0] != '\0');
    cJSON_AddStringToObject(root, "mqtt_prefix", c.mqtt_prefix);
    cJSON_AddStringToObject(root, "device", CONFIG_NOVY_DEVICE_NAME);
    nvs_config_get_ap_ssid(ap_ssid, sizeof(ap_ssid));
    cJSON_AddStringToObject(root, "ap_ssid", ap_ssid);        /* the AP password is never returned */
    return send_json(req, root);
}

/* Partial update of everything except the network settings; applied at once. */
static esp_err_t handle_api_config_post(httpd_req_t *req)
{
    cJSON *root = recv_json(req, 2048);
    if (!root) return ESP_OK;
    novy_cfg_t c;
    nvs_config_copy(&c);
    field_str(root, "ntp1", c.ntp1, sizeof(c.ntp1));
    field_str(root, "ntp2", c.ntp2, sizeof(c.ntp2));
    field_str(root, "tz", c.tz, sizeof(c.tz));
    bool b;
    if (field_bool(root, "mqtt_en", &b)) c.mqtt_en = b;
    field_str(root, "mqtt_uri", c.mqtt_uri, sizeof(c.mqtt_uri));
    field_str(root, "mqtt_user", c.mqtt_user, sizeof(c.mqtt_user));
    field_secret(root, "mqtt_pass", c.mqtt_pass, sizeof(c.mqtt_pass));
    field_str(root, "mqtt_prefix", c.mqtt_prefix, sizeof(c.mqtt_prefix));
    char ap_ssid[40] = "", ap_pass[64] = "";
    field_secret(root, "ap_ssid", ap_ssid, sizeof(ap_ssid));
    field_secret(root, "ap_pass", ap_pass, sizeof(ap_pass));
    cJSON_Delete(root);

    if (ap_ssid[0] && !net_ap_ssid_valid(ap_ssid))
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Setup AP SSID must be 1-32 printable ASCII characters");
    if (ap_pass[0] && !net_ap_pass_valid(ap_pass))
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Setup AP password must be 8-63 printable ASCII characters");
    const char *why = cfg_validate(&c);
    if (why) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, why);
    if (nvs_config_save(&c) != ESP_OK || (ap_ssid[0] && nvs_config_set_ap_ssid(ap_ssid) != ESP_OK) ||
        (ap_pass[0] && nvs_config_set_ap_pass(ap_pass) != ESP_OK))
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS write failed");
    time_sync_start(&c);
    mqtt_pub_reconfigure();
    return send_ok(req);
}

/* ── POST /api/network — stored as pending, tried after a reboot ─────── */
static esp_err_t handle_api_network(httpd_req_t *req)
{
    cJSON *root = recv_json(req, 1024);
    if (!root) return ESP_OK;
    novy_cfg_t c;
    nvs_config_copy(&c);
    net_cfg_t n = c.net;
    field_str(root, "wifi_ssid", n.wifi_ssid, sizeof(n.wifi_ssid));
    field_secret(root, "wifi_pass", n.wifi_pass, sizeof(n.wifi_pass));
    bool b;
    if (field_bool(root, "dhcp", &b)) n.dhcp = b;
    field_str(root, "ip", n.ip, sizeof(n.ip));
    field_str(root, "netmask", n.netmask, sizeof(n.netmask));
    field_str(root, "gateway", n.gateway, sizeof(n.gateway));
    field_str(root, "dns1", n.dns1, sizeof(n.dns1));
    field_str(root, "dns2", n.dns2, sizeof(n.dns2));
    field_str(root, "hostname", n.hostname, sizeof(n.hostname));
    double d;
    if (field_num(root, "tx_power_dbm", &d)) n.tx_power_qdbm = (d >= 2 && d <= 20) ? (uint8_t)lround(d * 4) : 0;
    cJSON_Delete(root);

    const char *why = net_validate(&n);
    if (why) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, why);
    if (nvs_config_save_pending_net(&n) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS write failed");
    send_ok(req);
    boot_guard_confirm("network settings saved from the web UI");
    restart_schedule("new network settings");
    return ESP_OK;
}

/* ── POST /api/setup-ap, POST /api/reboot, GET /api/log ──────────────── */
static esp_err_t handle_api_setup_ap(httpd_req_t *req)
{
    wifi_mgr_start_setup_ap();
    return send_ok(req);
}

static esp_err_t handle_api_reboot(httpd_req_t *req)
{
    send_ok(req);
    boot_guard_confirm("restart from the web UI");
    restart_schedule("reboot requested via web");
    return ESP_OK;
}

static esp_err_t handle_api_log(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    if (s_log_len == 0) return httpd_resp_sendstr(req, "(no log output yet)\n");
    size_t len = s_log_len, start;
    if (len >= LOG_RING_SIZE) { len = LOG_RING_SIZE; start = s_log_head; }
    else                      { start = (s_log_head + LOG_RING_SIZE - len) % LOG_RING_SIZE; }
    if (start + len <= LOG_RING_SIZE) return httpd_resp_send(req, &s_log_ring[start], len);
    size_t first = LOG_RING_SIZE - start;
    httpd_resp_send_chunk(req, &s_log_ring[start], first);
    httpd_resp_send_chunk(req, &s_log_ring[0], len - first);
    return httpd_resp_send_chunk(req, NULL, 0);
}

/* ── POST /api/ota[?force=1] ─────────────────────────────────────────── */
/* The app description of an image: header (24) + first segment header (8), then esp_app_desc_t. */
static bool image_project(const uint8_t *buf, size_t len, char *project, size_t plen, char *version, size_t vlen)
{
    size_t off = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t);
    if (len < off + sizeof(esp_app_desc_t) || buf[0] != ESP_IMAGE_HEADER_MAGIC) return false;
    const esp_app_desc_t *d = (const esp_app_desc_t *)(buf + off);
    if (d->magic_word != ESP_APP_DESC_MAGIC_WORD) return false;
    strlcpy(project, d->project_name, plen);
    strlcpy(version, d->version, vlen);
    return true;
}

static esp_err_t ota_fail(httpd_req_t *req, httpd_err_code_t code, const char *why)
{
    applog("OTA failed: %s", why);
    status_led_ota_end(false);
    return httpd_resp_send_err(req, code, why);
}

static esp_err_t handle_ota(httpd_req_t *req)
{
    char query[32] = "", force[4] = "";
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK)
        httpd_query_key_value(query, "force", force, sizeof(force));
    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
    if (!part) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition");
    if (req->content_len == 0) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No content length");
    if (req->content_len > part->size) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Image larger than the slot");

    uint8_t *buf = malloc(8192);
    if (!buf) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
    size_t first = 0;                          /* first block, to check the image before erasing */
    while (first < 8192 && first < req->content_len) {
        int r = httpd_req_recv(req, (char *)buf + first, (req->content_len < 8192 ? req->content_len : 8192) - first);
        if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (r <= 0) { free(buf); return ota_fail(req, HTTPD_400_BAD_REQUEST, "Receive error"); }
        first += r;
    }
    char project[33], version[33];
    if (!image_project(buf, first, project, sizeof(project), version, sizeof(version))) {
        free(buf);
        return ota_fail(req, HTTPD_400_BAD_REQUEST, "Not an ESP32 app image");
    }
    const char *ours = esp_app_get_description()->project_name;
    if (strcmp(project, ours) != 0 && strcmp(force, "1") != 0) {
        char msg[160];
        snprintf(msg, sizeof(msg), "Image is %s %s, not %s; add ?force=1 to install it anyway", project, version, ours);
        free(buf);
        return ota_fail(req, HTTPD_400_BAD_REQUEST, msg);
    }
    applog("OTA: %s %s, %u bytes -> %s", project, version, (unsigned)req->content_len, part->label);
    boot_guard_confirm("firmware upload");     /* esp_ota_begin() refuses while PENDING_VERIFY */

    esp_ota_handle_t ota = 0;
    if (esp_ota_begin(part, req->content_len, &ota) != ESP_OK) {
        free(buf);
        return ota_fail(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
    }
    size_t written = 0, chunk = first;
    bool   ok = true;
    for (;;) {
        if (chunk) {
            status_led_ota_block();
            if (esp_ota_write(ota, buf, chunk) != ESP_OK) { ok = false; break; }
            written += chunk;
            esp_task_wdt_reset();
        }
        if (written >= req->content_len) break;
        size_t want = req->content_len - written;
        int    r = httpd_req_recv(req, (char *)buf, want < 8192 ? want : 8192);
        if (r == HTTPD_SOCK_ERR_TIMEOUT) { chunk = 0; continue; }
        if (r <= 0) { ok = false; break; }
        chunk = (size_t)r;
    }
    free(buf);
    if (!ok) {
        esp_ota_abort(ota);
        return ota_fail(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA write/receive failed");
    }
    esp_err_t err = esp_ota_end(ota);
    if (err != ESP_OK) return ota_fail(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA validation failed");
    if (esp_ota_set_boot_partition(part) != ESP_OK) return ota_fail(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Set boot failed");
    applog("OTA success (%u bytes), restarting into %s", (unsigned)written, part->label);
    status_led_ota_end(true);
    httpd_resp_sendstr(req, "OK");
    restart_schedule("OTA");
    return ESP_OK;
}

/* ── Router ───────────────────────────────────────────────────────────── */
static const httpd_uri_t s_routes[] = {
    { .uri = "/",              .method = HTTP_GET,  .handler = handle_index },
    { .uri = "/config",        .method = HTTP_GET,  .handler = handle_config_page },
    { .uri = "/flashRed",      .method = HTTP_POST, .handler = handle_flash_red },
    { .uri = "/log",           .method = HTTP_GET,  .handler = handle_log },
    { .uri = "/log/clear",     .method = HTTP_POST, .handler = handle_log_clear },
    { .uri = "/heap",          .method = HTTP_GET,  .handler = handle_heap },
    { .uri = "/rssi",          .method = HTTP_GET,  .handler = handle_rssi },
    { .uri = "/mqtt-status",   .method = HTTP_GET,  .handler = handle_mqtt_status },
    { .uri = "/favicon.ico",   .method = HTTP_GET,  .handler = handle_favicon },
    { .uri = "/reset",         .method = HTTP_POST, .handler = handle_reset },
    { .uri = "/api/status",    .method = HTTP_GET,  .handler = handle_api_status },
    { .uri = "/api/log",       .method = HTTP_GET,  .handler = handle_api_log },
    { .uri = "/api/config",    .method = HTTP_GET,  .handler = handle_api_config_get },
    { .uri = "/api/config",    .method = HTTP_POST, .handler = handle_api_config_post },
    { .uri = "/api/network",   .method = HTTP_POST, .handler = handle_api_network },
    { .uri = "/api/setup-ap",  .method = HTTP_POST, .handler = handle_api_setup_ap },
    { .uri = "/api/reboot",    .method = HTTP_POST, .handler = handle_api_reboot },
    { .uri = "/api/ota",       .method = HTTP_POST, .handler = handle_ota },
};

static void web_restart_hook(void)
{
    s_log_capture = false;
    if (s_orig_vprintf) {
        esp_log_set_vprintf(s_orig_vprintf);
        s_orig_vprintf = NULL;
    }
    if (s_server) {                            /* closes listen + internal UDP ctrl socket */
        httpd_stop(s_server);
        s_server = NULL;
    }
}

esp_err_t web_server_start(uint32_t heap_initial)
{
    s_heap_initial = heap_initial;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port      = 80;
    cfg.max_uri_handlers = RF_CMD_COUNT + sizeof(s_routes) / sizeof(s_routes[0]) + 3 /* peers */ + 4;
    cfg.stack_size       = 8192;
    cfg.lru_purge_enable = true;
    cfg.ctrl_port        = 32769;              /* avoid a bind conflict right after an OTA reboot */

    esp_err_t err = httpd_start(&s_server, &cfg);
    if (err != ESP_OK) return err;
    for (size_t i = 0; i < sizeof(s_routes) / sizeof(s_routes[0]); i++)
        httpd_register_uri_handler(s_server, &s_routes[i]);
    for (int c = 0; c < RF_CMD_COUNT; c++) {  /* the 22 POST command endpoints of 2.3 */
        httpd_uri_t u = { .uri = rf_cmd_info(c)->path, .method = HTTP_POST, .handler = handle_cmd,
                          .user_ctx = (void *)(intptr_t)c };
        httpd_register_uri_handler(s_server, &u);
    }
    peers_web_start(s_server, NVS_CONFIG_NS, CONFIG_NOVY_DEVICE_NAME);
    httpd_register_err_handler(s_server, HTTPD_404_NOT_FOUND, handle_not_found);
    httpd_register_err_handler(s_server, HTTPD_405_METHOD_NOT_ALLOWED, handle_not_found);
    restart_add_hook(web_restart_hook);

    s_orig_vprintf = esp_log_set_vprintf(log_vprintf);    /* after the server is up */
    s_log_capture  = true;
    ESP_LOGI(TAG, "HTTP server on port 80");
    return ESP_OK;
}
