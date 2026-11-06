/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
/* Minimal assertions for host unit tests. Include once per test binary. */
#pragma once
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Set by the Makefile: the device name and the fixture directory (public or .local). */
#ifndef TEST_DEVICE
#define TEST_DEVICE "NOVY-RF"
#endif
#ifndef FIXTURES
#define FIXTURES "fixtures"
#endif
#define TEST_BASE "ESP/" TEST_DEVICE             /* <MQTT prefix>/<device> */

static int g_pass = 0, g_fail = 0;

#define CHECK(cond) do { \
    if (cond) g_pass++; \
    else { g_fail++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

#define CHECK_DONE() do { \
    printf("%s: %d passed, %d failed\n", __FILE__, g_pass, g_fail); \
    return g_fail ? 1 : 0; \
} while (0)
