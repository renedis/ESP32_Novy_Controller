/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include <stdio.h>
#include "check.h"
#include "light.h"

static void test_topics(void)
{
    CHECK(light_topic("rgb_light/set") == LIGHT_TOPIC_STATE);
    CHECK(light_topic("rgb_light/brightness/state") == LIGHT_TOPIC_BRIGHTNESS);
    CHECK(light_topic("rgb_light/rgb/state") == LIGHT_TOPIC_RGB);
    CHECK(light_topic("rgb_light/brightness/set") == LIGHT_TOPIC_NONE);   /* state topics, published by us */
    CHECK(light_topic("rgb_light/rgb/set") == LIGHT_TOPIC_NONE);
    CHECK(light_topic("rgb_light/status") == LIGHT_TOPIC_NONE);
    CHECK(light_topic("button/light/set") == LIGHT_TOPIC_NONE);
}

static void test_commands(void)
{
    light_t l;
    light_init(&l);
    CHECK(!l.on && l.brightness == 255 && l.r == 255 && l.g == 255 && l.b == 255);
    CHECK(light_apply(&l, LIGHT_TOPIC_STATE, "ON") && l.on);
    CHECK(light_apply(&l, LIGHT_TOPIC_STATE, "OFF") && !l.on);
    CHECK(!light_apply(&l, LIGHT_TOPIC_STATE, "on") && !l.on);            /* payload_on is "ON" */
    CHECK(!light_apply(&l, LIGHT_TOPIC_STATE, "TOGGLE"));
    CHECK(light_apply(&l, LIGHT_TOPIC_BRIGHTNESS, "128") && l.brightness == 128);
    CHECK(light_apply(&l, LIGHT_TOPIC_BRIGHTNESS, "0") && l.brightness == 0);
    CHECK(light_apply(&l, LIGHT_TOPIC_BRIGHTNESS, "255") && l.brightness == 255);
    const char *bad_bri[] = { "256", "-1", "", "12a", "1.5", " 5", "1000" };
    for (size_t i = 0; i < sizeof(bad_bri) / sizeof(bad_bri[0]); i++)
        CHECK(!light_apply(&l, LIGHT_TOPIC_BRIGHTNESS, bad_bri[i]) && l.brightness == 255);
    CHECK(light_apply(&l, LIGHT_TOPIC_RGB, "255,100,0") && l.r == 255 && l.g == 100 && l.b == 0);
    const char *bad_rgb[] = { "1,2", "1,2,3,4", "256,0,0", "a,b,c", "", "1,,3", "1,2,3,", "-1,2,3" };
    for (size_t i = 0; i < sizeof(bad_rgb) / sizeof(bad_rgb[0]); i++)
        CHECK(!light_apply(&l, LIGHT_TOPIC_RGB, bad_rgb[i]) && l.r == 255 && l.g == 100 && l.b == 0);
    CHECK(!light_apply(&l, LIGHT_TOPIC_NONE, "ON"));
}

static void test_json_and_output(void)
{
    light_t l;
    char    js[96];
    uint8_t r, g, b;
    light_init(&l);
    CHECK(light_state_json(&l, js, sizeof(js)) > 0);
    CHECK(strcmp(js, "{\"state\":\"OFF\",\"brightness\":255,\"rgb\":[255,255,255]}") == 0);
    light_output(&l, &r, &g, &b);
    CHECK(r == 0 && g == 0 && b == 0);

    light_apply(&l, LIGHT_TOPIC_STATE, "ON");
    light_apply(&l, LIGHT_TOPIC_BRIGHTNESS, "128");
    light_apply(&l, LIGHT_TOPIC_RGB, "255,100,0");
    light_state_json(&l, js, sizeof(js));
    CHECK(strcmp(js, "{\"state\":\"ON\",\"brightness\":128,\"rgb\":[255,100,0]}") == 0);
    light_output(&l, &r, &g, &b);
    CHECK(r == 128 && g == 50 && b == 0);
    CHECK(light_state_json(&l, js, 10) < 0);
}

int main(void)
{
    test_topics();
    test_commands();
    test_json_and_output();
    CHECK_DONE();
}
