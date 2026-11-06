/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include <stdio.h>
#include "check.h"
#include "net_config.h"

static net_cfg_t valid(void)
{
    net_cfg_t n;
    memset(&n, 0, sizeof(n));
    snprintf(n.wifi_ssid, sizeof(n.wifi_ssid), "%s", "homenet");
    snprintf(n.wifi_pass, sizeof(n.wifi_pass), "%s", "password1");
    snprintf(n.ip, sizeof(n.ip), "%s", "192.168.1.50");
    snprintf(n.netmask, sizeof(n.netmask), "%s", "255.255.255.0");
    snprintf(n.gateway, sizeof(n.gateway), "%s", "192.168.1.1");
    snprintf(n.dns1, sizeof(n.dns1), "%s", "192.168.1.2");
    snprintf(n.hostname, sizeof(n.hostname), "%s", "NOVY-RF");
    n.tx_power_qdbm = 34;
    return n;
}

static void test_parse_ipv4(void)
{
    uint32_t v = 0;
    CHECK(net_parse_ipv4("192.168.1.50", &v) && v == 0xC0A80132u);
    CHECK(net_parse_ipv4("255.255.255.255", &v) && v == 0xFFFFFFFFu);
    CHECK(!net_parse_ipv4("256.1.1.1", &v));
    CHECK(!net_parse_ipv4("1.2.3", &v));
    CHECK(!net_parse_ipv4("1.2.3.4.5", &v));
    CHECK(!net_parse_ipv4("", &v));
    CHECK(!net_parse_ipv4("1..2.3", &v));
    CHECK(!net_parse_ipv4(" 1.2.3.4", &v));
    CHECK(!net_parse_ipv4("1.2.3.4 ", &v));
    CHECK(!net_parse_ipv4("1.2.3.1234", &v));
}

static void test_mask(void)
{
    CHECK(net_mask_valid(0xFFFFFF00u));
    CHECK(net_mask_valid(0xFFFFFE00u));
    CHECK(net_mask_valid(0xFFFFFFFFu));
    CHECK(!net_mask_valid(0));
    CHECK(!net_mask_valid(0xFF00FF00u));
}

static void test_hostname(void)
{
    CHECK(net_hostname_valid("NOVY-RF"));
    CHECK(net_hostname_valid("abcdefghijabcdefghijabcdefghij12"));   /* 32 */
    CHECK(!net_hostname_valid("abcdefghijabcdefghijabcdefghij123")); /* 33 */
    CHECK(!net_hostname_valid(""));
    CHECK(!net_hostname_valid("-heat"));
    CHECK(!net_hostname_valid("heat-"));
    CHECK(!net_hostname_valid("rd_heat"));
}

static void test_validate(void)
{
    net_cfg_t n = valid();
    CHECK(net_validate(&n) == NULL);

    n = valid(); n.wifi_ssid[0] = '\0';                          CHECK(net_validate(&n) != NULL);
    n = valid(); snprintf(n.wifi_pass, sizeof(n.wifi_pass), "%s", "short"); CHECK(net_validate(&n) != NULL);
    n = valid(); snprintf(n.gateway, sizeof(n.gateway), "%s", "192.168.2.1"); CHECK(net_validate(&n) != NULL);
    n = valid(); snprintf(n.ip, sizeof(n.ip), "%s", "192.168.1.255");          CHECK(net_validate(&n) != NULL);
    n = valid(); snprintf(n.ip, sizeof(n.ip), "%s", "192.168.1.0");            CHECK(net_validate(&n) != NULL);
    n = valid(); snprintf(n.ip, sizeof(n.ip), "%s", "192.168.1.1");           CHECK(net_validate(&n) != NULL);
    n = valid(); snprintf(n.netmask, sizeof(n.netmask), "%s", "255.0.255.0"); CHECK(net_validate(&n) != NULL);
    n = valid(); snprintf(n.dns1, sizeof(n.dns1), "%s", "x");               CHECK(net_validate(&n) != NULL);
    n = valid(); n.dns1[0] = '\0';                                CHECK(net_validate(&n) == NULL);
    n = valid(); n.tx_power_qdbm = 4;                             CHECK(net_validate(&n) != NULL);
    n = valid(); n.tx_power_qdbm = 81;                            CHECK(net_validate(&n) != NULL);
    n = valid(); n.hostname[0] = '\0';                            CHECK(net_validate(&n) != NULL);

    n = valid(); n.dhcp = true;                                   /* static fields ignored */
    snprintf(n.ip, sizeof(n.ip), "%s", "garbage");
    CHECK(net_validate(&n) == NULL);
}

static void test_ap_pass(void)
{
    CHECK(net_ap_pass_valid("12345678"));                                 /* 8 */
    CHECK(net_ap_pass_valid("abcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghij123")); /* 63 */
    CHECK(!net_ap_pass_valid("abcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghij1234")); /* 64 */
    CHECK(!net_ap_pass_valid("1234567"));                                 /* 7 */
    CHECK(net_ap_pass_valid("with space ok"));
    CHECK(!net_ap_pass_valid("tab\tchar"));
    CHECK(!net_ap_pass_valid("caf\xc3\xa9-utf8"));                         /* non-ASCII */
}

static void test_ap_ssid(void)
{
    CHECK(net_ap_ssid_valid("NOVY-RF-setup"));
    CHECK(net_ap_ssid_valid("a"));
    CHECK(net_ap_ssid_valid("abcdefghijabcdefghijabcdefghij12"));      /* 32 */
    CHECK(!net_ap_ssid_valid("abcdefghijabcdefghijabcdefghij123"));    /* 33 */
    CHECK(!net_ap_ssid_valid(""));
    CHECK(!net_ap_ssid_valid(" leading"));
    CHECK(!net_ap_ssid_valid("trailing "));
    CHECK(net_ap_ssid_valid("with space"));
    CHECK(!net_ap_ssid_valid("tab\there"));
}

int main(void)
{
    test_ap_ssid();
    test_ap_pass();
    test_parse_ipv4();
    test_mask();
    test_hostname();
    test_validate();
    CHECK_DONE();
}
