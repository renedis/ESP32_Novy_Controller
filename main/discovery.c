/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "rf_cmd.h"
#include "discovery.h"

typedef struct {
    char  *buf;
    size_t cap, len;
    bool   err;
} out_t;

static void raw(out_t *o, const char *s)
{
    size_t n = strlen(s);
    if (o->err || o->len + n >= o->cap) { o->err = true; return; }
    memcpy(o->buf + o->len, s, n + 1);
    o->len += n;
}

/* JSON string body, escaped as ArduinoJson does. */
static void esc(out_t *o, const char *s)
{
    char tmp[8];
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        switch (c) {
        case '"':  raw(o, "\\\""); break;
        case '\\': raw(o, "\\\\"); break;
        case '\b': raw(o, "\\b"); break;
        case '\f': raw(o, "\\f"); break;
        case '\n': raw(o, "\\n"); break;
        case '\r': raw(o, "\\r"); break;
        case '\t': raw(o, "\\t"); break;
        default:
            if (c < 0x20) snprintf(tmp, sizeof(tmp), "\\u%04x", c);
            else { tmp[0] = (char)c; tmp[1] = '\0'; }
            raw(o, tmp);
        }
    }
}

/* ,"key":"<parts...>" — the value is the concatenation of the NULL-terminated parts. */
static void kv(out_t *o, const char *key, ...)
{
    va_list ap;
    if (o->len && o->buf[o->len - 1] != '{') raw(o, ",");
    raw(o, "\"");
    raw(o, key);
    raw(o, "\":\"");
    va_start(ap, key);
    for (const char *p = va_arg(ap, const char *); p; p = va_arg(ap, const char *)) esc(o, p);
    va_end(ap);
    raw(o, "\"");
}

static void device(out_t *o, const char *dev)
{
    raw(o, ",\"device\":{\"hw_version\":1,\"identifiers\":[\"");
    esc(o, dev);
    raw(o, "\"]");
    kv(o, "manufacturer", "renedis", NULL);
    kv(o, "model", "Novy + Fan RF Controller", NULL);
    kv(o, "model_id", "NOVYFANRF2.3", NULL);
    kv(o, "name", dev, " NovyFanController", NULL);
    kv(o, "sw_version", "2.3", NULL);
    kv(o, "via_device", "", NULL);
    raw(o, "}");
}

static void availability(out_t *o, const char *pre, const char *dev)
{
    kv(o, "availability_topic", pre, "/", dev, "/status", NULL);
    kv(o, "payload_available", "online", NULL);
    kv(o, "payload_not_available", "offline", NULL);
}

static void button(out_t *o, int cmd, const char *pre, const char *dev)
{
    const rf_cmd_info_t *b = rf_cmd_info(cmd);
    raw(o, "{");
    kv(o, "name", b->name, NULL);
    kv(o, "unique_id", dev, "_", b->id, NULL);
    kv(o, "icon", b->icon, NULL);
    kv(o, "command_topic", pre, "/", dev, "/button/", b->id, "/set", NULL);
    kv(o, "state_topic", pre, "/", dev, "/button/", b->id, "/state", NULL);
    availability(o, pre, dev);
    device(o, dev);
    raw(o, "}");
}

static void light(out_t *o, const char *pre, const char *dev)
{
    char low[64];
    size_t i;
    for (i = 0; dev[i] && i < sizeof(low) - 1; i++) low[i] = (char)tolower((unsigned char)dev[i]);
    low[i] = '\0';
    raw(o, "{");
    kv(o, "name", "LED", NULL);
    kv(o, "unique_id", dev, "_rgb_light", NULL);
    kv(o, "command_topic", pre, "/", dev, "/rgb_light/set", NULL);
    kv(o, "state_topic", pre, "/", dev, "/rgb_light/status", NULL);
    kv(o, "brightness_state_topic", pre, "/", dev, "/rgb_light/brightness/set", NULL);     /* sic, as 2.3 */
    kv(o, "brightness_command_topic", pre, "/", dev, "/rgb_light/brightness/state", NULL);
    kv(o, "rgb_state_topic", pre, "/", dev, "/rgb_light/rgb/set", NULL);
    kv(o, "rgb_command_topic", pre, "/", dev, "/rgb_light/rgb/state", NULL);
    kv(o, "state_value_template", "{{ value_json.state }}", NULL);
    kv(o, "brightness_value_template", "{{ value_json.brightness }}", NULL);
    kv(o, "rgb_value_template", "{{ value_json.rgb | join(',') }}", NULL);
    kv(o, "payload_on", "ON", NULL);
    kv(o, "payload_off", "OFF", NULL);
    kv(o, "object_id", "led_", low, NULL);
    raw(o, ",\"origin\":{");
    kv(o, "name", "Custom MQTT", NULL);
    kv(o, "sw", "1.7", NULL);
    raw(o, "}");
    device(o, dev);
    availability(o, pre, dev);
    raw(o, "}");
}

bool disc_build(int i, const char *prefix, const char *device_name,
                char *topic, size_t topic_len, char *payload, size_t payload_len)
{
    int n;
    out_t o = { payload, payload_len, 0, false };
    if (i < 0 || i >= DISC_COUNT || payload_len == 0) return false;
    payload[0] = '\0';
    if (i == DISC_LIGHT) {
        n = snprintf(topic, topic_len, "homeassistant/light/%s_rgb_light/config", device_name);
        light(&o, prefix, device_name);
    } else {
        n = snprintf(topic, topic_len, "homeassistant/button/%s/%s/config", device_name, rf_cmd_info(i)->id);
        button(&o, i, prefix, device_name);
    }
    return n > 0 && (size_t)n < topic_len && !o.err;
}
