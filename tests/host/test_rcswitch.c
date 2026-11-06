/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include <stdio.h>
#include "check.h"
#include "rcswitch.h"

typedef struct { int level; uint32_t us; } call_t;
static call_t g_calls[4096];
static int    g_n;

static void rec(void *ctx, int level, uint32_t us)
{
    (void)ctx;
    if (g_n < (int)(sizeof(g_calls) / sizeof(g_calls[0]))) g_calls[g_n++] = (call_t){ level, us };
}

static bool call_is(int i, int level, uint32_t us)
{
    return i < g_n && g_calls[i].level == level && g_calls[i].us == us;
}

static void test_table(void)
{
    const rcs_protocol_t *p1 = rcs_protocol_table(1), *p12 = rcs_protocol_table(12);
    CHECK(p1 && p1->pulse_length == 350 && p1->sync.high == 1 && p1->sync.low == 31);
    CHECK(p1->zero.high == 1 && p1->zero.low == 3 && p1->one.high == 3 && p1->one.low == 1 && !p1->inverted);
    CHECK(p12 && p12->pulse_length == 320 && p12->sync.high == 36 && p12->sync.low == 1);
    CHECK(p12->zero.high == 1 && p12->zero.low == 2 && p12->one.high == 2 && p12->one.low == 1 && p12->inverted);
    CHECK(rcs_protocol_table(6)->inverted && rcs_protocol_table(6)->sync.high == 23);
    CHECK(rcs_protocol_table(9)->sync.high == 130 && rcs_protocol_table(9)->sync.low == 7);
    CHECK(rcs_protocol_table(0) == NULL && rcs_protocol_table(13) == NULL);
}

static void test_init_and_setters(void)
{
    rcs_t s;
    rcs_init(&s, rec, NULL);
    CHECK(s.tx_pin == -1 && s.repeat_transmit == 10);
    CHECK(s.protocol.pulse_length == 350 && !s.protocol.inverted && s.protocol.sync.low == 31);

    rcs_set_pulse_length(&s, 350);
    rcs_set_protocol(&s, 12);                    /* the sketch's NOVY order: the 350 is lost */
    CHECK(s.protocol.pulse_length == 320 && s.protocol.inverted);

    rcs_set_protocol(&s, 1);                     /* the sketch's fan order */
    rcs_set_pulse_length(&s, 248);
    CHECK(s.protocol.pulse_length == 248 && !s.protocol.inverted);

    rcs_set_protocol(&s, 12);
    rcs_set_protocol(&s, 0);                     /* out of range -> protocol 1 */
    CHECK(s.protocol.pulse_length == 350 && !s.protocol.inverted);
    rcs_set_protocol(&s, 12);
    rcs_set_protocol(&s, 13);
    CHECK(s.protocol.pulse_length == 350);

    rcs_set_repeat_transmit(&s, 3);
    rcs_set_protocol(&s, 12);                    /* setProtocol() leaves the repeat count alone */
    CHECK(s.repeat_transmit == 3);

    rcs_set_pulse_length(&s, 70000);             /* uint16_t field, as rc-switch */
    CHECK(s.protocol.pulse_length == (uint16_t)70000);
    rcs_enable_transmit(&s, 4);
    CHECK(s.tx_pin == 4);
}

static void test_send_disabled(void)
{
    rcs_t s;
    rcs_init(&s, rec, NULL);
    g_n = 0;
    rcs_send(&s, 0x5, 4);                        /* no enableTransmit(): nothing at all */
    rcs_send_str(&s, "0101");
    CHECK(g_n == 0);
}

static void test_send_plain(void)
{
    rcs_t s;
    rcs_init(&s, rec, NULL);
    rcs_enable_transmit(&s, 4);
    rcs_set_pulse_length(&s, 100);
    rcs_set_repeat_transmit(&s, 1);
    g_n = 0;
    rcs_send(&s, 0x2, 2);                        /* "10", MSB first */
    CHECK(g_n == 7);
    CHECK(call_is(0, 1, 300) && call_is(1, 0, 100));     /* one  = {3,1} */
    CHECK(call_is(2, 1, 100) && call_is(3, 0, 300));     /* zero = {1,3} */
    CHECK(call_is(4, 1, 100) && call_is(5, 0, 3100));    /* sync = {1,31} */
    CHECK(call_is(6, 0, 0));                             /* digitalWrite(LOW) */
}

static void test_send_inverted_and_repeats(void)
{
    rcs_t s;
    rcs_init(&s, rec, NULL);
    rcs_enable_transmit(&s, 4);
    rcs_set_pulse_length(&s, 350);
    rcs_set_protocol(&s, 12);
    rcs_set_repeat_transmit(&s, 2);
    g_n = 0;
    rcs_send(&s, 0x1, 1);
    CHECK(g_n == 2 * 4 + 1);
    CHECK(call_is(0, 0, 640) && call_is(1, 1, 320));     /* one = {2,1}, low first */
    CHECK(call_is(2, 0, 11520) && call_is(3, 1, 320));   /* sync = {36,1} */
    CHECK(call_is(4, 0, 640) && call_is(7, 1, 320));
    CHECK(call_is(8, 0, 0));

    rcs_set_repeat_transmit(&s, 0);              /* no frames, still the final LOW */
    g_n = 0;
    rcs_send(&s, 0x1, 1);
    CHECK(g_n == 1 && call_is(0, 0, 0));
}

static void test_send_str_and_32_bits(void)
{
    rcs_t a;
    call_t first[64];
    int n1;
    rcs_init(&a, rec, NULL);
    rcs_enable_transmit(&a, 4);
    rcs_set_repeat_transmit(&a, 1);
    g_n = 0;
    rcs_send_str(&a, "0101x");                   /* 'x' counts as 1 */
    n1 = g_n;
    memcpy(first, g_calls, sizeof(first));
    g_n = 0;
    rcs_send(&a, 0xB, 5);
    CHECK(n1 == g_n && memcmp(first, g_calls, sizeof(call_t) * (size_t)n1) == 0);

    rcs_set_protocol(&a, 1);
    rcs_set_pulse_length(&a, 248);
    g_n = 0;
    rcs_send(&a, 0xA5A5A5A5u, 32);
    CHECK(g_n == 33 * 2 + 1);
    CHECK(call_is(0, 1, 744) && call_is(1, 0, 248));     /* bit 31 = 1 */
    CHECK(call_is(62, 1, 744) && call_is(63, 0, 248));   /* bit 0 = 1 */
    CHECK(call_is(64, 1, 248) && call_is(65, 0, 7688));  /* sync */

    g_n = 0;
    rcs_send(&a, 0, 0);                          /* length 0: sync only */
    CHECK(g_n == 3 && call_is(0, 1, 248) && call_is(1, 0, 7688));
}

int main(void)
{
    test_table();
    test_init_and_setters();
    test_send_disabled();
    test_send_plain();
    test_send_inverted_and_repeats();
    test_send_str_and_32_bits();
    CHECK_DONE();
}
