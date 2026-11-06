/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
/* Event log of firmware 2.3 (addToLogBuffer()/getLogContent()): the last 50 lines, each
 * "<uptime s>s: <message>", served oldest first with "\n" after every line by GET /log. */
#pragma once
#include <stddef.h>
#include <stdint.h>

#define EVLOG_LINES    50
#define EVLOG_LINE_MAX 160               /* longer messages are cut */

typedef struct {
    char line[EVLOG_LINES][EVLOG_LINE_MAX];
    int  next;                           /* logIndex */
    int  count;
} evlog_t;

void        evlog_init(evlog_t *l);
void        evlog_add(evlog_t *l, uint32_t uptime_s, const char *msg);
void        evlog_clear(evlog_t *l);
int         evlog_count(const evlog_t *l);
const char *evlog_line(const evlog_t *l, int i);         /* 0 = oldest; NULL past the end */
size_t      evlog_dump(const evlog_t *l, char *out, size_t cap);   /* getLogContent(); returns the length */
