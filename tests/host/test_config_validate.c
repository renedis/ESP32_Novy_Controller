/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include <stdio.h>
#include "check.h"
#include "config_validate.h"

static novy_cfg_t valid(void)
{
    novy_cfg_t c;
    cfg_set_defaults(&c, "NOVY-RF");
    snprintf(c.net.wifi_ssid, sizeof(c.net.wifi_ssid), "%s", "home net");
    snprintf(c.net.wifi_pass, sizeof(c.net.wifi_pass), "%s", "password1");
    return c;
}

static void test_defaults(void)
{
    novy_cfg_t c;
    cfg_set_defaults(&c, "NOVY-RF");
    CHECK(c.net.dhcp);                                         /* a DHCP reservation fixes the IP */
    CHECK(strcmp(c.net.hostname, "NOVY-RF") == 0);
    CHECK(strcmp(c.net.ip, "192.168.1.50") == 0 && strcmp(c.net.netmask, "255.255.255.0") == 0);
    CHECK(strcmp(c.net.gateway, "192.168.1.1") == 0 && c.net.dns1[0] == '\0');   /* empty: gateway */
    CHECK(c.net.tx_power_qdbm == 80);                          /* 20 dBm: 2.3 never set it */
    CHECK(strcmp(c.ntp1, "pool.ntp.org") == 0 && c.ntp2[0] == '\0');
    CHECK(strcmp(c.tz, "CET-1CEST,M3.5.0,M10.5.0/3") == 0);
    CHECK(!c.mqtt_en && strcmp(c.mqtt_prefix, "ESP") == 0);
    CHECK(cfg_validate(&c) != NULL);                           /* no WiFi yet */
    c = valid();
    CHECK(cfg_validate(&c) == NULL);
}

static void test_prefix(void)
{
    CHECK(cfg_prefix_valid("ESP") && cfg_prefix_valid("home/esp") && cfg_prefix_valid("a-b_c"));
    CHECK(!cfg_prefix_valid("") && !cfg_prefix_valid("a b") && !cfg_prefix_valid("a#") && !cfg_prefix_valid("a+"));
    CHECK(!cfg_prefix_valid("/ESP") && !cfg_prefix_valid("ESP/") && !cfg_prefix_valid("a\"b") && !cfg_prefix_valid("a\\b"));
    CHECK(!cfg_prefix_valid("tab\there"));
    CHECK(cfg_prefix_valid("abcdefghijabcdefghijabcdefghij1") && !cfg_prefix_valid("abcdefghijabcdefghijabcdefghij12"));
}

static void test_fields(void)
{
    novy_cfg_t c;
    c = valid(); snprintf(c.mqtt_prefix, sizeof(c.mqtt_prefix), "%s", "a b");       CHECK(cfg_validate(&c) != NULL);
    c = valid(); c.mqtt_en = true; c.mqtt_uri[0] = '\0';                          CHECK(cfg_validate(&c) != NULL);
    c = valid(); c.mqtt_en = true; snprintf(c.mqtt_uri, sizeof(c.mqtt_uri), "%s", "192.168.1.10"); CHECK(cfg_validate(&c) == NULL);
    c = valid(); c.tz[0] = '\0';                                                  CHECK(cfg_validate(&c) != NULL);
    c = valid(); snprintf(c.ntp1, sizeof(c.ntp1), "%s", "a b");                   CHECK(cfg_validate(&c) != NULL);
    c = valid(); c.ntp1[0] = '\0';                                                CHECK(cfg_validate(&c) == NULL);
    c = valid(); c.net.dhcp = false; snprintf(c.net.gateway, sizeof(c.net.gateway), "%s", "1.2.3.4"); CHECK(cfg_validate(&c) != NULL);
    c = valid(); c.net.dhcp = true;  snprintf(c.net.gateway, sizeof(c.net.gateway), "%s", "1.2.3.4"); CHECK(cfg_validate(&c) == NULL);
}

int main(void)
{
    test_defaults();
    test_prefix();
    test_fields();
    CHECK_DONE();
}
