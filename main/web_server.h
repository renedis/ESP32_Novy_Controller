/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#pragma once
#include <stdint.h>
#include "esp_err.h"

/* heap_initial: free heap at boot, the "initial" of GET /heap. Also registers the restart hook. */
esp_err_t web_server_start(uint32_t heap_initial);
