/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
/* App-level rollback guard that works with any bootloader. Pure decisions on a record that
 * boot_guard_idf.c keeps in NVS; one entry per OTA slot, tied to the image in it (ELF SHA-256).
 *
 *  - Every boot of an image counts as "fast" until BG_STABLE_S of uptime (or a planned restart).
 *  - BG_FAST_BOOTS fast boots in a row: boot the other slot, if it is usable, and mark this image
 *    failed so the other image never switches back to it.
 *  - An image that never ran BG_STABLE_S with an IP ("proven") and has no IP at
 *    BG_NET_DEADLINE_S switches to the other usable slot (not marked failed).
 *  - A usable other slot: valid image (checked by the caller's probe) and not marked failed here. */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define BG_MAGIC          0x4E425447u     /* "GTBN" */
#define BG_FAST_BOOTS     3
#define BG_STABLE_S       60
#define BG_NET_DEADLINE_S 300
#define BG_SHA_LEN        8
#define BG_SLOTS          2

typedef struct {
    uint32_t addr;                         /* partition address, 0 = unused entry */
    uint8_t  sha[BG_SHA_LEN];              /* first bytes of the image's ELF SHA-256 */
    uint8_t  fast_boots;
    uint8_t  proven;
    uint8_t  failed;
    uint8_t  reserved;
} bg_slot_t;

typedef struct {
    uint32_t  magic;
    bg_slot_t slot[BG_SLOTS];
} bg_record_t;

typedef struct {
    bool     image_ok;                     /* description, otadata state and esp_image_verify() fine */
    uint32_t addr;
    uint8_t  sha[BG_SHA_LEN];
} bg_other_t;

/* Fills *out for the other OTA slot; false when there is none. Slow (verifies the image), so the
 * guard calls it only when it is about to switch. */
typedef bool (*bg_probe_fn)(void *ctx, bg_other_t *out);

typedef enum { BG_STAY = 0, BG_SWITCH } bg_action_t;

void        bg_init(bg_record_t *r);       /* empty record */
bool        bg_valid(const bg_record_t *r);
bg_slot_t  *bg_find(bg_record_t *r, uint32_t addr);

/* Boot of the image (addr, sha). Updates *r; BG_SWITCH = set the other slot and restart. */
bg_action_t bg_on_boot(bg_record_t *r, uint32_t addr, const uint8_t sha[BG_SHA_LEN], bg_probe_fn probe, void *ctx);
/* Once a second: BG_SWITCH when the network deadline of a never-proven image passes. */
bg_action_t bg_on_tick(bg_record_t *r, uint32_t addr, uint32_t uptime_s, bool has_ip, bg_probe_fn probe, void *ctx);
void        bg_on_stable(bg_record_t *r, uint32_t addr);     /* BG_STABLE_S reached or planned restart */
void        bg_on_proven(bg_record_t *r, uint32_t addr);     /* BG_STABLE_S with an IP */
