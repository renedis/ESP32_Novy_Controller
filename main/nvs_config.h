/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "config_types.h"

#define NVS_CONFIG_NS "novyrf"                           /* own namespace; Arduino leftovers stay */

esp_err_t nvs_config_init(void);                         /* load, or defaults + Kconfig secrets */
void      nvs_config_copy(novy_cfg_t *out);              /* thread-safe snapshot */
esp_err_t nvs_config_save(const novy_cfg_t *cfg);        /* caller validated with cfg_validate() */

/* Network settings to try at the next boot (safe apply). take() loads and erases them. */
esp_err_t nvs_config_save_pending_net(const net_cfg_t *net);
bool      nvs_config_take_pending_net(net_cfg_t *out);

/* Setup AP SSID and password: NVS keys "ap_ssid"/"ap_pass", else CONFIG_NOVY_AP_SSID and
 * CONFIG_NOVY_AP_PASSWORD. Callers validate with net_ap_ssid_valid()/net_ap_pass_valid(). */
void      nvs_config_get_ap_ssid(char *out, size_t len);
esp_err_t nvs_config_set_ap_ssid(const char *ssid);
void      nvs_config_get_ap_pass(char *out, size_t len);
esp_err_t nvs_config_set_ap_pass(const char *pass);
