/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    bool     sta_connected;
    bool     ap_active;
    bool     trial;               /* trying new (pending) network settings */
    char     ssid[33];
    char     ip[16];              /* current values from the netif */
    char     netmask[16];
    char     gateway[16];
    char     dns[16];
    bool     have_rssi;
    int8_t   rssi;
    uint32_t ap_manual_left_s;
} wifi_info_t;

esp_err_t wifi_mgr_start(void);
bool      wifi_mgr_sta_connected(void);
bool      wifi_mgr_ap_active(void);
void      wifi_mgr_start_setup_ap(void);          /* manual, 10 minutes */
void      wifi_mgr_get_info(wifi_info_t *out);
uint32_t  wifi_mgr_noip_ms(void);                 /* time without an IP, 0 while connected */
int       wifi_mgr_rssi(void);                    /* WiFi.RSSI() of 2.3: 0 when not connected */
bool      wifi_mgr_ip(char *out, size_t len);     /* current STA address */
