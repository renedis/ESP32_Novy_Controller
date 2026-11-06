/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "config_types.h"

bool        net_parse_ipv4(const char *s, uint32_t *out);    /* host byte order */
bool        net_mask_valid(uint32_t mask);                   /* contiguous ones, not 0 */
bool        net_hostname_valid(const char *s);               /* 1-32 of [A-Za-z0-9-], no edge '-' */
const char *net_validate(const net_cfg_t *n);                /* NULL = valid, else a reason */
bool        net_ap_pass_valid(const char *s);                /* WPA2 passphrase: 8-63 printable ASCII */
bool        net_ap_ssid_valid(const char *s);                /* 1-32 printable ASCII, no edge spaces */
