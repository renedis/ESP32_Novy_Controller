/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
/* Novy cooker hood protocol constants, as in the Arduino version of this project: the ten channel
 * codes, the prefix and the command codes. A code word is
 * NOVY_DEVICE_CODE[NOVY_CHANNEL - 1] + NOVY_PREFIX + command, sent with rc-switch protocol 12.
 * Included by rf_cmd.c only. */
#pragma once

static const char *const NOVY_DEVICE_CODE[] = {
    "0101", "1001", "0001", "1110", "0110",
    "1010", "0010", "1100", "0100", "1000",
};
#define NOVY_PREFIX         "0101"
#define NOVY_COMMAND_LIGHT  "0111010001"
#define NOVY_COMMAND_POWER  "0111010011"
#define NOVY_COMMAND_PLUS   "0101"
#define NOVY_COMMAND_MINUS  "0110"
#define NOVY_COMMAND_NOVY   "0100"
