/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include <stdio.h>
#include <string.h>
#include "net_config.h"
#include "config_validate.h"

#define SET(dst, src) snprintf((dst), sizeof(dst), "%s", (src))

void cfg_set_defaults(novy_cfg_t *c, const char *device_name)
{
    memset(c, 0, sizeof(*c));
    c->net.dhcp = true;                         /* a DHCP reservation gives a fixed address */
    SET(c->net.ip, "192.168.1.50");             /* static fields: CONFIG_NOVY_STATIC_* override them */
    SET(c->net.netmask, "255.255.255.0");
    SET(c->net.gateway, "192.168.1.1");
    SET(c->net.hostname, device_name);
    c->net.tx_power_qdbm = 80;                  /* 20 dBm: firmware 2.3 ran at the default maximum */
    SET(c->ntp1, "pool.ntp.org");
    SET(c->tz, "CET-1CEST,M3.5.0,M10.5.0/3");
    c->mqtt_en = false;
    SET(c->mqtt_prefix, "ESP");
}

bool cfg_prefix_valid(const char *p)
{
    size_t n = strlen(p);
    if (n == 0 || n >= sizeof(((novy_cfg_t *)0)->mqtt_prefix) || p[0] == '/' || p[n - 1] == '/') return false;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)p[i];
        if (c <= 0x20 || c > 0x7E || c == '#' || c == '+' || c == '"' || c == '\\') return false;
    }
    return true;
}

const char *cfg_validate(const novy_cfg_t *c)
{
    const char *why = net_validate(&c->net);
    if (why) return why;
    if (strchr(c->ntp1, ' ') || strchr(c->ntp2, ' ')) return "NTP server must not contain spaces";
    if (c->tz[0] == '\0' || strchr(c->tz, ' ')) return "Timezone must be a POSIX TZ string without spaces";
    if (!cfg_prefix_valid(c->mqtt_prefix))
        return "MQTT prefix: 1-31 printable characters, no spaces, quotes, backslash, # or +, no '/' at either end";
    if (c->mqtt_en && c->mqtt_uri[0] == '\0') return "MQTT broker URI is empty";
    return NULL;
}
