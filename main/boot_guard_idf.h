/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
/* boot_guard.c on the device: record in NVS ("novyrf"/"bootguard"), other-slot probe, slot switch,
 * esp_ota_mark_app_valid_cancel_rollback() when the image is proven. */
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool     nvs_ok;               /* the guard could read and write NVS */
    uint8_t  fast_boots;
    bool     proven;
    bool     pending_verify;       /* the bootloader started this image as PENDING_VERIFY */
    char     running[17];          /* partition labels */
    char     other[17];
    char     other_project[33];    /* app description of the other slot, "" when none */
    char     other_version[33];
} boot_guard_info_t;

/* First call in app_main: nvs_flash_init() (never erased), count this boot and, after
 * BG_FAST_BOOTS fast boots, boot the other usable slot. Does not return in that case. */
void boot_guard_early(void);

/* Once a second from the main loop: stable at BG_STABLE_S, proven at BG_STABLE_S with an IP,
 * network deadline at BG_NET_DEADLINE_S. */
void boot_guard_tick(uint32_t uptime_s, bool has_ip);

/* The user reached the device over the network (OTA, reboot, network save): mark proven now. */
void boot_guard_confirm(const char *why);

/* Restart hook (register last): a planned restart is not a fast boot. */
void boot_guard_restart_hook(void);

void boot_guard_get_info(boot_guard_info_t *out);
