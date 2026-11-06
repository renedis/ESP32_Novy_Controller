/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include <string.h>
#include "boot_guard.h"

void bg_init(bg_record_t *r)
{
    memset(r, 0, sizeof(*r));
    r->magic = BG_MAGIC;
}

bool bg_valid(const bg_record_t *r)
{
    return r->magic == BG_MAGIC;
}

bg_slot_t *bg_find(bg_record_t *r, uint32_t addr)
{
    for (int i = 0; i < BG_SLOTS; i++)
        if (addr && r->slot[i].addr == addr) return &r->slot[i];
    return NULL;
}

/* Entry for (addr, sha); a new image in the slot starts a fresh entry. */
static bg_slot_t *entry(bg_record_t *r, uint32_t addr, const uint8_t sha[BG_SHA_LEN])
{
    bg_slot_t *s = bg_find(r, addr);
    if (!s) {
        s = &r->slot[0];
        for (int i = 0; i < BG_SLOTS; i++)
            if (r->slot[i].addr == 0) { s = &r->slot[i]; break; }
    }
    if (s->addr != addr || memcmp(s->sha, sha, BG_SHA_LEN) != 0) {
        memset(s, 0, sizeof(*s));
        s->addr = addr;
        memcpy(s->sha, sha, BG_SHA_LEN);
    }
    return s;
}

/* The other slot holds a valid image that this guard did not leave after a crash loop. */
static bool other_usable(bg_record_t *r, bg_probe_fn probe, void *ctx)
{
    bg_other_t o;
    if (!probe || !probe(ctx, &o) || !o.image_ok) return false;
    bg_slot_t *s = bg_find(r, o.addr);
    return !(s && s->failed && memcmp(s->sha, o.sha, BG_SHA_LEN) == 0);
}

bg_action_t bg_on_boot(bg_record_t *r, uint32_t addr, const uint8_t sha[BG_SHA_LEN], bg_probe_fn probe, void *ctx)
{
    if (!bg_valid(r)) bg_init(r);
    bg_slot_t *s = entry(r, addr, sha);
    if (s->fast_boots < 255) s->fast_boots++;
    if (s->fast_boots >= BG_FAST_BOOTS && other_usable(r, probe, ctx)) {
        s->fast_boots = 0;
        s->failed     = 1;
        return BG_SWITCH;
    }
    return BG_STAY;
}

bg_action_t bg_on_tick(bg_record_t *r, uint32_t addr, uint32_t uptime_s, bool has_ip, bg_probe_fn probe, void *ctx)
{
    bg_slot_t *s = bg_find(r, addr);
    if (!s || s->proven || has_ip || uptime_s < BG_NET_DEADLINE_S) return BG_STAY;
    return other_usable(r, probe, ctx) ? BG_SWITCH : BG_STAY;
}

void bg_on_stable(bg_record_t *r, uint32_t addr)
{
    bg_slot_t *s = bg_find(r, addr);
    if (s) s->fast_boots = 0;
}

void bg_on_proven(bg_record_t *r, uint32_t addr)
{
    bg_slot_t *s = bg_find(r, addr);
    if (s) {
        s->proven = 1;
        s->failed = 0;
    }
}
