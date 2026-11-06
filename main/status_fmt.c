/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include <stdio.h>
#include "status_fmt.h"

#define GREEN  "#30d158"
#define ORANGE "#ff9500"
#define RED    "#ff3b30"

int sf_rssi_percentage(int rssi)
{
    if (rssi >= -50) return 100;
    if (rssi <= -100) return 0;
    return 2 * (rssi + 100);
}

const char *sf_rssi_color(int rssi)
{
    if (rssi >= -50) return GREEN;          /* excellent */
    if (rssi >= -60) return GREEN;          /* good */
    if (rssi >= -70) return ORANGE;         /* fair */
    return RED;
}

int sf_rssi_bars(int rssi)
{
    if (rssi >= -50) return 4;
    if (rssi >= -60) return 3;
    if (rssi >= -70) return 2;
    return 1;
}

int sf_heap_percentage(uint32_t current, uint32_t initial)
{
    if (initial == 0) return 100;
    return (int)((current * 100u) / initial);
}

const char *sf_heap_color(int percentage)
{
    if (percentage > 64) return GREEN;
    if (percentage > 49) return ORANGE;
    return RED;
}

static int fit(int n, size_t len)
{
    return (n > 0 && (size_t)n < len) ? n : -1;
}

int sf_heap_json(char *out, size_t len, uint32_t current, uint32_t initial, uint32_t minimum)
{
    int p = sf_heap_percentage(current, initial);
    return fit(snprintf(out, len, "{\"current\":%lu,\"initial\":%lu,\"minimum\":%lu,\"percentage\":%d,\"color\":\"%s\"}",
                        (unsigned long)current, (unsigned long)initial, (unsigned long)minimum, p, sf_heap_color(p)),
               len);
}

int sf_rssi_json(char *out, size_t len, int rssi)
{
    return fit(snprintf(out, len, "{\"rssi\":%d,\"percentage\":%d,\"color\":\"%s\",\"bars\":%d}", rssi,
                        sf_rssi_percentage(rssi), sf_rssi_color(rssi), sf_rssi_bars(rssi)),
               len);
}

int sf_mqtt_json(char *out, size_t len, bool connected)
{
    return fit(snprintf(out, len, "{\"connected\":%s}", connected ? "true" : "false"), len);
}
