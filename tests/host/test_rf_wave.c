/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include <stdio.h>
#include "check.h"
#include "rf_wave.h"

static rf_sym_t  g_buf[64];
static rf_wave_t g_w;

static bool half_is(size_t i, int level, uint32_t us)
{
    int l;
    uint32_t d;
    return rf_wave_half(&g_w, i, &l, &d) && l == level && d == us;
}

static void test_layout(void)
{
    rf_sym_t s = { .val = 0 };
    s.d0 = 0x1234;
    s.l0 = 1;
    s.d1 = 0x0567;
    s.l1 = 0;
    CHECK(sizeof(rf_sym_t) == 4);
    CHECK(s.val == (0x1234u | (1u << 15) | (0x0567u << 16)));   /* rmt_symbol_word_t layout */
}

static void test_merge_and_drop(void)
{
    rf_wave_init(&g_w, g_buf, 64);
    CHECK(g_w.level == 0 && rf_wave_symbols(&g_w) == 0);
    rf_wave_level(&g_w, 0);
    rf_wave_delay(&g_w, 320);                    /* low at idle level: kept as a step */
    rf_wave_level(&g_w, 1);
    rf_wave_delay(&g_w, 640);
    rf_wave_level(&g_w, 1);                      /* same level again: no edge */
    rf_wave_delay(&g_w, 10);
    rf_wave_level(&g_w, 0);
    rf_wave_delay(&g_w, 0);                      /* zero length: nothing */
    rf_wave_level(&g_w, 1);
    rf_wave_delay(&g_w, 5);
    rf_wave_level(&g_w, 0);                      /* final digitalWrite(LOW) */
    /* the zero-length low vanished, so the 5 µs high continues the 650 µs high */
    CHECK(g_w.halves == 2);
    CHECK(half_is(0, 0, 320) && half_is(1, 1, 655));
    CHECK(!half_is(2, 1, 5));
    CHECK(g_w.level == 0 && g_w.total_us == 975);
}

static void test_merge_across_dropped_step(void)
{
    rf_wave_init(&g_w, g_buf, 64);
    rf_wave_out(&g_w, 1, 100);
    rf_wave_out(&g_w, 0, 0);                     /* zero-length low between two highs */
    rf_wave_out(&g_w, 1, 50);
    rf_wave_out(&g_w, 0, 200);
    rf_wave_out(&g_w, 0, 0);
    CHECK(g_w.halves == 2 && half_is(0, 1, 150) && half_is(1, 0, 200));
    CHECK(g_w.total_us == 350 && g_w.level == 0);
    CHECK(rf_wave_symbols(&g_w) == 1);
    CHECK(g_buf[0].d0 == 150 && g_buf[0].l0 == 1 && g_buf[0].d1 == 200 && g_buf[0].l1 == 0);
}

static void test_odd_tail(void)
{
    rf_wave_init(&g_w, g_buf, 64);
    rf_wave_out(&g_w, 1, 10);
    rf_wave_out(&g_w, 0, 20);
    rf_wave_out(&g_w, 1, 30);
    rf_wave_out(&g_w, 0, 0);
    CHECK(g_w.halves == 3 && rf_wave_symbols(&g_w) == 2);
    CHECK(g_buf[1].d0 == 30 && g_buf[1].l0 == 1 && g_buf[1].d1 == 0);    /* d1 = 0: end marker */
}

static void test_split_long(void)
{
    int lv[8];
    uint32_t us[8];
    rf_wave_init(&g_w, g_buf, 64);
    rf_wave_out(&g_w, 0, 70000);                 /* 32767 + 32767 + 4466 */
    rf_wave_out(&g_w, 1, 100);
    CHECK(g_w.halves == 4);
    CHECK(half_is(0, 0, 32767) && half_is(1, 0, 32767) && half_is(2, 0, 4466) && half_is(3, 1, 100));
    CHECK(rf_wave_segments(&g_w, lv, us, 8) == 2 && lv[0] == 0 && us[0] == 70000 && lv[1] == 1 && us[1] == 100);

    rf_wave_init(&g_w, g_buf, 64);
    rf_wave_out(&g_w, 1, 32000);
    rf_wave_out(&g_w, 1, 1000);                  /* merge would pass 32767: fill, then a new step */
    CHECK(g_w.halves == 2 && half_is(0, 1, 32767) && half_is(1, 1, 233));
    CHECK(rf_wave_segments(&g_w, lv, us, 8) == 1 && us[0] == 33000);
}

static void test_overflow_and_reset(void)
{
    rf_sym_t small[2];
    rf_wave_t w;
    rf_wave_init(&w, small, 2);
    for (int i = 0; i < 2; i++) {
        rf_wave_out(&w, 1, 10);
        rf_wave_out(&w, 0, 10);
    }
    CHECK(!w.overflow && w.halves == 4 && rf_wave_symbols(&w) == 2);
    rf_wave_out(&w, 1, 10);
    CHECK(w.overflow && w.halves == 4);
    rf_wave_reset(&w);
    CHECK(!w.overflow && w.halves == 0 && w.total_us == 0 && w.level == 0);
}

int main(void)
{
    test_layout();
    test_merge_and_drop();
    test_merge_across_dropped_step();
    test_odd_tail();
    test_split_long();
    test_overflow_and_reset();
    CHECK_DONE();
}
