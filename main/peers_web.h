/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#pragma once
#include "esp_err.h"
#include "esp_http_server.h"

/* Peer bar backend (list format in peers.h). Loads the list from the NVS string key
 * "peers" in namespace nvs_ns and registers three URI handlers on server:
 *   GET  /peers.js    bar script included by every page (fills <nav id="peers">)
 *   GET  /api/peers   {"self":"<name>","peers":[{"name":"..","url":".."},...]}
 *   POST /api/peers   {"peers":"Label=URL;..."}: validate, save, apply at once and
 *                     reply like GET; HTTP 400 with the reason when invalid
 * self is this device's name (its entry is highlighted instead of linked); the string
 * must stay valid, it is read on every request. The list is only used by the httpd task. */
esp_err_t peers_web_start(httpd_handle_t server, const char *nvs_ns, const char *self);
