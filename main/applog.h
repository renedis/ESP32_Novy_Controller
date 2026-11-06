/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
/* Event log of firmware 2.3 (addToLogBuffer()): every line goes to the ESP-IDF log and to the
 * 50-line ring served by GET /log. Thread-safe. */
#pragma once
#include <stddef.h>

void  applog_init(void);
void  applog(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void  applog_clear(void);
char *applog_dump(size_t *len);          /* malloc'd getLogContent() text, NULL on OOM */
