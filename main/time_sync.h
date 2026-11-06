/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "config_types.h"

void     time_sync_start(const novy_cfg_t *cfg);   /* (re)apply NTP servers and timezone */
bool     time_sync_is_synced(void);
uint32_t time_sync_last_sync(void);                /* epoch seconds, 0 = never */
