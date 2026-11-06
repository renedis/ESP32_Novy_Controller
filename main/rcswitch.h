/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
/* Based on rc-switch 2.6.4 (https://github.com/sui77/rc-switch), Copyright (c) 2011 Suat Özgür
 * and contributors, LGPL-2.1-or-later; used under GPL-3.0 as section 3 of the LGPL-2.1 allows. */
/* Transmit half of rc-switch 2.6.4 (RCSwitch.cpp) with the same state and side effects:
 * setProtocol() copies the whole table entry including the pulse length, setRepeatTransmit()
 * sticks, send() ends with digitalWrite(LOW). Output goes through a callback instead of
 * digitalWrite()/delayMicroseconds(). */
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t high, low;               /* multiples of the pulse length */
} rcs_highlow_t;

typedef struct {
    uint16_t      pulse_length;      /* µs */
    rcs_highlow_t sync, zero, one;
    bool          inverted;          /* pulses start low */
} rcs_protocol_t;

/* digitalWrite(pin, level), then delayMicroseconds(us); us 0 = the write only. */
typedef void (*rcs_out_fn)(void *ctx, int level, uint32_t us);

typedef struct {
    int            tx_pin;           /* nTransmitterPin, -1 = transmit disabled */
    int            repeat_transmit;  /* nRepeatTransmit */
    rcs_protocol_t protocol;
    rcs_out_fn     out;
    void          *ctx;
} rcs_t;

#define RCS_PROTOCOL_COUNT 12

const rcs_protocol_t *rcs_protocol_table(int n);           /* protocol 1..12, NULL otherwise */

void rcs_init(rcs_t *s, rcs_out_fn out, void *ctx);         /* RCSwitch(): repeat 10, protocol 1 */
void rcs_enable_transmit(rcs_t *s, int pin);
void rcs_set_protocol(rcs_t *s, int n);                     /* out of range -> protocol 1 */
void rcs_set_pulse_length(rcs_t *s, int us);
void rcs_set_repeat_transmit(rcs_t *s, int n);
void rcs_send(rcs_t *s, uint32_t code, unsigned length);    /* bits length-1 .. 0, length <= 32 */
void rcs_send_str(rcs_t *s, const char *bits);              /* any character but '0' is a 1 */
