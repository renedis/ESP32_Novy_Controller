/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
/* GET /heap, /rssi and /mqtt-status of firmware 2.3: the same helper formulas and the same JSON
 * (ArduinoJson key order, no spaces). */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

int         sf_rssi_percentage(int rssi);         /* getRSSIPercentage() */
const char *sf_rssi_color(int rssi);              /* getRSSIStatusColor() */
int         sf_rssi_bars(int rssi);               /* getRSSIBars() */
int         sf_heap_percentage(uint32_t current, uint32_t initial);   /* getHeapPercentage() */
const char *sf_heap_color(int percentage);        /* getHeapStatusColor() */

int sf_heap_json(char *out, size_t len, uint32_t current, uint32_t initial, uint32_t minimum);
int sf_rssi_json(char *out, size_t len, int rssi);
int sf_mqtt_json(char *out, size_t len, bool connected);
