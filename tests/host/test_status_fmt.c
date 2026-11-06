/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include <stdio.h>
#include "check.h"
#include "status_fmt.h"

/* Body of a "GET <path> <code> <type> <body>" line in <fixtures>/http_live.txt */
static const char *live_body(const char *path, char *buf, size_t len)
{
    char line[512], prefix[64];
    FILE *f = fopen(FIXTURES "/http_live.txt", "r");
    if (!f) return NULL;
    snprintf(prefix, sizeof(prefix), "GET %s 200 application/json ", path);
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, prefix, strlen(prefix)) == 0) {
            snprintf(buf, len, "%s", line + strlen(prefix));
            fclose(f);
            return buf;
        }
    }
    fclose(f);
    return NULL;
}

static void test_helpers(void)
{
    CHECK(sf_rssi_percentage(-39) == 100 && sf_rssi_percentage(-50) == 100 && sf_rssi_percentage(-51) == 98);
    CHECK(sf_rssi_percentage(-80) == 40 && sf_rssi_percentage(-100) == 0 && sf_rssi_percentage(-120) == 0);
    CHECK(sf_rssi_percentage(0) == 100);                               /* not connected: 0 dBm, as 2.3 */
    CHECK(strcmp(sf_rssi_color(-60), "#30d158") == 0 && strcmp(sf_rssi_color(-61), "#ff9500") == 0);
    CHECK(strcmp(sf_rssi_color(-70), "#ff9500") == 0 && strcmp(sf_rssi_color(-71), "#ff3b30") == 0);
    CHECK(sf_rssi_bars(-50) == 4 && sf_rssi_bars(-51) == 3 && sf_rssi_bars(-60) == 3 && sf_rssi_bars(-61) == 2);
    CHECK(sf_rssi_bars(-70) == 2 && sf_rssi_bars(-71) == 1);
    CHECK(sf_heap_percentage(170968, 246016) == 69 && sf_heap_percentage(5, 0) == 100);
    CHECK(strcmp(sf_heap_color(65), "#30d158") == 0 && strcmp(sf_heap_color(64), "#ff9500") == 0);
    CHECK(strcmp(sf_heap_color(50), "#ff9500") == 0 && strcmp(sf_heap_color(49), "#ff3b30") == 0);
}

static void test_json_equals_live_capture(void)
{
    char got[160], want[160];
    CHECK(sf_heap_json(got, sizeof(got), 170968, 246016, 148928) > 0);
    CHECK(live_body("/heap", want, sizeof(want)) && strcmp(got, want) == 0);
    CHECK(sf_rssi_json(got, sizeof(got), -39) > 0);
    CHECK(live_body("/rssi", want, sizeof(want)) && strcmp(got, want) == 0);
    CHECK(sf_mqtt_json(got, sizeof(got), true) > 0);
    CHECK(live_body("/mqtt-status", want, sizeof(want)) && strcmp(got, want) == 0);
    sf_mqtt_json(got, sizeof(got), false);
    CHECK(strcmp(got, "{\"connected\":false}") == 0);
    CHECK(sf_rssi_json(got, 8, -39) < 0);
}

int main(void)
{
    test_helpers();
    test_json_equals_live_capture();
    CHECK_DONE();
}
