/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#pragma once

/* Hooks run in registration order in the restart task, before esp_restart(). */
void restart_add_hook(void (*fn)(void));
/* Safe from any task, including httpd handlers (closes the sockets cleanly after an OTA). */
void restart_schedule(const char *reason);
/* Why the chip last reset (esp_reset_reason()), e.g. "power-on", "software", "brownout". */
const char *reset_reason_str(void);
