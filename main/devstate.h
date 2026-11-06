/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
/* Tracked device state of firmware 2.3 (handleButtonPress()) and the MQTT messages of a press. */
#pragma once
#include <stdbool.h>

typedef struct {
    bool novy_light;
    int  fan_speed;                 /* 0 = off, 1-6 */
    bool fan_light;
    bool fan_reverse;               /* fan_direction: false = "forward", true = "reverse" */
} devstate_t;

typedef struct {
    char topic[128];
    char payload[16];
    bool retain;
} mqtt_msg_t;

#define DEVSTATE_PRESS_MSGS 6

void devstate_init(devstate_t *s);              /* OFF, 0, OFF, forward (RAM only, as 2.3) */
void devstate_apply(devstate_t *s, int cmd);    /* the switch in handleButtonPress() */
const char *devstate_direction(const devstate_t *s);

/* After a press, in the sketch's order: <base>/state/{novy_light,fan_speed,fan_light,fan_direction}
 * (retained), then <base>/button/<id>/state "PRESS" and "" (not retained). base = "<prefix>/<device>", e.g. "ESP/NOVY-RF".
 * Returns the count, 0 when cmd is invalid or cap < DEVSTATE_PRESS_MSGS. */
int devstate_press_msgs(const devstate_t *s, int cmd, const char *base, mqtt_msg_t *out, int cap);
