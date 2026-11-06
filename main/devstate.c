/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include <stdio.h>
#include "rf_cmd.h"
#include "devstate.h"

void devstate_init(devstate_t *s)
{
    s->novy_light  = false;
    s->fan_speed   = 0;
    s->fan_light   = false;
    s->fan_reverse = false;
}

void devstate_apply(devstate_t *s, int cmd)
{
    switch (cmd) {
    case RF_CMD_LIGHT:       s->novy_light = !s->novy_light; break;
    case RF_CMD_FAN_LIGHT:   s->fan_light = !s->fan_light; break;
    case RF_CMD_FAN_ALL_OFF: s->fan_speed = 0; s->fan_light = false; break;
    case RF_CMD_FAN_OFF:     s->fan_speed = 0; break;
    case RF_CMD_FAN_SPEED1:  s->fan_speed = 1; break;
    case RF_CMD_FAN_SPEED2:  s->fan_speed = 2; break;
    case RF_CMD_FAN_SPEED3:  s->fan_speed = 3; break;
    case RF_CMD_FAN_SPEED4:  s->fan_speed = 4; break;
    case RF_CMD_FAN_SPEED5:  s->fan_speed = 5; break;
    case RF_CMD_FAN_SPEED6:  s->fan_speed = 6; break;
    case RF_CMD_FAN_FORWARD: s->fan_reverse = false; break;
    case RF_CMD_FAN_REVERSE: s->fan_reverse = true; break;
    default: break;
    }
}

const char *devstate_direction(const devstate_t *s)
{
    return s->fan_reverse ? "reverse" : "forward";
}

static void msg(mqtt_msg_t *m, const char *base, const char *mid, const char *leaf, const char *payload, bool retain)
{
    snprintf(m->topic, sizeof(m->topic), "%s/%s%s", base, mid, leaf);
    snprintf(m->payload, sizeof(m->payload), "%s", payload);
    m->retain = retain;
}

int devstate_press_msgs(const devstate_t *s, int cmd, const char *base, mqtt_msg_t *out, int cap)
{
    const rf_cmd_info_t *info = rf_cmd_info(cmd);
    char speed[8], button[48];
    if (!info || cap < DEVSTATE_PRESS_MSGS) return 0;
    snprintf(speed, sizeof(speed), "%d", s->fan_speed);
    snprintf(button, sizeof(button), "%s/state", info->id);
    msg(&out[0], base, "state/", "novy_light", s->novy_light ? "ON" : "OFF", true);
    msg(&out[1], base, "state/", "fan_speed", speed, true);
    msg(&out[2], base, "state/", "fan_light", s->fan_light ? "ON" : "OFF", true);
    msg(&out[3], base, "state/", "fan_direction", devstate_direction(s), true);
    msg(&out[4], base, "button/", button, "PRESS", false);
    msg(&out[5], base, "button/", button, "", false);
    return DEVSTATE_PRESS_MSGS;
}
