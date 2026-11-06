/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include <stdio.h>
#include "check.h"
#include "boot_guard.h"

#define SLOT_A 0x10000u
#define SLOT_B 0x150000u

static const uint8_t SHA_X[BG_SHA_LEN] = { 1, 2, 3, 4, 5, 6, 7, 8 };
static const uint8_t SHA_Y[BG_SHA_LEN] = { 9, 9, 9, 9, 9, 9, 9, 9 };
static const uint8_t SHA_ARD[BG_SHA_LEN] = { 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0, 1 };

typedef struct {
    int        calls;
    bool       exists;
    bg_other_t o;
} probe_t;

static bool probe(void *ctx, bg_other_t *out)
{
    probe_t *p = ctx;
    p->calls++;
    *out = p->o;
    return p->exists;
}

static probe_t other(uint32_t addr, const uint8_t *sha, bool ok)
{
    probe_t p = { .calls = 0, .exists = true, .o = { .image_ok = ok, .addr = addr } };
    memcpy(p.o.sha, sha, BG_SHA_LEN);
    return p;
}

static void test_record(void)
{
    bg_record_t r;
    bg_init(&r);
    CHECK(bg_valid(&r) && bg_find(&r, SLOT_A) == NULL);
    r.magic = 0;
    CHECK(!bg_valid(&r));
}

static void test_crash_loop_switches_to_good_other_slot(void)
{
    bg_record_t r;
    probe_t     p = other(SLOT_A, SHA_ARD, true);           /* Arduino image in app0 */
    bg_init(&r);
    CHECK(bg_on_boot(&r, SLOT_B, SHA_X, probe, &p) == BG_STAY);
    CHECK(bg_find(&r, SLOT_B)->fast_boots == 1 && p.calls == 0);    /* no probe below the limit */
    CHECK(bg_on_boot(&r, SLOT_B, SHA_X, probe, &p) == BG_STAY);
    CHECK(bg_on_boot(&r, SLOT_B, SHA_X, probe, &p) == BG_SWITCH);    /* third fast boot */
    CHECK(p.calls == 1);
    bg_slot_t *s = bg_find(&r, SLOT_B);
    CHECK(s->fast_boots == 0 && s->failed == 1 && s->proven == 0);
}

static void test_no_usable_other_slot(void)
{
    bg_record_t r;
    probe_t     bad = other(SLOT_A, SHA_ARD, false), none = other(SLOT_A, SHA_ARD, true);
    none.exists = false;
    bg_init(&r);
    for (int i = 0; i < 2; i++) bg_on_boot(&r, SLOT_B, SHA_X, probe, &bad);
    CHECK(bg_on_boot(&r, SLOT_B, SHA_X, probe, &bad) == BG_STAY && bad.calls == 1);
    CHECK(bg_on_boot(&r, SLOT_B, SHA_X, probe, &none) == BG_STAY && none.calls == 1);
    CHECK(bg_find(&r, SLOT_B)->fast_boots == 4 && bg_find(&r, SLOT_B)->failed == 0);
    for (int i = 0; i < 300; i++) bg_on_boot(&r, SLOT_B, SHA_X, probe, &bad);
    CHECK(bg_find(&r, SLOT_B)->fast_boots == 255);           /* capped */
}

static void test_stable_and_planned_restart_clear(void)
{
    bg_record_t r;
    probe_t     p = other(SLOT_A, SHA_ARD, true);
    bg_init(&r);
    bg_on_boot(&r, SLOT_B, SHA_X, probe, &p);
    bg_on_boot(&r, SLOT_B, SHA_X, probe, &p);
    bg_on_stable(&r, SLOT_B);                                /* 60 s or reboot button */
    CHECK(bg_find(&r, SLOT_B)->fast_boots == 0);
    CHECK(bg_on_boot(&r, SLOT_B, SHA_X, probe, &p) == BG_STAY);
    CHECK(bg_on_boot(&r, SLOT_B, SHA_X, probe, &p) == BG_STAY);
    CHECK(p.calls == 0);
    bg_on_stable(&r, 0x999);                                 /* unknown slot: no effect */
    CHECK(bg_find(&r, SLOT_B)->fast_boots == 2);
}

static void test_new_image_starts_fresh(void)
{
    bg_record_t r;
    probe_t     p = other(SLOT_A, SHA_ARD, true);
    bg_init(&r);
    bg_on_boot(&r, SLOT_B, SHA_X, probe, &p);
    bg_on_boot(&r, SLOT_B, SHA_X, probe, &p);
    bg_on_proven(&r, SLOT_B);
    CHECK(bg_on_boot(&r, SLOT_B, SHA_Y, probe, &p) == BG_STAY);     /* other image, same slot */
    bg_slot_t *s = bg_find(&r, SLOT_B);
    CHECK(s->fast_boots == 1 && s->proven == 0 && memcmp(s->sha, SHA_Y, BG_SHA_LEN) == 0);

    bg_record_t junk;                                        /* garbage in NVS: starts fresh */
    memset(&junk, 0x5A, sizeof(junk));
    CHECK(bg_on_boot(&junk, SLOT_B, SHA_X, probe, &p) == BG_STAY);
    CHECK(bg_valid(&junk) && bg_find(&junk, SLOT_B)->fast_boots == 1 && bg_find(&junk, SLOT_A) == NULL);
}

static void test_never_back_to_a_failed_image(void)
{
    bg_record_t r;
    probe_t     to_x = other(SLOT_B, SHA_X, true), to_y = other(SLOT_A, SHA_Y, true);
    bg_init(&r);
    /* X in app1 proven, Y installed into app0 crash-loops: back to X, Y failed */
    bg_on_boot(&r, SLOT_B, SHA_X, probe, &to_y);
    bg_on_proven(&r, SLOT_B);
    bg_on_stable(&r, SLOT_B);
    for (int i = 0; i < 2; i++) CHECK(bg_on_boot(&r, SLOT_A, SHA_Y, probe, &to_x) == BG_STAY);
    CHECK(bg_on_boot(&r, SLOT_A, SHA_Y, probe, &to_x) == BG_SWITCH);
    CHECK(bg_find(&r, SLOT_A)->failed == 1);
    /* X now crash-loops too (bad luck): Y is known to fail, so stay */
    for (int i = 0; i < 2; i++) bg_on_boot(&r, SLOT_B, SHA_X, probe, &to_y);
    CHECK(bg_on_boot(&r, SLOT_B, SHA_X, probe, &to_y) == BG_STAY && to_y.calls == 1);
    /* a fresh Y' in app0 is not the failed image */
    probe_t to_y2 = other(SLOT_A, SHA_ARD, true);
    CHECK(bg_on_boot(&r, SLOT_B, SHA_X, probe, &to_y2) == BG_SWITCH);
}

static void test_network_deadline(void)
{
    bg_record_t r;
    probe_t     p = other(SLOT_A, SHA_ARD, true);
    bg_init(&r);
    bg_on_boot(&r, SLOT_B, SHA_X, probe, &p);
    bg_on_stable(&r, SLOT_B);
    CHECK(bg_on_tick(&r, SLOT_B, BG_NET_DEADLINE_S - 1, false, probe, &p) == BG_STAY);
    CHECK(bg_on_tick(&r, SLOT_B, BG_NET_DEADLINE_S, true, probe, &p) == BG_STAY);   /* has an IP now */
    CHECK(p.calls == 0);
    CHECK(bg_on_tick(&r, SLOT_B, BG_NET_DEADLINE_S, false, probe, &p) == BG_SWITCH);
    CHECK(bg_find(&r, SLOT_B)->failed == 0);                 /* no network is not a crash */

    bg_on_proven(&r, SLOT_B);                                /* proven images never time out */
    CHECK(bg_on_tick(&r, SLOT_B, 100000, false, probe, &p) == BG_STAY);

    bg_record_t r2;
    probe_t     bad = other(SLOT_A, SHA_ARD, false);
    bg_init(&r2);
    bg_on_boot(&r2, SLOT_B, SHA_X, probe, &bad);
    CHECK(bg_on_tick(&r2, SLOT_B, 400, false, probe, &bad) == BG_STAY);   /* nothing better to boot */
    CHECK(bg_on_tick(&r2, 0x777, 400, false, probe, &bad) == BG_STAY);    /* unknown slot */
}

int main(void)
{
    test_record();
    test_crash_loop_switches_to_good_other_slot();
    test_no_usable_other_slot();
    test_stable_and_planned_restart_clear();
    test_new_image_starts_fresh();
    test_never_back_to_a_failed_image();
    test_network_deadline();
    CHECK_DONE();
}
