/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
/* Records the digitalWrite()/delayMicroseconds() steps of an rc-switch transmission straight into
 * RMT symbols. Equal levels merge (a write of the current level is no edge), zero-length steps
 * vanish, a step longer than RF_SYM_MAX_US is split. The line idles low before and after. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RF_SYM_MAX_US 32767u             /* 15-bit RMT duration at 1 µs per tick */

/* Same bit layout as rmt_symbol_word_t: duration0:15, level0:1, duration1:15, level1:1. */
typedef union {
    struct {
        uint16_t d0 : 15;
        uint16_t l0 : 1;
        uint16_t d1 : 15;
        uint16_t l1 : 1;
    };
    uint32_t val;
} rf_sym_t;

typedef struct {
    rf_sym_t *sym;
    size_t    cap;                       /* symbols */
    size_t    halves;                    /* durations written: two per symbol */
    int       level;                     /* current output level */
    bool      overflow;
    uint64_t  total_us;
} rf_wave_t;

void   rf_wave_init(rf_wave_t *w, rf_sym_t *buf, size_t cap);
void   rf_wave_reset(rf_wave_t *w);              /* empty, level low */
void   rf_wave_level(rf_wave_t *w, int level);   /* digitalWrite() */
void   rf_wave_delay(rf_wave_t *w, uint32_t us); /* delayMicroseconds() */
void   rf_wave_out(void *wave, int level, uint32_t us);   /* rcs_out_fn: write, then delay */

size_t rf_wave_symbols(const rf_wave_t *w);      /* to transmit; an odd tail ends with d1 = 0 */
bool   rf_wave_half(const rf_wave_t *w, size_t i, int *level, uint32_t *us);

/* Merged view: steps of the same level joined again (undoes splitting). Returns the count. */
size_t rf_wave_segments(const rf_wave_t *w, int *levels, uint32_t *us, size_t cap);
