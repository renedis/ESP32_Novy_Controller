/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
/* Peer list: links to the other devices, shown as a button bar on every page.
 *
 * Text form (NVS string key "peers", the API and the settings page):
 *   "Label=URL;Label=URL"   entries split on ';' (newlines work too)
 * - Label: 1-24 characters [A-Za-z0-9 _.-]; blanks around it are dropped.
 * - URL: at most 96 characters, starts with http:// or https:// followed by a host,
 *   printable ASCII without blanks, quotes or <>.
 * - At most 12 entries and 512 characters. A repeated label (ignoring case) keeps
 *   the first entry. Empty entries are skipped.
 *
 * Pure C without ESP-IDF, host-tested in tests/host/test_peers.c. */
#pragma once
#include <stdbool.h>
#include <stddef.h>

#define PEERS_MAX       12
#define PEERS_LABEL_MAX 24
#define PEERS_URL_MAX   96
#define PEERS_TEXT_MAX  512     /* serialised list, without the terminating NUL */

typedef struct {
    char label[PEERS_LABEL_MAX + 1];
    char url[PEERS_URL_MAX + 1];
} peer_t;

typedef struct {
    int    count;
    peer_t item[PEERS_MAX];
} peers_t;

bool        peers_label_valid(const char *label);
bool        peers_url_valid(const char *url);

/* NULL when text is valid (*out then holds the list), otherwise a short reason
 * (static buffer, never echoes the input) and *out is left empty. NULL text = empty list. */
const char *peers_parse(const char *text, peers_t *out);

/* Writes "Label=URL;Label=URL" into buf. Returns its length, or -1 when it does not fit
 * (a buffer of PEERS_TEXT_MAX + 1 bytes always fits a list from peers_parse()). */
int         peers_serialise(const peers_t *p, char *buf, size_t len);
