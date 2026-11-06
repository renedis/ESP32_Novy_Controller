/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
/* The "LED" light entity of the 2.3 discovery, served the way Home Assistant uses it: commands on
 * rgb_light/set (ON/OFF), rgb_light/brightness/state (0-255) and rgb_light/rgb/state (r,g,b); state
 * as one JSON on rgb_light/status, rgb_light/brightness/set and rgb_light/rgb/set. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    bool    on;
    uint8_t brightness;              /* 0-255, Home Assistant scale */
    uint8_t r, g, b;
} light_t;

typedef enum {
    LIGHT_TOPIC_NONE = 0,
    LIGHT_TOPIC_STATE,               /* rgb_light/set */
    LIGHT_TOPIC_BRIGHTNESS,          /* rgb_light/brightness/state */
    LIGHT_TOPIC_RGB,                 /* rgb_light/rgb/state */
} light_topic_t;

void          light_init(light_t *l);                          /* OFF, 255, white */
light_topic_t light_topic(const char *suffix);                 /* topic after "<prefix>/<device>/" */
bool          light_apply(light_t *l, light_topic_t t, const char *payload);   /* false = ignored */
int           light_state_json(const light_t *l, char *out, size_t len);
void          light_output(const light_t *l, uint8_t *r, uint8_t *g, uint8_t *b);   /* 0 when OFF */
