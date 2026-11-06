/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
/* Home Assistant discovery of firmware 2.3 (sendHomeAssistantDiscovery()), byte for byte: the
 * 22 buttons in table order, then the RGB light. Keys in ArduinoJson insertion order. */
#pragma once
#include <stdbool.h>
#include <stddef.h>

#define DISC_COUNT       23              /* 22 buttons + LED light */
#define DISC_LIGHT       22
#define DISC_TOPIC_MAX   128
#define DISC_PAYLOAD_MAX 1280

/* prefix = MQTT_PREFIX ("ESP"), device = HOSTNAME, e.g. "NOVY-RF". False when i is out of range or
 * a buffer is too small. */
bool disc_build(int i, const char *prefix, const char *device,
                char *topic, size_t topic_len, char *payload, size_t payload_len);
