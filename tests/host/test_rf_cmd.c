/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include <ctype.h>
#include <stdio.h>
#include "check.h"
#include "rf_cmd.h"
#include "rf_wave.h"

#define MAXSEG 8192
static rf_sym_t       g_buf[2048];
static rf_wave_t      g_w;
static rf_cmd_state_t g_st;
static char           g_log[200];
static int            g_lv[MAXSEG];
static uint32_t       g_us[MAXSEG];
static size_t         g_nseg;

static void fresh(void)
{
    rf_wave_init(&g_w, g_buf, 2048);
    rf_cmd_init(&g_st, rf_wave_out, &g_w);
}

static void send(int cmd)
{
    rf_wave_reset(&g_w);
    rf_cmd_send(&g_st, cmd, g_log, sizeof(g_log));
    g_nseg = rf_wave_segments(&g_w, g_lv, g_us, MAXSEG);
}

static int count_seg(int level, uint32_t us)
{
    int n = 0;
    for (size_t i = 0; i < g_nseg; i++) n += (g_lv[i] == level && g_us[i] == us);
    return n;
}

static void test_table(void)
{
    const rf_cmd_info_t *i;
    CHECK(RF_CMD_COUNT == 22 && RF_FAN_FIRST == 7 && RF_FAN_COUNT == 15);
    i = rf_cmd_info(RF_CMD_LIGHT);
    CHECK(strcmp(i->name, "Light") == 0 && strcmp(i->id, "light") == 0 && strcmp(i->icon, "mdi:lightbulb") == 0);
    CHECK(strcmp(i->path, "/toggleLight") == 0 && strcmp(i->reply, "Light toggled") == 0);
    i = rf_cmd_info(RF_CMD_BRIGHTDIMLOW);
    CHECK(strcmp(i->name, "Brightness-low") == 0 && strcmp(i->id, "brightness-low") == 0);
    CHECK(strcmp(i->icon, "mdi:brightness-5") == 0 && strcmp(i->path, "/toggleBrightnesslow") == 0);
    CHECK(strcmp(i->reply, "Brightness low toggled") == 0);
    i = rf_cmd_info(RF_CMD_FAN_ALL_OFF);
    CHECK(strcmp(i->name, "Fan-All-Off") == 0 && strcmp(i->id, "fan-all-off") == 0 && strcmp(i->icon, "mdi:power-off") == 0);
    CHECK(strcmp(i->path, "/fan_all_off") == 0 && strcmp(i->reply, "Fan All Off") == 0);
    i = rf_cmd_info(RF_CMD_FAN_SPEED5);
    CHECK(strcmp(i->path, "/fan_speed5") == 0 && strcmp(i->reply, "Fan Speed 5") == 0);
    i = rf_cmd_info(RF_CMD_FAN_TIMER4H);
    CHECK(strcmp(i->name, "Fan-Timer4H") == 0 && strcmp(i->id, "fan-timer4h") == 0 && strcmp(i->icon, "mdi:timer-4") == 0);
    CHECK(strcmp(i->path, "/fan_timer4h") == 0 && strcmp(i->reply, "Fan Timer 4H") == 0);
    CHECK(rf_cmd_info(-1) == NULL && rf_cmd_info(RF_CMD_COUNT) == NULL);

    for (int c = 0; c < RF_CMD_COUNT; c++) {        /* id = toLowerCase(name), paths unique */
        char low[32];
        size_t k;
        i = rf_cmd_info(c);
        for (k = 0; i->name[k] && k < sizeof(low) - 1; k++) low[k] = (char)tolower((unsigned char)i->name[k]);
        low[k] = '\0';
        CHECK(strcmp(low, i->id) == 0);
        CHECK(rf_cmd_from_id(i->id) == c && rf_cmd_from_path(i->path) == c);
    }
    CHECK(rf_cmd_from_id("Light") == -1 && rf_cmd_from_id("") == -1 && rf_cmd_from_id("fan-speed7") == -1);
    CHECK(rf_cmd_from_path("/toggleLight/") == -1 && rf_cmd_from_path("toggleLight") == -1);
    CHECK(!rf_cmd_is_fan(RF_CMD_BRIGHTDIMLOW) && rf_cmd_is_fan(RF_CMD_FAN_ALL_OFF) && rf_cmd_is_fan(RF_CMD_FAN_TIMER4H));
    CHECK(!rf_cmd_is_fan(-1) && !rf_cmd_is_fan(RF_CMD_COUNT));
    CHECK(rf_tx_gpio == 4);
}

static void test_init(void)
{
    fresh();
    CHECK(g_st.rcs.tx_pin == 4 && g_st.rcs.repeat_transmit == 10);
    CHECK(g_st.rcs.protocol.pulse_length == 320 && g_st.rcs.protocol.inverted);   /* 350 overwritten */
    for (int k = 0; k < RF_FAN_COUNT; k++) CHECK(g_st.fan_cycle[k] == 0);
    CHECK(g_w.halves == 0);                      /* init sends nothing */
}

/* Expected NOVY log line: the channel code of the RF codes in use + the generic prefix and command. */
static void want_novy(char *out, size_t len, const char *name, const char *command, int repeat)
{
    snprintf(out, len, "RF sent: %s (%s0101%s), repeat signal: %d", name, rf_cmd_novy_channel_code(), command, repeat);
}

static void test_novy(void)
{
    char   bits[32], want[160];
    size_t k = 0;
    CHECK(rf_cmd_novy_channel() >= 1 && rf_cmd_novy_channel() <= 10 && strlen(rf_cmd_novy_channel_code()) == 4);
    snprintf(bits, sizeof(bits), "%s01010101", rf_cmd_novy_channel_code());   /* Plus: channel, prefix 0101, 0101 */
    fresh();
    send(RF_CMD_PLUS);
    want_novy(want, sizeof(want), "Plus", "0101", 1);
    CHECK(strcmp(g_log, want) == 0);
    CHECK(count_seg(0, 11520) == 10);            /* 10 frames per send() at boot */
    for (const char *b = bits; *b; b++) {        /* first frame: low first, 320 µs pulses */
        CHECK(g_lv[k] == 0 && g_us[k] == (*b == '1' ? 640u : 320u));
        k++;
        CHECK(g_lv[k] == 1 && g_us[k] == (*b == '1' ? 320u : 640u));
        k++;
    }
    CHECK(g_lv[k] == 0 && g_us[k] == 11520 && g_lv[k + 1] == 1 && g_us[k + 1] == 320);
    CHECK(g_w.level == 0 && g_w.total_us == 10ull * (12 * 960 + 11840));

    send(RF_CMD_LIGHT);
    want_novy(want, sizeof(want), "Light", "0111010001", 2);
    CHECK(strcmp(g_log, want) == 0 && count_seg(0, 11520) == 20);
    send(RF_CMD_POWER);
    want_novy(want, sizeof(want), "Power", "0111010011", 1);
    CHECK(strcmp(g_log, want) == 0);
    send(RF_CMD_MINUS);
    want_novy(want, sizeof(want), "Minus", "0110", 1);
    CHECK(strcmp(g_log, want) == 0);
    send(RF_CMD_NOVY);
    want_novy(want, sizeof(want), "Novy", "0100", 1);
    CHECK(strcmp(g_log, want) == 0);
    send(RF_CMD_BRIGHTDIM);
    want_novy(want, sizeof(want), "Brightness", "0111010001", 4);
    CHECK(strcmp(g_log, want) == 0 && count_seg(0, 11520) == 40);
    send(RF_CMD_BRIGHTDIMLOW);
    want_novy(want, sizeof(want), "Brightness-low", "0111010001", 10);
    CHECK(strcmp(g_log, want) == 0);
    CHECK(count_seg(0, 11520) == 100 && rf_wave_symbols(&g_w) == 1900 && !g_w.overflow);
}

/* Expected fan log line for step `step` of a fan button with log name `name`. */
static void want_fan(char *out, size_t len, int cmd, const char *name, int step)
{
    int             cycle;
    const uint16_t *codes = rf_cmd_fan_codes(cmd, &cycle);
    unsigned long   c = codes[step % cycle];
    snprintf(out, len, "Fan RF sent: %s (DeviceID: 0x%lx, Counter: %lu, FullCode: %lu, Cycle: %d/%d)", name,
             (unsigned long)rf_cmd_fan_device_id(), c, ((unsigned long)rf_cmd_fan_device_id() << 12) | c, step % cycle,
             cycle);
}

static void test_fan_and_repeat_state(void)
{
    char           want[160];
    int            cycle;
    const uint32_t code = (rf_cmd_fan_device_id() << 12) | rf_cmd_fan_codes(RF_CMD_FAN_SPEED1, &cycle)[0];
    CHECK(rf_cmd_fan_device_id() < (1u << 20) && cycle == 8 && rf_cmd_fan_codes(RF_CMD_LIGHT, NULL) == NULL);
    for (int c = RF_FAN_FIRST; c < RF_CMD_COUNT; c++)
        for (int i = 0; i < 8; i++) CHECK(rf_cmd_fan_codes(c, NULL)[i] < 4096);
    fresh();
    send(RF_CMD_FAN_SPEED1);
    want_fan(want, sizeof(want), RF_CMD_FAN_SPEED1, "Speed 1", 0);
    CHECK(strcmp(g_log, want) == 0);
    CHECK(count_seg(0, 7688) == 3 && g_st.rcs.repeat_transmit == 3);
    CHECK(g_st.rcs.protocol.pulse_length == 248 && !g_st.rcs.protocol.inverted);
    for (int b = 31, k = 0; b >= 0; b--, k += 2) {   /* first frame, high first, MSB first */
        bool one = (code >> b) & 1u;
        CHECK(g_lv[k] == 1 && g_us[k] == (one ? 744u : 248u) && g_lv[k + 1] == 0 && g_us[k + 1] == (one ? 248u : 744u));
    }
    CHECK(g_lv[64] == 1 && g_us[64] == 248 && g_lv[65] == 0 && g_us[65] == 7688);

    send(RF_CMD_LIGHT);                          /* the sketch never resets the repeat count */
    CHECK(count_seg(0, 11520) == 6 && g_st.rcs.protocol.pulse_length == 320);
    send(RF_CMD_BRIGHTDIMLOW);
    CHECK(count_seg(0, 11520) == 30);
}

static void test_cycles(void)
{
    char want[160];
    fresh();
    for (int n = 0; n < 17; n++) {
        send(RF_CMD_FAN_SPEED1);
        want_fan(want, sizeof(want), RF_CMD_FAN_SPEED1, "Speed 1", n);
        CHECK(strcmp(g_log, want) == 0);
    }
    send(RF_CMD_FAN_LIGHT);                      /* every button has its own index */
    want_fan(want, sizeof(want), RF_CMD_FAN_LIGHT, "Light Toggle", 0);
    CHECK(strcmp(g_log, want) == 0);
    CHECK(g_st.fan_cycle[RF_CMD_FAN_SPEED1 - RF_FAN_FIRST] == 1 && g_st.fan_cycle[RF_CMD_FAN_LIGHT - RF_FAN_FIRST] == 1);
    send(RF_CMD_FAN_TIMER2H);
    want_fan(want, sizeof(want), RF_CMD_FAN_TIMER2H, "Timer 2H", 0);
    CHECK(strcmp(g_log, want) == 0);
    send(RF_CMD_FAN_ALL_OFF);
    want_fan(want, sizeof(want), RF_CMD_FAN_ALL_OFF, "All Off", 0);
    CHECK(strcmp(g_log, want) == 0);
}

static void test_invalid(void)
{
    fresh();
    strcpy(g_log, "x");
    send(RF_CMD_COUNT);
    CHECK(g_w.halves == 0 && g_log[0] == '\0');
    send(-1);
    CHECK(g_w.halves == 0 && g_log[0] == '\0');
    rf_cmd_send(&g_st, RF_CMD_LIGHT, NULL, 0);   /* log optional */
    CHECK(g_w.halves > 0);
}

int main(void)
{
    test_table();
    test_init();
    test_novy();
    test_fan_and_repeat_state();
    test_cycles();
    test_invalid();
    CHECK_DONE();
}
