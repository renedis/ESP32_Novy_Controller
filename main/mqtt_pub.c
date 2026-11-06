/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "mqtt_client.h"
#include "sdkconfig.h"
#include "applog.h"
#include "backoff.h"
#include "devstate.h"
#include "discovery.h"
#include "nvs_config.h"
#include "restart.h"
#include "rf_cmd.h"
#include "rf_tx.h"
#include "status_led.h"
#include "wifi_mgr.h"
#include "mqtt_pub.h"

static const char *TAG = "mqtt";

#define DISCOVERY_DELAY_MS 2000           /* 2.3: discoveryDueAt = millis() + 2000 */
#define OUT_QUEUE_LEN      24

static esp_mqtt_client_handle_t s_client;
static SemaphoreHandle_t        s_lock;                /* guards s_client */
static volatile bool            s_connected;
static volatile bool            s_enabled;
static QueueHandle_t            s_out;                 /* mqtt_msg_t from the RF worker */
static char                     s_client_id[48];
static char                     s_prefix[32];
static char                     s_base[80];            /* <prefix>/<device> */
static char                     s_lwt[96];
static backoff_t                s_backoff;
static volatile uint32_t        s_retry_at;            /* 0 = no retry pending */
static volatile uint32_t        s_discovery_due;       /* 0 = none pending */
static bool                     s_discovery_done;      /* once per boot, as 2.3 */
static volatile bool            s_light_dirty;
static bool                     s_start_pending;       /* started once WiFi has an IP */
static int                      s_attempt;
static light_t                  s_light;
static portMUX_TYPE             s_mux = portMUX_INITIALIZER_UNLOCKED;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/* ── Outgoing ─────────────────────────────────────────────────────────── */
static void publish_locked(const char *topic, const char *payload, bool retain)
{
    if (s_client && s_connected) esp_mqtt_client_publish(s_client, topic, payload, 0, 0, retain ? 1 : 0);
}

static void publish_discovery_locked(void)
{
    char *topic = malloc(DISC_TOPIC_MAX), *payload = malloc(DISC_PAYLOAD_MAX);
    if (!topic || !payload) {
        free(topic);
        free(payload);
        return;
    }
    applog("Sending HA Discovery...");
    for (int i = 0; i < DISC_COUNT; i++) {
        if (disc_build(i, s_prefix, CONFIG_NOVY_DEVICE_NAME, topic, DISC_TOPIC_MAX, payload, DISC_PAYLOAD_MAX))
            publish_locked(topic, payload, true);
        vTaskDelay(1);
    }
    free(topic);
    free(payload);
    applog("HA Discovery sent");
}

static void publish_light_locked(void)
{
    static const char *const suffix[] = { "rgb_light/status", "rgb_light/brightness/set", "rgb_light/rgb/set" };
    char    topic[128], js[96];
    light_t l;
    taskENTER_CRITICAL(&s_mux);
    l = s_light;
    taskEXIT_CRITICAL(&s_mux);
    if (light_state_json(&l, js, sizeof(js)) < 0) return;
    for (size_t i = 0; i < sizeof(suffix) / sizeof(suffix[0]); i++) {
        snprintf(topic, sizeof(topic), "%s/%s", s_base, suffix[i]);
        publish_locked(topic, js, true);
    }
}

/* rf_press_fn, from the RF worker: the 2.3 messages of a press (state topics, PRESS, "") */
static void on_press(int cmd, const devstate_t *st)
{
    mqtt_msg_t m[DEVSTATE_PRESS_MSGS];
    if (!s_out || !s_connected) return;                    /* 2.3: if (client.connected()) */
    int n = devstate_press_msgs(st, cmd, s_base, m, DEVSTATE_PRESS_MSGS);
    for (int i = 0; i < n; i++)
        if (xQueueSend(s_out, &m[i], pdMS_TO_TICKS(50)) != pdTRUE) ESP_LOGW(TAG, "Out queue full");
}

/* ── Incoming ─────────────────────────────────────────────────────────── */
static void handle_message(const char *topic, const char *payload)
{
    applog("HA (MQTT) message received [%s] %s", topic, payload);
    size_t bl = strlen(s_base);
    if (strncmp(topic, s_base, bl) != 0 || topic[bl] != '/') return;
    const char *rest = topic + bl + 1;
    if (strncmp(rest, "button/", 7) == 0) {
        char        id[32];
        const char *b   = rest + 7;
        size_t      len = strlen(b);
        if (len <= 4 || strcmp(b + len - 4, "/set") != 0 || len - 4 >= sizeof(id)) return;
        memcpy(id, b, len - 4);
        id[len - 4] = '\0';
        int cmd = rf_cmd_from_id(id);
        if (cmd >= 0) rf_tx_submit(cmd, RF_SRC_MQTT);
        return;
    }
    light_topic_t lt = light_topic(rest);
    if (lt == LIGHT_TOPIC_NONE) return;
    taskENTER_CRITICAL(&s_mux);
    bool    ok = light_apply(&s_light, lt, payload);
    light_t l  = s_light;
    taskEXIT_CRITICAL(&s_mux);
    if (ok) {
        status_led_set_light(&l);
        s_light_dirty = true;
    }
}

static void subscribe_all(esp_mqtt_client_handle_t c)
{
    static const char *const light[] = { "rgb_light/set", "rgb_light/brightness/state", "rgb_light/rgb/state" };
    char t[128];
    for (int i = 0; i < RF_CMD_COUNT; i++) {
        snprintf(t, sizeof(t), "%s/button/%s/set", s_base, rf_cmd_info(i)->id);
        esp_mqtt_client_subscribe(c, t, 0);
    }
    for (size_t i = 0; i < sizeof(light) / sizeof(light[0]); i++) {
        snprintf(t, sizeof(t), "%s/%s", s_base, light[i]);
        esp_mqtt_client_subscribe(c, t, 0);
    }
}

static void mqtt_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    esp_mqtt_event_handle_t ev = (esp_mqtt_event_handle_t)data;
    switch (id) {
    case MQTT_EVENT_BEFORE_CONNECT:
        applog("Connecting to MQTT... attempt %d", ++s_attempt);
        break;
    case MQTT_EVENT_CONNECTED:
        applog("Connected to MQTT");
        subscribe_all(ev->client);
        if (!s_discovery_done) s_discovery_due = now_ms() + DISCOVERY_DELAY_MS;
        esp_mqtt_client_publish(ev->client, s_lwt, "online", 0, 0, 1);
        backoff_reset(&s_backoff);
        s_attempt     = 0;
        s_retry_at    = 0;
        s_light_dirty = true;
        s_connected   = true;
        break;
    case MQTT_EVENT_DISCONNECTED: {
        uint32_t wait = backoff_next(&s_backoff);
        if (s_connected) applog("MQTT connection lost");
        else             applog("MQTT failed, retrying in %lu s", (unsigned long)(wait / 1000));
        s_connected = false;
        s_retry_at  = now_ms() + wait;
        if (s_retry_at == 0) s_retry_at = 1;
        break;
    }
    case MQTT_EVENT_DATA: {
        char topic[128], payload[64];
        if (ev->current_data_offset != 0 || ev->data_len != ev->total_data_len) break;   /* > buffer, as PubSubClient */
        if (ev->topic_len <= 0 || ev->topic_len >= (int)sizeof(topic)) break;
        int dl = ev->data_len < (int)sizeof(payload) - 1 ? ev->data_len : (int)sizeof(payload) - 1;  /* cut: buttons ignore it, light values are short */
        memcpy(topic, ev->topic, ev->topic_len);
        topic[ev->topic_len] = '\0';
        memcpy(payload, ev->data, dl);
        payload[dl] = '\0';
        handle_message(topic, payload);
        break;
    }
    case MQTT_EVENT_ERROR:
        if (ev->error_handle && ev->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
            ESP_LOGW(TAG, "Broker refused the connection (code %d)", ev->error_handle->connect_return_code);
        break;
    default:
        break;
    }
}

/* ── Client lifecycle ─────────────────────────────────────────────────── */
static void stop_locked(void)
{
    if (!s_client) return;
    if (s_connected) esp_mqtt_client_publish(s_client, s_lwt, "offline", 0, 0, 1);
    esp_mqtt_client_stop(s_client);
    esp_mqtt_client_destroy(s_client);
    s_client        = NULL;
    s_connected     = false;
    s_retry_at      = 0;
    s_start_pending = false;
}

esp_err_t mqtt_pub_init(void)
{
    uint8_t mac[6];
    s_lock = xSemaphoreCreateMutex();
    s_out  = xQueueCreate(OUT_QUEUE_LEN, sizeof(mqtt_msg_t));
    if (!s_lock || !s_out) return ESP_ERR_NO_MEM;
    esp_read_mac(mac, ESP_MAC_WIFI_STA);        /* WiFi.macAddress() of 2.3, colons removed */
    snprintf(s_client_id, sizeof(s_client_id), "%s_%02X%02X%02X%02X%02X%02X", CONFIG_NOVY_DEVICE_NAME,
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    backoff_init(&s_backoff, 1000, 60000);
    light_init(&s_light);
    rf_tx_set_press_handler(on_press);
    restart_add_hook(mqtt_pub_stop);
    return ESP_OK;
}

void mqtt_pub_stop(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    stop_locked();
    xSemaphoreGive(s_lock);
}

void mqtt_pub_reconfigure(void)
{
    novy_cfg_t cfg;
    char       uri[160];
    nvs_config_copy(&cfg);

    xSemaphoreTake(s_lock, portMAX_DELAY);
    stop_locked();
    s_enabled = cfg.mqtt_en && cfg.mqtt_uri[0];
    if (!s_enabled) {
        ESP_LOGI(TAG, "MQTT disabled");
        xSemaphoreGive(s_lock);
        return;
    }
    if (strstr(cfg.mqtt_uri, "://") == NULL) snprintf(uri, sizeof(uri), "mqtt://%s", cfg.mqtt_uri);
    else                                     strlcpy(uri, cfg.mqtt_uri, sizeof(uri));
    if (strcmp(s_prefix, cfg.mqtt_prefix) != 0) s_discovery_done = false;   /* new topics: announce again */
    strlcpy(s_prefix, cfg.mqtt_prefix, sizeof(s_prefix));
    snprintf(s_base, sizeof(s_base), "%s/%s", cfg.mqtt_prefix, CONFIG_NOVY_DEVICE_NAME);
    snprintf(s_lwt, sizeof(s_lwt), "%s/status", s_base);

    esp_mqtt_client_config_t mc = {             /* esp-mqtt copies all strings */
        .broker.address.uri                  = uri,
        .credentials.username                = cfg.mqtt_user[0] ? cfg.mqtt_user : NULL,
        .credentials.authentication.password = cfg.mqtt_pass[0] ? cfg.mqtt_pass : NULL,
        .credentials.client_id               = s_client_id,
        .session.keepalive                   = 15,          /* PubSubClient MQTT_KEEPALIVE */
        .session.protocol_ver                = MQTT_PROTOCOL_V_3_1_1,
        .session.last_will = {
            .topic  = s_lwt,
            .msg    = "offline",
            .qos    = 0,
            .retain = 1,
        },
        .network.disable_auto_reconnect = true,             /* our backoff drives reconnects */
        .buffer.size                    = 2048,             /* PubSubClient setBufferSize(2048) */
    };
    s_client = esp_mqtt_client_init(&mc);
    if (s_client) {
        esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
        s_start_pending = true;
        ESP_LOGI(TAG, "MQTT client %s -> %s (topics %s/...)", s_client_id, uri, s_base);
    } else {
        ESP_LOGE(TAG, "MQTT client init failed");
    }
    xSemaphoreGive(s_lock);
}

bool mqtt_pub_is_connected(void)
{
    return s_connected;
}

bool mqtt_pub_enabled(void)
{
    return s_enabled;
}

const char *mqtt_pub_client_id(void)
{
    return s_client_id;
}

void mqtt_pub_get_light(light_t *out)
{
    taskENTER_CRITICAL(&s_mux);
    *out = s_light;
    taskEXIT_CRITICAL(&s_mux);
}

void mqtt_pub_task(void *arg)
{
    esp_task_wdt_add(NULL);
    for (;;) {
        mqtt_msg_t m;
        bool       got = xQueueReceive(s_out, &m, pdMS_TO_TICKS(100)) == pdTRUE;
        esp_task_wdt_reset();
        uint32_t now = now_ms();

        xSemaphoreTake(s_lock, portMAX_DELAY);
        if (got) publish_locked(m.topic, m.payload, m.retain);
        if (s_client && s_connected) {
            uint32_t due = s_discovery_due;
            if (due && (int32_t)(now - due) >= 0) {
                s_discovery_due  = 0;
                s_discovery_done = true;
                publish_discovery_locked();
            }
            if (s_light_dirty) {
                s_light_dirty = false;
                publish_light_locked();
            }
        } else if (s_client && s_start_pending && wifi_mgr_sta_connected()) {
            s_start_pending = false;
            esp_mqtt_client_start(s_client);
        } else if (s_client && s_retry_at && (int32_t)(now - s_retry_at) >= 0 && wifi_mgr_sta_connected()) {
            s_retry_at = 0;
            if (esp_mqtt_client_reconnect(s_client) != ESP_OK) s_retry_at = now + 1000;   /* not waiting yet */
        }
        xSemaphoreGive(s_lock);
    }
}
