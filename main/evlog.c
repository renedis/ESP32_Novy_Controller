/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include <stdio.h>
#include <string.h>
#include "evlog.h"

void evlog_init(evlog_t *l)
{
    evlog_clear(l);
}

void evlog_clear(evlog_t *l)
{
    memset(l->line, 0, sizeof(l->line));
    l->next  = 0;
    l->count = 0;
}

void evlog_add(evlog_t *l, uint32_t uptime_s, const char *msg)
{
    snprintf(l->line[l->next], EVLOG_LINE_MAX, "%lus: %s", (unsigned long)uptime_s, msg);
    l->next = (l->next + 1) % EVLOG_LINES;
    if (l->count < EVLOG_LINES) l->count++;
}

int evlog_count(const evlog_t *l)
{
    return l->count;
}

const char *evlog_line(const evlog_t *l, int i)
{
    if (i < 0 || i >= l->count) return NULL;
    return l->line[(l->next - l->count + i + EVLOG_LINES) % EVLOG_LINES];
}

size_t evlog_dump(const evlog_t *l, char *out, size_t cap)
{
    size_t n = 0;
    if (cap) out[0] = '\0';
    for (int i = 0; i < l->count; i++) {
        const char *s = evlog_line(l, i);
        size_t      k = strlen(s);
        if (n + k + 2 > cap) break;
        memcpy(out + n, s, k);
        out[n + k]     = '\n';
        out[n + k + 1] = '\0';
        n += k + 1;
    }
    return n;
}
