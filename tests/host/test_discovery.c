/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include "check.h"
#include "cJSON.h"
#include "discovery.h"

#define MAXLINES 64
static char *g_topic[MAXLINES], *g_payload[MAXLINES];
static int   g_n;

/* <fixtures>/discovery.txt: "<topic> <payload>" per line; topics may contain spaces (stale entries of an
 * older firmware under another device name are part of the capture). */
static void load_golden(void)
{
    static char text[32768];
    FILE *f = fopen(FIXTURES "/discovery.txt", "r");
    CHECK(f != NULL);
    if (!f) return;
    size_t len = fread(text, 1, sizeof(text) - 1, f);
    fclose(f);
    text[len] = '\0';
    for (char *line = strtok(text, "\n"); line && g_n < MAXLINES; line = strtok(NULL, "\n")) {
        char *sep = strstr(line, " {");
        if (!sep) continue;
        *sep = '\0';
        g_topic[g_n]   = line;
        g_payload[g_n] = sep + 1;
        g_n++;
    }
}

static const char *golden_for(const char *topic)
{
    for (int i = 0; i < g_n; i++)
        if (strcmp(g_topic[i], topic) == 0) return g_payload[i];
    return NULL;
}

static void test_against_capture(void)
{
    char topic[DISC_TOPIC_MAX], payload[DISC_PAYLOAD_MAX];
    int  ours_found = 0, golden_ours = 0;
    load_golden();
    CHECK(g_n == 31);
    for (int i = 0; i < g_n; i++) golden_ours += strstr(g_topic[i], "/" TEST_DEVICE) != NULL;
    CHECK(golden_ours == DISC_COUNT);

    for (int i = 0; i < DISC_COUNT; i++) {
        CHECK(disc_build(i, "ESP", TEST_DEVICE, topic, sizeof(topic), payload, sizeof(payload)));
        CHECK(strstr(topic, "/" TEST_DEVICE) != NULL);       /* never one of the stale entries */
        const char *want = golden_for(topic);
        if (!want) { printf("  not in capture: %s\n", topic); CHECK(want != NULL); continue; }
        ours_found++;
        if (strcmp(payload, want) != 0) printf("  differs: %s\n    ours: %s\n    want: %s\n", topic, payload, want);
        CHECK(strcmp(payload, want) == 0);                       /* byte-equal */
        cJSON *a = cJSON_Parse(payload), *b = cJSON_Parse(want);
        CHECK(a && b && cJSON_Compare(a, b, true));              /* and JSON-equal */
        cJSON_Delete(a);
        cJSON_Delete(b);
    }
    CHECK(ours_found == DISC_COUNT);
}

static void test_fields(void)
{
    char topic[DISC_TOPIC_MAX], payload[DISC_PAYLOAD_MAX];
    char object_id[64] = "led_";
    for (size_t i = 0; TEST_DEVICE[i] && i < sizeof(object_id) - 5; i++)
        object_id[4 + i] = (char)tolower((unsigned char)TEST_DEVICE[i]);
    CHECK(disc_build(0, "ESP", TEST_DEVICE, topic, sizeof(topic), payload, sizeof(payload)));
    CHECK(strcmp(topic, "homeassistant/button/" TEST_DEVICE "/light/config") == 0);
    CHECK(disc_build(DISC_LIGHT, "ESP", TEST_DEVICE, topic, sizeof(topic), payload, sizeof(payload)));
    CHECK(strcmp(topic, "homeassistant/light/" TEST_DEVICE "_rgb_light/config") == 0);
    cJSON *j = cJSON_Parse(payload);
    CHECK(j && strcmp(cJSON_GetObjectItem(j, "object_id")->valuestring, object_id) == 0);
    CHECK(j && strcmp(cJSON_GetObjectItem(j, "rgb_command_topic")->valuestring, TEST_BASE "/rgb_light/rgb/state") == 0);
    cJSON_Delete(j);

    /* Another prefix moves the topics, never the unique ids or the discovery topic. */
    CHECK(disc_build(13, "home", TEST_DEVICE, topic, sizeof(topic), payload, sizeof(payload)));
    CHECK(strcmp(topic, "homeassistant/button/" TEST_DEVICE "/fan-speed1/config") == 0);
    j = cJSON_Parse(payload);
    CHECK(j && strcmp(cJSON_GetObjectItem(j, "unique_id")->valuestring, TEST_DEVICE "_fan-speed1") == 0);
    CHECK(j && strcmp(cJSON_GetObjectItem(j, "command_topic")->valuestring, "home/" TEST_DEVICE "/button/fan-speed1/set") == 0);
    cJSON_Delete(j);

    /* Escaping: a quote or backslash cannot break the JSON. */
    CHECK(disc_build(0, "a\"b\\c", TEST_DEVICE, topic, sizeof(topic), payload, sizeof(payload)));
    j = cJSON_Parse(payload);
    CHECK(j && strcmp(cJSON_GetObjectItem(j, "availability_topic")->valuestring, "a\"b\\c/" TEST_DEVICE "/status") == 0);
    cJSON_Delete(j);
}

static void test_limits(void)
{
    char topic[DISC_TOPIC_MAX], payload[DISC_PAYLOAD_MAX], small[200];
    CHECK(!disc_build(-1, "ESP", TEST_DEVICE, topic, sizeof(topic), payload, sizeof(payload)));
    CHECK(!disc_build(DISC_COUNT, "ESP", TEST_DEVICE, topic, sizeof(topic), payload, sizeof(payload)));
    CHECK(!disc_build(0, "ESP", TEST_DEVICE, topic, sizeof(topic), small, sizeof(small)));
    CHECK(!disc_build(0, "ESP", TEST_DEVICE, small, 10, payload, sizeof(payload)));
    size_t longest = 0;
    for (int i = 0; i < DISC_COUNT; i++) {
        disc_build(i, "ESP", TEST_DEVICE, topic, sizeof(topic), payload, sizeof(payload));
        if (strlen(payload) > longest) longest = strlen(payload);
    }
    CHECK(longest < DISC_PAYLOAD_MAX - 200);      /* room for a longer prefix */
}

int main(void)
{
    test_against_capture();
    test_fields();
    test_limits();
    CHECK_DONE();
}
