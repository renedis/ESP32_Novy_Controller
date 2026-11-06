/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include <string.h>
#include "net_config.h"

bool net_parse_ipv4(const char *s, uint32_t *out)
{
    uint32_t v = 0;
    const char *p = s;
    for (int part = 0; part < 4; part++) {
        if (*p < '0' || *p > '9') return false;
        uint32_t octet = 0;
        int digits = 0;
        while (*p >= '0' && *p <= '9') {
            octet = octet * 10 + (uint32_t)(*p - '0');
            if (++digits > 3 || octet > 255) return false;
            p++;
        }
        v = (v << 8) | octet;
        if (part < 3) {
            if (*p != '.') return false;
            p++;
        }
    }
    if (*p != '\0') return false;
    *out = v;
    return true;
}

bool net_mask_valid(uint32_t mask)
{
    uint32_t inv = ~mask;
    return mask != 0 && (inv & (inv + 1)) == 0;
}

bool net_hostname_valid(const char *s)
{
    size_t n = strlen(s);
    if (n == 0 || n > 32 || s[0] == '-' || s[n - 1] == '-') return false;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-';
        if (!ok) return false;
    }
    return true;
}

bool net_ap_ssid_valid(const char *s)
{
    size_t n = strlen(s);
    if (n == 0 || n > 32 || s[0] == ' ' || s[n - 1] == ' ') return false;
    for (size_t i = 0; i < n; i++)
        if ((unsigned char)s[i] < 0x20 || (unsigned char)s[i] > 0x7E) return false;
    return true;
}

bool net_ap_pass_valid(const char *s)
{
    size_t n = strlen(s);
    if (n < 8 || n > 63) return false;
    for (size_t i = 0; i < n; i++)
        if ((unsigned char)s[i] < 0x20 || (unsigned char)s[i] > 0x7E) return false;
    return true;
}

const char *net_validate(const net_cfg_t *n)
{
    size_t pl = strlen(n->wifi_pass);
    if (n->wifi_ssid[0] == '\0') return "WiFi SSID is empty";
    if (pl < 8 || pl > 64) return "WiFi password must be 8-64 characters";
    if (!net_hostname_valid(n->hostname))
        return "Hostname: 1-32 letters, digits or '-', not starting or ending with '-'";
    if (n->tx_power_qdbm < 8 || n->tx_power_qdbm > 80) return "TX power must be 2-20 dBm";
    if (n->dhcp) return NULL;

    uint32_t ip, mask, gw, dns;
    if (!net_parse_ipv4(n->ip, &ip)) return "IP address is invalid";
    if (!net_parse_ipv4(n->netmask, &mask) || !net_mask_valid(mask)) return "Netmask is invalid";
    if (!net_parse_ipv4(n->gateway, &gw)) return "Gateway is invalid";
    if ((ip & ~mask) == 0 || (ip | mask) == 0xFFFFFFFFu) return "IP is the network or broadcast address";
    if ((gw & mask) != (ip & mask)) return "Gateway is not in the IP subnet";
    if (gw == ip) return "Gateway equals the IP address";
    if (n->dns1[0] && !net_parse_ipv4(n->dns1, &dns)) return "DNS 1 is invalid";
    if (n->dns2[0] && !net_parse_ipv4(n->dns2, &dns)) return "DNS 2 is invalid";
    return NULL;
}
