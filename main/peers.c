/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include <stdio.h>
#include <string.h>
#include "peers.h"

#define STR_(x) #x
#define STR(x)  STR_(x)

static char s_msg[128];                     /* reason returned by peers_parse() */

static bool label_char(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           c == ' ' || c == '_' || c == '.' || c == '-';
}

/* Printable ASCII without blanks, quotes, backtick or <>. */
static bool url_char(char c)
{
    unsigned char u = (unsigned char)c;
    return u > 0x20 && u < 0x7f && c != '"' && c != '\'' && c != '`' && c != '<' && c != '>';
}

/* Length of the scheme prefix, 0 when the URL does not start with http:// or https://. */
static size_t scheme_len(const char *url)
{
    if (strncmp(url, "http://", 7) == 0)  return 7;
    if (strncmp(url, "https://", 8) == 0) return 8;
    return 0;
}

bool peers_label_valid(const char *label)
{
    if (!label) return false;
    size_t n = strlen(label);
    if (n == 0 || n > PEERS_LABEL_MAX || label[0] == ' ' || label[n - 1] == ' ') return false;
    for (size_t i = 0; i < n; i++)
        if (!label_char(label[i])) return false;
    return true;
}

bool peers_url_valid(const char *url)
{
    if (!url) return false;
    size_t n = strlen(url), s = scheme_len(url);
    if (n > PEERS_URL_MAX || s == 0 || n == s || url[s] == '/') return false;   /* needs a host */
    for (size_t i = s; i < n; i++)
        if (!url_char(url[i])) return false;
    return true;
}

static bool blank(char c)
{
    return c == ' ' || c == '\t';
}

static bool separator(char c)
{
    return c == ';' || c == '\n' || c == '\r';
}

/* Trims blanks off [*s, *e). */
static void trim(const char **s, const char **e)
{
    while (*s < *e && blank(**s)) (*s)++;
    while (*e > *s && blank((*e)[-1])) (*e)--;
}

static bool same_label(const char *a, const char *b)
{
    for (;; a++, b++) {
        char x = (*a >= 'A' && *a <= 'Z') ? (char)(*a - 'A' + 'a') : *a;
        char y = (*b >= 'A' && *b <= 'Z') ? (char)(*b - 'A' + 'a') : *b;
        if (x != y) return false;
        if (x == '\0') return true;
    }
}

static bool known_label(const peers_t *p, const char *label)
{
    for (int i = 0; i < p->count; i++)
        if (same_label(p->item[i].label, label)) return true;
    return false;
}

/* Empties *out; the reason names the entry (1-based) but never repeats its text. */
static const char *fail(peers_t *out, int entry, const char *why)
{
    out->count = 0;
    if (entry > 0) snprintf(s_msg, sizeof(s_msg), "Peer %d: %s", entry, why);
    else           snprintf(s_msg, sizeof(s_msg), "%s", why);
    return s_msg;
}

const char *peers_parse(const char *text, peers_t *out)
{
    size_t total = 0;                       /* length of the serialised list */
    int    entry = 0;

    out->count = 0;
    if (!text) return NULL;
    for (const char *p = text; *p; ) {
        const char *s = p, *e = p;
        while (*e && !separator(*e)) e++;
        p = *e ? e + 1 : e;
        trim(&s, &e);
        if (s == e) continue;               /* empty entry */
        entry++;

        const char *eq = memchr(s, '=', (size_t)(e - s));
        if (!eq) return fail(out, entry, "expected Label=URL");
        const char *ls = s, *le = eq, *us = eq + 1, *ue = e;
        trim(&ls, &le);
        trim(&us, &ue);
        size_t llen = (size_t)(le - ls), ulen = (size_t)(ue - us);

        peer_t peer;
        if (llen == 0 || llen > PEERS_LABEL_MAX)
            return fail(out, entry, "label must be 1-" STR(PEERS_LABEL_MAX) " characters");
        memcpy(peer.label, ls, llen);
        peer.label[llen] = '\0';
        if (!peers_label_valid(peer.label))
            return fail(out, entry, "label may only hold letters, digits, space, _ . -");
        if (ulen == 0)
            return fail(out, entry, "URL is empty");
        if (ulen > PEERS_URL_MAX)
            return fail(out, entry, "URL is longer than " STR(PEERS_URL_MAX) " characters");
        memcpy(peer.url, us, ulen);
        peer.url[ulen] = '\0';
        if (!peers_url_valid(peer.url))
            return fail(out, entry,
                        "URL must be http:// or https:// and a host, no blanks, quotes or angle brackets");

        if (known_label(out, peer.label)) continue;         /* duplicate: first one wins */
        if (out->count == PEERS_MAX)
            return fail(out, 0, "At most " STR(PEERS_MAX) " peers");
        total += (out->count ? 1 : 0) + llen + 1 + ulen;
        out->item[out->count++] = peer;
    }
    if (total > PEERS_TEXT_MAX)
        return fail(out, 0, "Peer list is longer than " STR(PEERS_TEXT_MAX) " characters");
    return NULL;
}

int peers_serialise(const peers_t *p, char *buf, size_t len)
{
    size_t n = 0;
    if (len == 0) return -1;
    for (int i = 0; i < p->count; i++) {
        int w = snprintf(buf + n, len - n, "%s%s=%s", i ? ";" : "", p->item[i].label, p->item[i].url);
        if (w < 0 || (size_t)w >= len - n) return -1;
        n += (size_t)w;
    }
    buf[n] = '\0';
    return (int)n;
}
