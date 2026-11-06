/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include "check.h"
#include "backoff.h"

int main(void)
{
    static const uint32_t want[] = { 1000, 2000, 4000, 8000, 16000, 32000, 60000, 60000, 60000 };
    backoff_t b;
    backoff_init(&b, 1000, 60000);
    for (size_t i = 0; i < sizeof(want) / sizeof(want[0]); i++) CHECK(backoff_next(&b) == want[i]);
    backoff_reset(&b);
    CHECK(backoff_next(&b) == 1000 && backoff_next(&b) == 2000);
    backoff_init(&b, 0, 10);                         /* degenerate input: never 0, never above max */
    CHECK(backoff_next(&b) == 1 && backoff_next(&b) == 2);
    for (int i = 0; i < 40; i++) CHECK(backoff_next(&b) <= 10);
    CHECK_DONE();
}
