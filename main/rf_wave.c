/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include <string.h>
#include "rf_wave.h"

void rf_wave_init(rf_wave_t *w, rf_sym_t *buf, size_t cap)
{
    w->sym = buf;
    w->cap = cap;
    rf_wave_reset(w);
}

void rf_wave_reset(rf_wave_t *w)
{
    w->halves   = 0;
    w->level    = 0;
    w->overflow = false;
    w->total_us = 0;
}

static void set_half(rf_wave_t *w, size_t i, int level, uint32_t us)
{
    rf_sym_t *s = &w->sym[i / 2];
    if (i % 2 == 0) {
        s->val = 0;              /* d1 = 0 until the next half arrives: end marker on an odd tail */
        s->d0  = (uint16_t)us;
        s->l0  = (uint16_t)(level ? 1 : 0);
    } else {
        s->d1 = (uint16_t)us;
        s->l1 = (uint16_t)(level ? 1 : 0);
    }
}

bool rf_wave_half(const rf_wave_t *w, size_t i, int *level, uint32_t *us)
{
    if (i >= w->halves) return false;
    const rf_sym_t *s = &w->sym[i / 2];
    *level = (i % 2 == 0) ? s->l0 : s->l1;
    *us    = (i % 2 == 0) ? s->d0 : s->d1;
    return true;
}

static bool append(rf_wave_t *w, int level, uint32_t us)
{
    if (w->halves >= w->cap * 2) {
        w->overflow = true;
        return false;
    }
    set_half(w, w->halves++, level, us);
    return true;
}

void rf_wave_level(rf_wave_t *w, int level)
{
    w->level = level ? 1 : 0;
}

void rf_wave_delay(rf_wave_t *w, uint32_t us)
{
    if (us == 0 || w->overflow) return;
    w->total_us += us;
    int      last_level;
    uint32_t last_us;
    if (rf_wave_half(w, w->halves - 1, &last_level, &last_us) && last_level == w->level &&
        last_us < RF_SYM_MAX_US) {
        uint32_t add = RF_SYM_MAX_US - last_us;
        if (add > us) add = us;
        set_half(w, w->halves - 1, w->level, last_us + add);
        us -= add;
    }
    while (us > 0) {
        uint32_t chunk = us > RF_SYM_MAX_US ? RF_SYM_MAX_US : us;
        if (!append(w, w->level, chunk)) return;
        us -= chunk;
    }
}

void rf_wave_out(void *wave, int level, uint32_t us)
{
    rf_wave_t *w = (rf_wave_t *)wave;
    rf_wave_level(w, level);
    rf_wave_delay(w, us);
}

size_t rf_wave_symbols(const rf_wave_t *w)
{
    return (w->halves + 1) / 2;
}

size_t rf_wave_segments(const rf_wave_t *w, int *levels, uint32_t *us, size_t cap)
{
    size_t n = 0;
    for (size_t i = 0; i < w->halves; i++) {
        int      l;
        uint32_t d;
        rf_wave_half(w, i, &l, &d);
        if (n > 0 && levels[n - 1] == l) {
            us[n - 1] += d;
        } else if (n < cap) {
            levels[n] = l;
            us[n++]   = d;
        } else {
            break;
        }
    }
    return n;
}
