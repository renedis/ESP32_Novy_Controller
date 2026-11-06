/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#pragma once
#include "config_types.h"

/* Non-secret defaults (hostname = device name); nvs_config adds WiFi and MQTT from Kconfig. */
void        cfg_set_defaults(novy_cfg_t *c, const char *device_name);
const char *cfg_validate(const novy_cfg_t *c);       /* NULL = valid, else a reason */
bool        cfg_prefix_valid(const char *p);         /* MQTT topic prefix */
