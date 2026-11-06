/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
/* Reconnect backoff of firmware 2.3's MQTT loop: 1 s doubling to 60 s, back to 1 s after a connect. */
#pragma once
#include <stdint.h>

typedef struct {
    uint32_t cur_ms, min_ms, max_ms;
} backoff_t;

void     backoff_init(backoff_t *b, uint32_t min_ms, uint32_t max_ms);
uint32_t backoff_next(backoff_t *b);        /* wait before the next try, then double (capped) */
void     backoff_reset(backoff_t *b);       /* after a successful connect */
