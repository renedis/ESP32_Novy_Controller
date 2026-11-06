// ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0
// Firmware against the Arduino firmware 2.3: rc-switch 2.6.4 (ref/RCSwitch.*, verbatim) and the
// sketch's own button, RF and press functions (sketch_ref.inc, verbatim; the public copy takes the
// default Novy channel from config_rf.h), built with the RF part of config.h (config_rf.h). Fake
// digitalWrite()/delayMicroseconds() record what would go on air; the firmware's
// rf_cmd/rcswitch/devstate modules must produce the same, step for step.
#include <stdio.h>
#include <string>
#include <vector>
#include "Arduino.h"
#include "RCSwitch.h"

extern "C" {
#include "devstate.h"
#include "rf_cmd.h"
#include "rf_wave.h"
}
#include "check.h"

#define BUF_SYMS 4096

// ---- Arduino shim ---------------------------------------------------------------------------
struct RawOp {
    char     op;                                  // 'W' digitalWrite level, 'D' delay µs
    unsigned v;
};
static rf_sym_t           g_ref_buf[BUF_SYMS];
static rf_wave_t          g_ref_wave;
static std::vector<RawOp> g_raw;
static int                g_out_pin = -1;
static int                g_writes_other_pins;

void pinMode(int pin, int mode)
{
    if (mode == OUTPUT) g_out_pin = pin;
}
void digitalWrite(int pin, int level)
{
    if (pin != g_out_pin) { g_writes_other_pins++; return; }
    rf_wave_level(&g_ref_wave, level);
    g_raw.push_back({ 'W', (unsigned)level });
}
void delayMicroseconds(unsigned int us)
{
    rf_wave_delay(&g_ref_wave, us);
    g_raw.push_back({ 'D', us });
}
unsigned long micros(void) { return 0; }
void attachInterrupt(int, void (*)(void), int) {}
void detachInterrupt(int) {}
void yield(void) {}

// ---- what the sketch talks to -----------------------------------------------------------------
struct Msg {
    std::string topic, payload;
    bool        retain;
};
static std::vector<Msg>         g_pub;
static std::vector<std::string> g_log;
static int                      g_flash;

struct FakePubSub {
    bool connected() { return true; }
    bool publish(const char *t, const char *p) { g_pub.push_back({ t, p, false }); return true; }
    bool publish(const char *t, const char *p, bool r) { g_pub.push_back({ t, p, r }); return true; }
};
static FakePubSub client;
static void addToLogBuffer(String m) { g_log.push_back(m.c_str()); }
static void flashRed() { g_flash++; }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include "config_rf.h"                           // <fixtures>/ first, see the Makefile
#include "sketch_ref.inc"                        // <fixtures>/ (local mode) or ref/
#pragma clang diagnostic pop

// ---- both sides --------------------------------------------------------------------------------
static rf_sym_t       g_fw_buf[BUF_SYMS];
static rf_wave_t      g_fw_wave;
static rf_cmd_state_t g_fw;
static devstate_t     g_ds;

static void reset_all()
{
    transmitter = RCSwitch();                    // the sketch's globals, as at power-on
    memset(fanCycleIndex, 0, sizeof(fanCycleIndex));
    novyLightOn  = false;
    fanSpeed     = 0;
    fanLightOn   = false;
    fanDirection = "forward";
    g_out_pin    = -1;
    rf_wave_init(&g_ref_wave, g_ref_buf, BUF_SYMS);
    sketch_setup_rf();                           // the three transmitter lines of setup()
    rf_wave_init(&g_fw_wave, g_fw_buf, BUF_SYMS);
    rf_cmd_init(&g_fw, rf_wave_out, &g_fw_wave);
    devstate_init(&g_ds);
}

// Independent merge of the raw calls, to check the recorder itself.
static std::vector<std::pair<int, unsigned>> raw_segments()
{
    std::vector<std::pair<int, unsigned>> s;
    int level = 0;
    for (const RawOp &o : g_raw) {
        if (o.op == 'W') { level = (int)o.v; continue; }
        if (o.v == 0) continue;
        if (!s.empty() && s.back().first == level) s.back().second += o.v;
        else s.push_back({ level, o.v });
    }
    return s;
}

static std::vector<std::pair<int, unsigned>> wave_segments(const rf_wave_t *w)
{
    static int      lv[2 * BUF_SYMS];
    static uint32_t us[2 * BUF_SYMS];
    size_t n = rf_wave_segments(w, lv, us, 2 * BUF_SYMS);
    std::vector<std::pair<int, unsigned>> s;
    for (size_t i = 0; i < n; i++) s.push_back({ lv[i], us[i] });
    return s;
}

static bool same_wave()
{
    if (g_ref_wave.overflow || g_fw_wave.overflow) return false;
    if (g_ref_wave.halves != g_fw_wave.halves || g_ref_wave.level != g_fw_wave.level) return false;
    if (g_ref_wave.total_us != g_fw_wave.total_us) return false;
    return memcmp(g_ref_buf, g_fw_buf, sizeof(rf_sym_t) * rf_wave_symbols(&g_ref_wave)) == 0;
}

// One web press on both sides: handleButtonPress() against rf_cmd_send() + devstate.
static int g_checked_presses;
static void press_web(int cmd)
{
    char        log[200];
    mqtt_msg_t  m[DEVSTATE_PRESS_MSGS];
    rf_wave_reset(&g_ref_wave);
    rf_wave_reset(&g_fw_wave);
    g_raw.clear();
    g_pub.clear();
    g_log.clear();

    handleButtonPress((ButtonCommand)cmd);
    rf_cmd_send(&g_fw, cmd, log, sizeof(log));
    devstate_apply(&g_ds, cmd);
    int n = devstate_press_msgs(&g_ds, cmd, TEST_BASE, m, DEVSTATE_PRESS_MSGS);

    bool ok = same_wave() && raw_segments() == wave_segments(&g_ref_wave) && g_ref_wave.level == 0;
    ok = ok && g_log.size() == 1 && g_log[0] == log;
    ok = ok && n == (int)g_pub.size();
    for (int i = 0; ok && i < n; i++)
        ok = g_pub[i].topic == m[i].topic && g_pub[i].payload == m[i].payload && g_pub[i].retain == m[i].retain;
    if (!ok) printf("  mismatch on %s (press %d)\n", rf_cmd_info(cmd)->name, g_checked_presses);
    CHECK(ok);
    g_checked_presses++;
}

static void test_each_command_fresh()
{
    for (int c = 0; c < RF_CMD_COUNT; c++) {
        reset_all();
        press_web(c);
    }
}

// Long random sequence: every fan cycle step many times, NOVY before and after the first fan
// command (repeat count 10 and 3), state toggles.
static void test_sequence()
{
    uint32_t x = 0x2C0FFEE5u;
    reset_all();
    for (int c = 0; c < RF_FAN_FIRST; c++) press_web(c);   // all NOVY commands with 10 frames per send()
    for (int i = 0; i < 2000; i++) {
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;             // xorshift32
        press_web((int)(x % RF_CMD_COUNT));
    }
    for (int k = 0; k < RF_FAN_COUNT; k++) CHECK(fanCycleIndex[k] == g_fw.fan_cycle[k]);
}

static void test_every_cycle_step()
{
    reset_all();
    for (int c = RF_FAN_FIRST; c < RF_CMD_COUNT; c++)
        for (int n = 0; n < 17; n++) press_web(c);
}

// The quirk the firmware keeps: NOVY frames per send() are 10 until the first fan command, then 3.
static int frames_novy()
{
    int n = 0;
    for (auto &s : wave_segments(&g_fw_wave)) n += (s.first == 0 && s.second == 11520);
    return n;
}

static void test_repeat_state()
{
    reset_all();
    press_web(CMD_LIGHT);
    CHECK(frames_novy() == 20);
    press_web(CMD_FAN_TIMER1H);
    press_web(CMD_LIGHT);
    CHECK(frames_novy() == 6);
    press_web(CMD_BRIGHTDIMLOW);
    CHECK(frames_novy() == 30);
}

// mqttCallback(): same RF as a web press; 2.3 publishes only PRESS and "" there, the firmware
// publishes the state topics first as well (documented difference).
static void test_mqtt_path()
{
    reset_all();
    for (int c = 0; c < RF_CMD_COUNT; c++) {
        char       log[200];
        mqtt_msg_t m[DEVSTATE_PRESS_MSGS];
        String     topic = TEST_BASE "/button/" + String(rf_cmd_info(c)->id) + "/set";
        char       payload[] = "PRESS";
        rf_wave_reset(&g_ref_wave);
        rf_wave_reset(&g_fw_wave);
        g_pub.clear();
        g_log.clear();
        int flashes = g_flash;

        mqttCallback((char *)topic.c_str(), (byte *)payload, 5);
        rf_cmd_send(&g_fw, rf_cmd_from_id(rf_cmd_info(c)->id), log, sizeof(log));
        devstate_apply(&g_ds, c);
        int n = devstate_press_msgs(&g_ds, c, TEST_BASE, m, DEVSTATE_PRESS_MSGS);

        CHECK(same_wave());
        CHECK(g_flash == flashes + 1);
        CHECK(g_log.size() == 2 && g_log[1] == log);  // [0] = "HA (MQTT) message received [...] PRESS"
        CHECK(g_log.size() == 2 && g_log[0] == "HA (MQTT) message received [" + std::string(topic.c_str()) + "] PRESS");
        CHECK(g_pub.size() == 2 && n == 6);
        for (size_t i = 0; i < g_pub.size() && n == 6; i++) {
            const mqtt_msg_t &t = m[4 + i];
            CHECK(g_pub[i].topic == t.topic && g_pub[i].payload == t.payload && g_pub[i].retain == t.retain);
        }
    }
    // Topics that 2.3 ignores are not commands in the firmware either.
    const char *ignored[] = { TEST_BASE "/button/LIGHT/set", TEST_BASE "/button/unknown/set",
                              TEST_BASE "/rgb_light/set" };
    for (const char *t : ignored) {
        String ts = t;
        String id = ts.substring(String(TEST_BASE "/button/").length(), ts.length() - 4);
        char   payload[] = "ON";
        rf_wave_reset(&g_ref_wave);
        mqttCallback((char *)t, (byte *)payload, 2);
        CHECK(g_ref_wave.halves == 0);
        CHECK(!ts.startsWith(TEST_BASE "/button/") || rf_cmd_from_id(id.c_str()) == -1);
    }
}

static void test_tables()
{
    CHECK(BUTTON_COUNT == RF_CMD_COUNT);
    for (int c = 0; c < BUTTON_COUNT && c < RF_CMD_COUNT; c++) {
        CHECK((int)buttons[c].cmd == c);
        CHECK(buttons[c].name == String(rf_cmd_info(c)->name));
        CHECK(buttons[c].icon == String(rf_cmd_info(c)->icon));
        CHECK(toLowerCase(buttons[c].name) == String(rf_cmd_info(c)->id));
        CHECK(getButtonNameByCommand((ButtonCommand)c) == String(rf_cmd_info(c)->name));
    }
    CHECK(FAN_CMD_COUNT == RF_FAN_COUNT && (int)FAN_CMD_OFFSET == (int)RF_FAN_FIRST);
    CHECK(TRANSMIT_433MHZ_PIN == rf_tx_gpio);
    CHECK(HOSTNAME == String(TEST_DEVICE) && MQTT_PREFIX == String("ESP"));
    CHECK(g_writes_other_pins == 0);
}

int main()
{
    test_each_command_fresh();
    test_sequence();
    test_every_cycle_step();
    test_repeat_state();
    test_mqtt_path();
    test_tables();
    printf("test_reference: %d presses compared step by step\n", g_checked_presses);
    CHECK_DONE();
}
