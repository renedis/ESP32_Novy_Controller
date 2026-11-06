/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include <stdio.h>
#include <string.h>
#include "rf_cmd.h"
#include "rf_novy.h"
#ifdef RF_CODES_H
#include RF_CODES_H                     /* .local/rf_codes.h: your channel and fan remote */
const bool rf_codes_example = false;
#else
#include "rf_codes_example.h"
const bool rf_codes_example = true;
#endif

_Static_assert(NOVY_CHANNEL >= 1 && NOVY_CHANNEL <= 10, "NOVY_CHANNEL must be 1-10");

const int rf_tx_gpio = RF_TX_GPIO;

static const rf_cmd_info_t s_info[RF_CMD_COUNT] = {
    { "Light",          "light",          "mdi:lightbulb",        "/toggleLight",         "Light toggled" },
    { "Power",          "power",          "mdi:power",            "/togglePower",         "Power toggled" },
    { "Plus",           "plus",           "mdi:plus",             "/togglePlus",          "Plus toggled" },
    { "Minus",          "minus",          "mdi:minus",            "/toggleMinus",         "Minus toggled" },
    { "Novy",           "novy",           "mdi:fan",              "/toggleNovy",          "Novy toggled" },
    { "Brightness",     "brightness",     "mdi:brightness-6",     "/toggleBrightness",    "Brightness toggled" },
    { "Brightness-low", "brightness-low", "mdi:brightness-5",     "/toggleBrightnesslow", "Brightness low toggled" },
    { "Fan-All-Off",    "fan-all-off",    "mdi:power-off",        "/fan_all_off",         "Fan All Off" },
    { "Fan-Off",        "fan-off",        "mdi:fan-off",          "/fan_off",             "Fan Off" },
    { "Fan-Light",      "fan-light",      "mdi:lightbulb",        "/fan_light",           "Fan Light" },
    { "Fan-Forward",    "fan-forward",    "mdi:rotate-right",     "/fan_forward",         "Fan Forward" },
    { "Fan-Reverse",    "fan-reverse",    "mdi:rotate-left",      "/fan_reverse",         "Fan Reverse" },
    { "Fan-Wave",       "fan-wave",       "mdi:wave",             "/fan_wave",            "Fan Wave" },
    { "Fan-Speed1",     "fan-speed1",     "mdi:numeric-1-circle", "/fan_speed1",          "Fan Speed 1" },
    { "Fan-Speed2",     "fan-speed2",     "mdi:numeric-2-circle", "/fan_speed2",          "Fan Speed 2" },
    { "Fan-Speed3",     "fan-speed3",     "mdi:numeric-3-circle", "/fan_speed3",          "Fan Speed 3" },
    { "Fan-Speed4",     "fan-speed4",     "mdi:numeric-4-circle", "/fan_speed4",          "Fan Speed 4" },
    { "Fan-Speed5",     "fan-speed5",     "mdi:numeric-5-circle", "/fan_speed5",          "Fan Speed 5" },
    { "Fan-Speed6",     "fan-speed6",     "mdi:numeric-6-circle", "/fan_speed6",          "Fan Speed 6" },
    { "Fan-Timer1H",    "fan-timer1h",    "mdi:timer-1",          "/fan_timer1h",         "Fan Timer 1H" },
    { "Fan-Timer2H",    "fan-timer2h",    "mdi:timer-2",          "/fan_timer2h",         "Fan Timer 2H" },
    { "Fan-Timer4H",    "fan-timer4h",    "mdi:timer-4",          "/fan_timer4h",         "Fan Timer 4H" },
};

/* sendRFCommand(): command and repeatCount per button */
typedef struct {
    const char *command;
    int         repeat;
} novy_cmd_t;

static const novy_cmd_t s_novy[RF_FAN_FIRST] = {
    [RF_CMD_LIGHT]        = { NOVY_COMMAND_LIGHT, 2 },    /* 3 or more brighten/dim */
    [RF_CMD_POWER]        = { NOVY_COMMAND_POWER, 1 },
    [RF_CMD_PLUS]         = { NOVY_COMMAND_PLUS, 1 },
    [RF_CMD_MINUS]        = { NOVY_COMMAND_MINUS, 1 },
    [RF_CMD_NOVY]         = { NOVY_COMMAND_NOVY, 1 },
    [RF_CMD_BRIGHTDIM]    = { NOVY_COMMAND_LIGHT, 4 },
    [RF_CMD_BRIGHTDIMLOW] = { NOVY_COMMAND_LIGHT, 10 },
};

/* sendFanRFCommand(): code array, cycle size and log name per button */
typedef struct {
    const uint16_t *codes;
    int             cycle;
    const char     *name;
} fan_cmd_t;

static const fan_cmd_t s_fan[RF_FAN_COUNT] = {
    { ALL_OFF_CODES,      ALL_OFF_CYCLE_SIZE,      "All Off" },
    { FAN_OFF_CODES,      FAN_OFF_CYCLE_SIZE,      "Fan Off" },
    { LIGHT_TOGGLE_CODES, LIGHT_TOGGLE_CYCLE_SIZE, "Light Toggle" },
    { FAN_FORWARD_CODES,  FAN_FORWARD_CYCLE_SIZE,  "Forward" },
    { FAN_REVERSE_CODES,  FAN_REVERSE_CYCLE_SIZE,  "Reverse" },
    { FAN_WAVE_CODES,     FAN_WAVE_CYCLE_SIZE,     "Wave" },
    { SPEED_1_CODES,      SPEED_1_CYCLE_SIZE,      "Speed 1" },
    { SPEED_2_CODES,      SPEED_2_CYCLE_SIZE,      "Speed 2" },
    { SPEED_3_CODES,      SPEED_3_CYCLE_SIZE,      "Speed 3" },
    { SPEED_4_CODES,      SPEED_4_CYCLE_SIZE,      "Speed 4" },
    { SPEED_5_CODES,      SPEED_5_CYCLE_SIZE,      "Speed 5" },
    { SPEED_6_CODES,      SPEED_6_CYCLE_SIZE,      "Speed 6" },
    { TIMER_1H_CODES,     TIMER_1H_CYCLE_SIZE,     "Timer 1H" },
    { TIMER_2H_CODES,     TIMER_2H_CYCLE_SIZE,     "Timer 2H" },
    { TIMER_4H_CODES,     TIMER_4H_CYCLE_SIZE,     "Timer 4H" },
};

const rf_cmd_info_t *rf_cmd_info(int cmd)
{
    return (cmd >= 0 && cmd < RF_CMD_COUNT) ? &s_info[cmd] : NULL;
}

int rf_cmd_from_id(const char *id)
{
    for (int c = 0; c < RF_CMD_COUNT; c++)
        if (strcmp(s_info[c].id, id) == 0) return c;
    return -1;
}

int rf_cmd_from_path(const char *path)
{
    for (int c = 0; c < RF_CMD_COUNT; c++)
        if (strcmp(s_info[c].path, path) == 0) return c;
    return -1;
}

bool rf_cmd_is_fan(int cmd)
{
    return cmd >= RF_FAN_FIRST && cmd < RF_CMD_COUNT;
}

void rf_cmd_init(rf_cmd_state_t *s, rcs_out_fn out, void *ctx)
{
    memset(s->fan_cycle, 0, sizeof(s->fan_cycle));
    rcs_init(&s->rcs, out, ctx);                       /* RCSwitch transmitter = RCSwitch(); */
    rcs_enable_transmit(&s->rcs, RF_TX_GPIO);          /* setup() */
    rcs_set_pulse_length(&s->rcs, 350);
    rcs_set_protocol(&s->rcs, 12);
}

static void send_novy(rf_cmd_state_t *s, int cmd, char *log, size_t log_len)
{
    char code[64];
    rcs_enable_transmit(&s->rcs, RF_TX_GPIO);          /* "Reconfigure transmitter for NOVY" */
    rcs_set_pulse_length(&s->rcs, 350);
    rcs_set_protocol(&s->rcs, 12);
    snprintf(code, sizeof(code), "%s%s%s", NOVY_DEVICE_CODE[NOVY_CHANNEL - 1], NOVY_PREFIX, s_novy[cmd].command);
    for (int i = 0; i < s_novy[cmd].repeat; i++) rcs_send_str(&s->rcs, code);
    if (log) snprintf(log, log_len, "RF sent: %s (%s), repeat signal: %d", s_info[cmd].name, code, s_novy[cmd].repeat);
}

static void send_fan(rf_cmd_state_t *s, int cmd, char *log, size_t log_len)
{
    const fan_cmd_t *f = &s_fan[cmd - RF_FAN_FIRST];
    int *idx = &s->fan_cycle[cmd - RF_FAN_FIRST];
    rcs_enable_transmit(&s->rcs, RF_TX_GPIO);          /* "Reconfigure transmitter for ceiling fan" */
    rcs_set_protocol(&s->rcs, 1);
    rcs_set_pulse_length(&s->rcs, 248);
    rcs_set_repeat_transmit(&s->rcs, 3);
    unsigned long counter = f->codes[*idx];
    unsigned long full    = (KNOWN_DEVICE_ID << 12) | counter;
    rcs_send(&s->rcs, (uint32_t)full, 32);
    if (log)
        snprintf(log, log_len, "Fan RF sent: %s (DeviceID: 0x%lx, Counter: %lu, FullCode: %lu, Cycle: %d/%d)",
                 f->name, KNOWN_DEVICE_ID, counter, full, *idx, f->cycle);
    *idx = (*idx + 1) % f->cycle;
}

const char *rf_cmd_novy_channel_code(void)
{
    return NOVY_DEVICE_CODE[NOVY_CHANNEL - 1];
}

int rf_cmd_novy_channel(void)
{
    return NOVY_CHANNEL;
}

uint32_t rf_cmd_fan_device_id(void)
{
    return (uint32_t)KNOWN_DEVICE_ID;
}

const uint16_t *rf_cmd_fan_codes(int cmd, int *cycle)
{
    if (!rf_cmd_is_fan(cmd)) return NULL;
    if (cycle) *cycle = s_fan[cmd - RF_FAN_FIRST].cycle;
    return s_fan[cmd - RF_FAN_FIRST].codes;
}

void rf_cmd_send(rf_cmd_state_t *s, int cmd, char *log, size_t log_len)
{
    if (log && log_len) log[0] = '\0';
    if (cmd < 0 || cmd >= RF_CMD_COUNT) return;
    if (rf_cmd_is_fan(cmd)) send_fan(s, cmd, log, log_len);
    else                    send_novy(s, cmd, log, log_len);
}
