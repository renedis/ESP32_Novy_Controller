/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
/* RF jobs from web and MQTT, served in order by one worker: rf_cmd/rcswitch build the 2.3 waveform,
 * one RMT TX channel (1 µs ticks) sends it, devstate tracks the press. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "devstate.h"
#include "rf_cmd.h"

#define RF_QUEUE_LEN 16

typedef enum { RF_SRC_WEB = 0, RF_SRC_MQTT } rf_src_t;

/* Called by the worker after every transmission (MQTT publishes the press). */
typedef void (*rf_press_fn)(int cmd, const devstate_t *state);

typedef struct {
    bool        ready;                  /* RMT channel up */
    int         gpio;
    uint32_t    sent, dropped, failed;
    int         queued;
    int         repeat_transmit;        /* NOVY frames per send(): 10 until the first fan command */
    int         fan_cycle[RF_FAN_COUNT];
    devstate_t  state;
    int         last_cmd;               /* -1 = none yet */
    uint32_t    last_uptime_s;
} rf_tx_status_t;

esp_err_t rf_tx_start(void);
void      rf_tx_set_press_handler(rf_press_fn fn);
bool      rf_tx_submit(int cmd, rf_src_t src);      /* false: invalid, not ready or queue full */
void      rf_tx_get_status(rf_tx_status_t *out);
void      rf_tx_restart_hook(void);                 /* finish the running job, take no new ones */
