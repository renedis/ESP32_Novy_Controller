/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include <stdio.h>
#include "check.h"
#include "evlog.h"

static evlog_t g_l;
static char    g_out[EVLOG_LINES * (EVLOG_LINE_MAX + 1) + 1];

int main(void)
{
    char msg[300];
    evlog_init(&g_l);
    CHECK(evlog_count(&g_l) == 0 && evlog_dump(&g_l, g_out, sizeof(g_out)) == 0 && g_out[0] == '\0');
    evlog_add(&g_l, 5, "Connected to MQTT");
    evlog_add(&g_l, 7, "HA Discovery sent");
    CHECK(evlog_dump(&g_l, g_out, sizeof(g_out)) == strlen(g_out));
    CHECK(strcmp(g_out, "5s: Connected to MQTT\n7s: HA Discovery sent\n") == 0);

    for (int i = 0; i < 60; i++) {                 /* ring of 50: lines 10..59 stay */
        snprintf(msg, sizeof(msg), "line %d", i);
        evlog_add(&g_l, (uint32_t)(100 + i), msg);
    }
    CHECK(evlog_count(&g_l) == 50);
    CHECK(strcmp(evlog_line(&g_l, 0), "110s: line 10") == 0);
    CHECK(strcmp(evlog_line(&g_l, 49), "159s: line 59") == 0);
    CHECK(evlog_line(&g_l, 50) == NULL && evlog_line(&g_l, -1) == NULL);
    evlog_dump(&g_l, g_out, sizeof(g_out));
    CHECK(strncmp(g_out, "110s: line 10\n111s: line 11\n", 28) == 0);

    memset(msg, 'x', sizeof(msg) - 1);             /* cut, never overflow */
    msg[sizeof(msg) - 1] = '\0';
    evlog_add(&g_l, 1, msg);
    CHECK(strlen(evlog_line(&g_l, 49)) == EVLOG_LINE_MAX - 1);

    char small[20];                                /* dump into a small buffer: whole lines only */
    size_t n = evlog_dump(&g_l, small, sizeof(small));
    CHECK(n == strlen(small) && n == 14 && strcmp(small, "111s: line 11\n") == 0);

    evlog_clear(&g_l);
    CHECK(evlog_count(&g_l) == 0 && evlog_dump(&g_l, g_out, sizeof(g_out)) == 0);
    evlog_add(&g_l, 4294967u, "big uptime");
    CHECK(strcmp(evlog_line(&g_l, 0), "4294967s: big uptime") == 0);
    CHECK_DONE();
}
