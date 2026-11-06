/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
/* Based on rc-switch 2.6.4 (https://github.com/sui77/rc-switch), Copyright (c) 2011 Suat Özgür
 * and contributors, LGPL-2.1-or-later; used under GPL-3.0 as section 3 of the LGPL-2.1 allows. */
#include <stddef.h>
#include "rcswitch.h"

/* rc-switch 2.6.4 proto[]: {pulselength, sync, zero, one, invertedSignal} */
static const rcs_protocol_t s_proto[RCS_PROTOCOL_COUNT] = {
    { 350, {   1, 31 }, {  1,  3 }, {  3,  1 }, false },   /* 1 */
    { 650, {   1, 10 }, {  1,  2 }, {  2,  1 }, false },   /* 2 */
    { 100, {  30, 71 }, {  4, 11 }, {  9,  6 }, false },   /* 3 */
    { 380, {   1,  6 }, {  1,  3 }, {  3,  1 }, false },   /* 4 */
    { 500, {   6, 14 }, {  1,  2 }, {  2,  1 }, false },   /* 5 */
    { 450, {  23,  1 }, {  1,  2 }, {  2,  1 }, true  },   /* 6 HT6P20B */
    { 150, {   2, 62 }, {  1,  6 }, {  6,  1 }, false },   /* 7 HS2303-PT */
    { 200, {   3, 130}, {  7, 16 }, {  3, 16 }, false },   /* 8 Conrad RS-200 RX */
    { 200, { 130,  7 }, { 16,  7 }, { 16,  3 }, true  },   /* 9 Conrad RS-200 TX */
    { 365, {  18,  1 }, {  3,  1 }, {  1,  3 }, true  },   /* 10 1ByOne doorbell */
    { 270, {  36,  1 }, {  1,  2 }, {  2,  1 }, true  },   /* 11 HT12E */
    { 320, {  36,  1 }, {  1,  2 }, {  2,  1 }, true  },   /* 12 SM5212 */
};

const rcs_protocol_t *rcs_protocol_table(int n)
{
    return (n >= 1 && n <= RCS_PROTOCOL_COUNT) ? &s_proto[n - 1] : NULL;
}

void rcs_init(rcs_t *s, rcs_out_fn out, void *ctx)
{
    s->tx_pin = -1;
    s->out    = out;
    s->ctx    = ctx;
    rcs_set_repeat_transmit(s, 10);
    rcs_set_protocol(s, 1);
}

void rcs_enable_transmit(rcs_t *s, int pin)
{
    s->tx_pin = pin;             /* pinMode(OUTPUT) leaves the output latch (low) as it is */
}

void rcs_set_protocol(rcs_t *s, int n)
{
    if (n < 1 || n > RCS_PROTOCOL_COUNT) n = 1;
    s->protocol = s_proto[n - 1];
}

void rcs_set_pulse_length(rcs_t *s, int us)
{
    s->protocol.pulse_length = (uint16_t)us;
}

void rcs_set_repeat_transmit(rcs_t *s, int n)
{
    s->repeat_transmit = n;
}

static void transmit(rcs_t *s, rcs_highlow_t p)
{
    int first = s->protocol.inverted ? 0 : 1;
    s->out(s->ctx, first, (uint32_t)s->protocol.pulse_length * p.high);
    s->out(s->ctx, !first, (uint32_t)s->protocol.pulse_length * p.low);
}

void rcs_send(rcs_t *s, uint32_t code, unsigned length)
{
    if (s->tx_pin == -1) return;
    if (length > 32) length = 32;
    for (int r = 0; r < s->repeat_transmit; r++) {
        for (int i = (int)length - 1; i >= 0; i--)
            transmit(s, ((code >> i) & 1u) ? s->protocol.one : s->protocol.zero);
        transmit(s, s->protocol.sync);
    }
    s->out(s->ctx, 0, 0);       /* "Disable transmit after sending (i.e., for inverted protocols)" */
}

void rcs_send_str(rcs_t *s, const char *bits)
{
    uint32_t code = 0;
    unsigned length = 0;
    for (const char *p = bits; *p; p++) {
        code <<= 1;
        if (*p != '0') code |= 1u;
        length++;
    }
    rcs_send(s, code, length);
}
