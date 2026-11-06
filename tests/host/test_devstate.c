/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include <stdio.h>
#include "check.h"
#include "devstate.h"
#include "rf_cmd.h"

static bool same(const devstate_t *a, const devstate_t *b)
{
    return a->novy_light == b->novy_light && a->fan_speed == b->fan_speed && a->fan_light == b->fan_light &&
           a->fan_reverse == b->fan_reverse;
}

static bool msg_is(const mqtt_msg_t *m, const char *topic, const char *payload, bool retain)
{
    return strcmp(m->topic, topic) == 0 && strcmp(m->payload, payload) == 0 && m->retain == retain;
}

static void test_init(void)
{
    devstate_t s;
    devstate_init(&s);
    CHECK(!s.novy_light && s.fan_speed == 0 && !s.fan_light && !s.fan_reverse);
    CHECK(strcmp(devstate_direction(&s), "forward") == 0);
}

static void test_apply(void)
{
    devstate_t s, before;
    devstate_init(&s);
    devstate_apply(&s, RF_CMD_LIGHT);
    CHECK(s.novy_light);
    devstate_apply(&s, RF_CMD_LIGHT);
    CHECK(!s.novy_light);
    devstate_apply(&s, RF_CMD_FAN_LIGHT);
    CHECK(s.fan_light);
    for (int n = 1; n <= 6; n++) {
        devstate_apply(&s, RF_CMD_FAN_SPEED1 + n - 1);
        CHECK(s.fan_speed == n);
    }
    devstate_apply(&s, RF_CMD_FAN_OFF);                  /* speed 0, light stays */
    CHECK(s.fan_speed == 0 && s.fan_light);
    devstate_apply(&s, RF_CMD_FAN_SPEED3);
    devstate_apply(&s, RF_CMD_FAN_ALL_OFF);              /* speed 0 and light off */
    CHECK(s.fan_speed == 0 && !s.fan_light);
    devstate_apply(&s, RF_CMD_FAN_REVERSE);
    CHECK(s.fan_reverse && strcmp(devstate_direction(&s), "reverse") == 0);
    devstate_apply(&s, RF_CMD_FAN_FORWARD);
    CHECK(!s.fan_reverse);

    static const int no_change[] = { RF_CMD_POWER, RF_CMD_PLUS, RF_CMD_MINUS, RF_CMD_NOVY, RF_CMD_BRIGHTDIM,
                                     RF_CMD_BRIGHTDIMLOW, RF_CMD_FAN_WAVE, RF_CMD_FAN_TIMER1H, RF_CMD_FAN_TIMER2H,
                                     RF_CMD_FAN_TIMER4H, -1, RF_CMD_COUNT };
    devstate_apply(&s, RF_CMD_LIGHT);
    devstate_apply(&s, RF_CMD_FAN_SPEED2);
    before = s;
    for (size_t i = 0; i < sizeof(no_change) / sizeof(no_change[0]); i++) {
        devstate_apply(&s, no_change[i]);
        CHECK(same(&s, &before));
    }
}

static void test_press_msgs(void)
{
    devstate_t s;
    mqtt_msg_t m[8];
    devstate_init(&s);
    devstate_apply(&s, RF_CMD_LIGHT);
    CHECK(devstate_press_msgs(&s, RF_CMD_LIGHT, TEST_BASE, m, 8) == DEVSTATE_PRESS_MSGS);
    CHECK(msg_is(&m[0], TEST_BASE "/state/novy_light", "ON", true));
    CHECK(msg_is(&m[1], TEST_BASE "/state/fan_speed", "0", true));
    CHECK(msg_is(&m[2], TEST_BASE "/state/fan_light", "OFF", true));
    CHECK(msg_is(&m[3], TEST_BASE "/state/fan_direction", "forward", true));
    CHECK(msg_is(&m[4], TEST_BASE "/button/light/state", "PRESS", false));
    CHECK(msg_is(&m[5], TEST_BASE "/button/light/state", "", false));

    devstate_apply(&s, RF_CMD_FAN_SPEED6);
    devstate_apply(&s, RF_CMD_FAN_REVERSE);
    devstate_press_msgs(&s, RF_CMD_FAN_REVERSE, TEST_BASE, m, 8);
    CHECK(msg_is(&m[1], TEST_BASE "/state/fan_speed", "6", true));
    CHECK(msg_is(&m[3], TEST_BASE "/state/fan_direction", "reverse", true));
    CHECK(msg_is(&m[4], TEST_BASE "/button/fan-reverse/state", "PRESS", false));

    CHECK(devstate_press_msgs(&s, RF_CMD_COUNT, TEST_BASE, m, 8) == 0);
    CHECK(devstate_press_msgs(&s, RF_CMD_LIGHT, TEST_BASE, m, 5) == 0);
}

/* The four state topics after boot equal the retained values captured on the broker. */
static void test_golden_state_topics(void)
{
    devstate_t s;
    mqtt_msg_t m[8];
    char line[256], want[4][160];
    int n = 0;
    FILE *f = fopen(FIXTURES "/state_topics.txt", "r");
    CHECK(f != NULL);
    if (!f) return;
    while (fgets(line, sizeof(line), f) && n < 4) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strstr(line, "/state/")) snprintf(want[n++], sizeof(want[0]), "%s", line);
    }
    fclose(f);
    CHECK(n == 4);
    devstate_init(&s);
    devstate_press_msgs(&s, RF_CMD_POWER, TEST_BASE, m, 8);
    for (int i = 0; i < 4 && i < n; i++) {
        char got[160];
        snprintf(got, sizeof(got), "%s %s", m[i].topic, m[i].payload);
        CHECK(strcmp(got, want[i]) == 0);
    }
}

int main(void)
{
    test_init();
    test_apply();
    test_press_msgs();
    test_golden_state_topics();
    CHECK_DONE();
}
