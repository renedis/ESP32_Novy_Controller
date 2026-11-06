/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
/* MQTT as firmware 2.3: client id <device>_<MAC>, keepalive 15 s, LWT <prefix>/<device>/status,
 * button commands, retained state topics, discovery once per boot 2 s after connecting, reconnect
 * backoff 1 s -> 60 s. Plus the working RGB light entity. */
#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "light.h"

esp_err_t mqtt_pub_init(void);          /* ids, queues, RF press handler, restart hook */
void      mqtt_pub_reconfigure(void);   /* (re)start the client from the saved settings */
void      mqtt_pub_stop(void);          /* publishes "offline" first when connected */
void      mqtt_pub_task(void *arg);
bool      mqtt_pub_is_connected(void);
bool      mqtt_pub_enabled(void);
void      mqtt_pub_get_light(light_t *out);
const char *mqtt_pub_client_id(void);
