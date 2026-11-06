/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */

#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "nvs.h"
#include "cJSON.h"
#include "peers.h"
#include "peers_web.h"

static const char *TAG = "peers";

#define NVS_KEY  "peers"
#define BODY_MAX 2048           /* the JSON body; the list itself is capped by peers_parse() */

extern const char peers_js_start[] asm("_binary_peers_js_start");
extern const char peers_js_end[]   asm("_binary_peers_js_end");

static peers_t     s_peers;     /* live list, httpd task only */
static const char *s_ns;
static const char *s_self;

static void load(void)
{
    char *text = calloc(1, PEERS_TEXT_MAX + 1);
    if (!text) return;
    nvs_handle_t h;
    size_t len = PEERS_TEXT_MAX + 1;
    if (nvs_open(s_ns, NVS_READONLY, &h) == ESP_OK) {
        if (nvs_get_str(h, NVS_KEY, text, &len) != ESP_OK) text[0] = '\0';
        nvs_close(h);
    }
    const char *why = peers_parse(text, &s_peers);
    if (why) ESP_LOGW(TAG, "Stored peer list ignored: %s", why);
    ESP_LOGI(TAG, "%d peers", s_peers.count);
    free(text);
}

static esp_err_t save(const peers_t *p)
{
    char *text = malloc(PEERS_TEXT_MAX + 1);
    if (!text) return ESP_ERR_NO_MEM;
    esp_err_t err = ESP_ERR_INVALID_SIZE;
    if (peers_serialise(p, text, PEERS_TEXT_MAX + 1) >= 0) {
        nvs_handle_t h;
        err = nvs_open(s_ns, NVS_READWRITE, &h);
        if (err == ESP_OK) {
            err = nvs_set_str(h, NVS_KEY, text);
            if (err == ESP_OK) err = nvs_commit(h);
            nvs_close(h);
        }
    }
    free(text);
    return err;
}

static esp_err_t send_list(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "self", s_self ? s_self : "");
    cJSON *list = cJSON_AddArrayToObject(root, "peers");
    for (int i = 0; i < s_peers.count; i++) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "name", s_peers.item[i].label);
        cJSON_AddStringToObject(o, "url", s_peers.item[i].url);
        cJSON_AddItemToArray(list, o);
    }
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    esp_err_t err = httpd_resp_sendstr(req, json);
    free(json);
    return err;
}

/* ── GET /peers.js ───────────────────────────────────────────────────── */
static esp_err_t handle_js(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/javascript");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    return httpd_resp_send(req, peers_js_start, (ssize_t)(peers_js_end - peers_js_start - 1));
}

/* ── GET /api/peers ──────────────────────────────────────────────────── */
static esp_err_t handle_get(httpd_req_t *req)
{
    return send_list(req);
}

/* ── POST /api/peers ─────────────────────────────────────────────────── */
static esp_err_t handle_post(httpd_req_t *req)
{
    if (req->content_len == 0 || req->content_len > BODY_MAX)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid body size");
    char *body = malloc(req->content_len + 1);
    if (!body) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
    size_t total = 0;
    while (total < req->content_len) {
        int r = httpd_req_recv(req, body + total, req->content_len - total);
        if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (r <= 0) {
            free(body);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Receive error");
        }
        total += r;
    }
    body[total] = '\0';
    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad JSON");

    cJSON   *v = cJSON_GetObjectItem(root, "peers");
    peers_t *p = malloc(sizeof(*p));
    const char *why = !cJSON_IsString(v) ? "Expected {\"peers\": \"Label=URL;Label=URL\"}"
                    : !p                 ? "OOM"
                    : peers_parse(v->valuestring, p);
    cJSON_Delete(root);
    if (why) {
        free(p);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, why);
    }
    if (save(p) != ESP_OK) {
        free(p);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS write failed");
    }
    s_peers = *p;                       /* applied: the next page load shows the new bar */
    free(p);
    ESP_LOGI(TAG, "Peer list saved: %d peers", s_peers.count);
    return send_list(req);
}

esp_err_t peers_web_start(httpd_handle_t server, const char *nvs_ns, const char *self)
{
    static const httpd_uri_t routes[] = {
        { .uri = "/peers.js",  .method = HTTP_GET,  .handler = handle_js },
        { .uri = "/api/peers", .method = HTTP_GET,  .handler = handle_get },
        { .uri = "/api/peers", .method = HTTP_POST, .handler = handle_post },
    };
    s_ns   = nvs_ns;
    s_self = self;
    load();
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        esp_err_t err = httpd_register_uri_handler(server, &routes[i]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Cannot register %s: %s", routes[i].uri, esp_err_to_name(err));
            return err;
        }
    }
    return ESP_OK;
}
