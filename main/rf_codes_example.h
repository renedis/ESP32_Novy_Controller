/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
/* EXAMPLE RF settings. The fan values are made up and work with no real fan.
 *
 * Put your own values in .local/rf_codes.h (git-ignored; the build uses it when it exists, after
 * idf.py reconfigure), in exactly this format:
 * - RF_TX_GPIO: the GPIO wired to the 433 MHz transmitter's DATA pin;
 * - NOVY_CHANNEL: the channel (1-10) your Novy hood listens on;
 * - KNOWN_DEVICE_ID and the *_CODES arrays: your ceiling fan remote's 20-bit id and, per button,
 *   the rolling 12-bit counters it cycles through (capture them with a 433 MHz receiver and
 *   rc-switch, protocol 1, 32 bits: id << 12 | counter).
 * tools/make_rf_codes.py writes that file from the config.h of the Arduino version.
 * Included by rf_cmd.c only. */
#pragma once
#include <stdint.h>

#define RF_TX_GPIO   4                              /* STX882 DATA */
#define NOVY_CHANNEL 4                              /* Novy hood channel, 1-10 */

#define KNOWN_DEVICE_ID 0x12345ul                   /* ceiling fan remote id, 20 bits */

static const uint16_t ALL_OFF_CODES[]      = {  1,   2,   3,   4,   5,   6,   7,   8};
static const uint16_t FAN_OFF_CODES[]      = {  9,  10,  11,  12,  13,  14,  15,  16};
static const uint16_t LIGHT_TOGGLE_CODES[] = { 17,  18,  19,  20,  21,  22,  23,  24};
static const uint16_t FAN_FORWARD_CODES[]  = { 25,  26,  27,  28,  29,  30,  31,  32};
static const uint16_t FAN_REVERSE_CODES[]  = { 33,  34,  35,  36,  37,  38,  39,  40};
static const uint16_t FAN_WAVE_CODES[]     = { 41,  42,  43,  44,  45,  46,  47,  48};
static const uint16_t SPEED_1_CODES[]      = { 49,  50,  51,  52,  53,  54,  55,  56};
static const uint16_t SPEED_2_CODES[]      = { 57,  58,  59,  60,  61,  62,  63,  64};
static const uint16_t SPEED_3_CODES[]      = { 65,  66,  67,  68,  69,  70,  71,  72};
static const uint16_t SPEED_4_CODES[]      = { 73,  74,  75,  76,  77,  78,  79,  80};
static const uint16_t SPEED_5_CODES[]      = { 81,  82,  83,  84,  85,  86,  87,  88};
static const uint16_t SPEED_6_CODES[]      = { 89,  90,  91,  92,  93,  94,  95,  96};
static const uint16_t TIMER_1H_CODES[]     = { 97,  98,  99, 100, 101, 102, 103, 104};
static const uint16_t TIMER_2H_CODES[]     = {105, 106, 107, 108, 109, 110, 111, 112};
static const uint16_t TIMER_4H_CODES[]     = {113, 114, 115, 116, 117, 118, 119, 120};

#define ALL_OFF_CYCLE_SIZE      8
#define FAN_OFF_CYCLE_SIZE      8
#define LIGHT_TOGGLE_CYCLE_SIZE 8
#define FAN_FORWARD_CYCLE_SIZE  8
#define FAN_REVERSE_CYCLE_SIZE  8
#define FAN_WAVE_CYCLE_SIZE     8
#define SPEED_1_CYCLE_SIZE      8
#define SPEED_2_CYCLE_SIZE      8
#define SPEED_3_CYCLE_SIZE      8
#define SPEED_4_CYCLE_SIZE      8
#define SPEED_5_CYCLE_SIZE      8
#define SPEED_6_CYCLE_SIZE      8
#define TIMER_1H_CYCLE_SIZE     8
#define TIMER_2H_CYCLE_SIZE     8
#define TIMER_4H_CYCLE_SIZE     8
