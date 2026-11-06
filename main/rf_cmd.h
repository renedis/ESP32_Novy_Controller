/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
/* The 22 buttons of firmware 2.3 and its RF send sequences (setup(), sendRFCommand(),
 * sendFanRFCommand()), replayed on the rc-switch emulation. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "rcswitch.h"

/* Same values as the sketch's ButtonCommand (= index in its buttons[] table). */
typedef enum {
    RF_CMD_LIGHT = 0,
    RF_CMD_POWER,
    RF_CMD_PLUS,
    RF_CMD_MINUS,
    RF_CMD_NOVY,
    RF_CMD_BRIGHTDIM,
    RF_CMD_BRIGHTDIMLOW,
    RF_CMD_FAN_ALL_OFF,
    RF_CMD_FAN_OFF,
    RF_CMD_FAN_LIGHT,
    RF_CMD_FAN_FORWARD,
    RF_CMD_FAN_REVERSE,
    RF_CMD_FAN_WAVE,
    RF_CMD_FAN_SPEED1,
    RF_CMD_FAN_SPEED2,
    RF_CMD_FAN_SPEED3,
    RF_CMD_FAN_SPEED4,
    RF_CMD_FAN_SPEED5,
    RF_CMD_FAN_SPEED6,
    RF_CMD_FAN_TIMER1H,
    RF_CMD_FAN_TIMER2H,
    RF_CMD_FAN_TIMER4H,
    RF_CMD_COUNT
} rf_cmd_t;

#define RF_FAN_FIRST RF_CMD_FAN_ALL_OFF     /* FAN_CMD_OFFSET */
#define RF_FAN_COUNT 15                     /* FAN_CMD_COUNT */

typedef struct {
    const char *name;     /* buttons[].name, the Home Assistant entity name */
    const char *id;       /* toLowerCase(name): MQTT topic level, discovery object id */
    const char *icon;
    const char *path;     /* web endpoint (POST) */
    const char *reply;    /* web response text */
} rf_cmd_info_t;

const rf_cmd_info_t *rf_cmd_info(int cmd);   /* NULL when out of range */
int  rf_cmd_from_id(const char *id);         /* -1 = unknown (case-sensitive, as the sketch) */
int  rf_cmd_from_path(const char *path);     /* -1 = unknown */
bool rf_cmd_is_fan(int cmd);

extern const int  rf_tx_gpio;                /* RF_TX_GPIO of the RF codes header */
extern const bool rf_codes_example;          /* built with main/rf_codes_example.h (made-up fan codes) */

/* The RF codes in use (.local/rf_codes.h or main/rf_codes_example.h). */
const char     *rf_cmd_novy_channel_code(void);           /* NOVY_DEVICE_CODE[NOVY_CHANNEL - 1] */
int             rf_cmd_novy_channel(void);                /* 1-10 */
uint32_t        rf_cmd_fan_device_id(void);               /* KNOWN_DEVICE_ID */
const uint16_t *rf_cmd_fan_codes(int cmd, int *cycle);    /* cycle array of a fan button, NULL otherwise */

typedef struct {
    rcs_t rcs;                               /* the sketch's global `transmitter` */
    int   fan_cycle[RF_FAN_COUNT];           /* fanCycleIndex[] */
} rf_cmd_state_t;

/* RCSwitch() and the setup() lines: enableTransmit(pin), setPulseLength(350), setProtocol(12). */
void rf_cmd_init(rf_cmd_state_t *s, rcs_out_fn out, void *ctx);

/* sendRFCommand(cmd) for the hood, sendFanRFCommand(cmd) for the fan. log receives the text the
 * sketch passes to addToLogBuffer() (empty for an invalid cmd). */
void rf_cmd_send(rf_cmd_state_t *s, int cmd, char *log, size_t log_len);
