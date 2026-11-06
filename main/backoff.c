/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include "backoff.h"

void backoff_init(backoff_t *b, uint32_t min_ms, uint32_t max_ms)
{
    b->min_ms = min_ms ? min_ms : 1;
    b->max_ms = max_ms < b->min_ms ? b->min_ms : max_ms;
    b->cur_ms = b->min_ms;
}

uint32_t backoff_next(backoff_t *b)
{
    uint32_t wait = b->cur_ms;
    b->cur_ms = (b->cur_ms > b->max_ms / 2) ? b->max_ms : b->cur_ms * 2;
    return wait;
}

void backoff_reset(backoff_t *b)
{
    b->cur_ms = b->min_ms;
}
