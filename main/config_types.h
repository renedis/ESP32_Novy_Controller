/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    char    wifi_ssid[33];
    char    wifi_pass[65];
    bool    dhcp;
    char    ip[16];
    char    netmask[16];
    char    gateway[16];
    char    dns1[16];               /* empty: gateway is used */
    char    dns2[16];
    char    hostname[33];
    uint8_t tx_power_qdbm;          /* quarter dBm (esp_wifi_set_max_tx_power): 8-80 = 2-20 dBm */
} net_cfg_t;

typedef struct {
    net_cfg_t net;                  /* last-good network settings */
    char      ntp1[64];
    char      ntp2[64];
    char      tz[48];
    bool      mqtt_en;
    char      mqtt_uri[128];
    char      mqtt_user[64];
    char      mqtt_pass[64];
    char      mqtt_prefix[32];      /* topics are <prefix>/<device>/... ("ESP") */
} novy_cfg_t;
