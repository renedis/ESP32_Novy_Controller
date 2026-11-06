/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include <stdio.h>
#include <string.h>
#include "light.h"

void light_init(light_t *l)
{
    l->on         = false;
    l->brightness = 255;
    l->r = l->g = l->b = 255;
}

light_topic_t light_topic(const char *suffix)
{
    if (strcmp(suffix, "rgb_light/set") == 0)              return LIGHT_TOPIC_STATE;
    if (strcmp(suffix, "rgb_light/brightness/state") == 0) return LIGHT_TOPIC_BRIGHTNESS;
    if (strcmp(suffix, "rgb_light/rgb/state") == 0)        return LIGHT_TOPIC_RGB;
    return LIGHT_TOPIC_NONE;
}

/* 1-3 decimal digits, 0-255; *end gets the first character after them. */
static bool byte_at(const char *s, const char **end, uint8_t *out)
{
    unsigned v = 0;
    int      n = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (unsigned)(*s++ - '0');
        if (++n > 3) return false;
    }
    if (n == 0 || v > 255) return false;
    *end = s;
    *out = (uint8_t)v;
    return true;
}

bool light_apply(light_t *l, light_topic_t t, const char *p)
{
    const char *e;
    uint8_t     v[3];
    switch (t) {
    case LIGHT_TOPIC_STATE:
        if (strcmp(p, "ON") == 0)  { l->on = true;  return true; }
        if (strcmp(p, "OFF") == 0) { l->on = false; return true; }
        return false;
    case LIGHT_TOPIC_BRIGHTNESS:
        if (!byte_at(p, &e, &v[0]) || *e) return false;
        l->brightness = v[0];
        return true;
    case LIGHT_TOPIC_RGB:
        for (int i = 0; i < 3; i++) {
            if (!byte_at(p, &e, &v[i])) return false;
            if (*e != (i < 2 ? ',' : '\0')) return false;
            p = e + 1;
        }
        l->r = v[0];
        l->g = v[1];
        l->b = v[2];
        return true;
    default:
        return false;
    }
}

int light_state_json(const light_t *l, char *out, size_t len)
{
    int n = snprintf(out, len, "{\"state\":\"%s\",\"brightness\":%u,\"rgb\":[%u,%u,%u]}", l->on ? "ON" : "OFF",
                     l->brightness, l->r, l->g, l->b);
    return (n > 0 && (size_t)n < len) ? n : -1;
}

void light_output(const light_t *l, uint8_t *r, uint8_t *g, uint8_t *b)
{
    unsigned k = l->on ? l->brightness : 0;
    *r = (uint8_t)((l->r * k + 127) / 255);
    *g = (uint8_t)((l->g * k + 127) / 255);
    *b = (uint8_t)((l->b * k + 127) / 255);
}
