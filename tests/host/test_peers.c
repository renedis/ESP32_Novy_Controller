/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include "check.h"
#include "peers.h"

static void test_label_rules(void)
{
    const char *ok[] = { "A", "UPS-1", "Floor heating", "a_b.c-d", "0123456789",
                         "abcdefghijklmnopqrstuvwx" /* 24 */ };
    const char *bad[] = { "", "abcdefghijklmnopqrstuvwxy" /* 25 */, "a=b", "a;b", "x<y",
                          "a\"b", " lead", "trail ", "tab\tx", "caf\xc3\xa9", "a/b", "   " };
    for (size_t i = 0; i < sizeof(ok) / sizeof(ok[0]); i++)   CHECK(peers_label_valid(ok[i]));
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) CHECK(!peers_label_valid(bad[i]));
    CHECK(!peers_label_valid(NULL));
}

static void test_url_rules(void)
{
    char longest[PEERS_URL_MAX + 2];
    memset(longest, 'a', sizeof(longest));
    memcpy(longest, "http://", 7);
    longest[PEERS_URL_MAX] = '\0';                 /* exactly 96 characters */

    const char *ok[] = { "http://192.168.1.10/", "https://nas.local", "http://h",
                         "http://192.168.1.20:8080/path?x=1&y=2#top", longest };
    const char *bad[] = { "", "ftp://host/", "HTTP://host/", "javascript:alert(1)", "//host/",
                          "http://", "https://", "http:///path", "http://a b", "http://a\tb",
                          "http://a\"b", "http://a'b", "http://a<b", "http://a>b", "http://a`b",
                          "http://caf\xc3\xa9/", "http://a\x7f" };
    for (size_t i = 0; i < sizeof(ok) / sizeof(ok[0]); i++)   CHECK(peers_url_valid(ok[i]));
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) CHECK(!peers_url_valid(bad[i]));

    longest[PEERS_URL_MAX] = 'a';
    longest[PEERS_URL_MAX + 1] = '\0';             /* 97 characters */
    CHECK(!peers_url_valid(longest));
    CHECK(!peers_url_valid(NULL));
}

static bool has(const peers_t *p, int i, const char *label, const char *url)
{
    return i < p->count && strcmp(p->item[i].label, label) == 0 && strcmp(p->item[i].url, url) == 0;
}

static void test_parse_basic(void)
{
    peers_t p;
    CHECK(peers_parse("Boiler=http://192.168.1.20/;NAS=https://nas.local:8443/ui", &p) == NULL);
    CHECK(p.count == 2);
    CHECK(has(&p, 0, "Boiler", "http://192.168.1.20/"));
    CHECK(has(&p, 1, "NAS", "https://nas.local:8443/ui"));
}

static void test_parse_empty(void)
{
    peers_t p;
    p.count = 5;
    CHECK(peers_parse("", &p) == NULL && p.count == 0);
    p.count = 5;
    CHECK(peers_parse(NULL, &p) == NULL && p.count == 0);
    p.count = 5;
    CHECK(peers_parse(" ;; \n\r\n\t;", &p) == NULL && p.count == 0);
}

static void test_parse_trims_blanks(void)
{
    peers_t p;
    CHECK(peers_parse("  Boiler room = http://192.168.1.20/  ;\tNAS=http://nas.local\t;", &p) == NULL);
    CHECK(p.count == 2);
    CHECK(has(&p, 0, "Boiler room", "http://192.168.1.20/"));
    CHECK(has(&p, 1, "NAS", "http://nas.local"));
}

static void test_parse_newlines(void)
{
    peers_t p;
    CHECK(peers_parse("A=http://a/\nB=http://b/\r\nC=http://c/\n", &p) == NULL);
    CHECK(p.count == 3);
    CHECK(has(&p, 2, "C", "http://c/"));
}

static void test_parse_url_keeps_equals(void)
{
    peers_t p;
    CHECK(peers_parse("Q=http://h/?a=b&c=d", &p) == NULL);
    CHECK(has(&p, 0, "Q", "http://h/?a=b&c=d"));
}

static void test_parse_duplicates_collapse(void)
{
    peers_t p;
    CHECK(peers_parse("UPS=http://a/;ups=http://b/;B=http://c/;UPS=http://a/", &p) == NULL);
    CHECK(p.count == 2);
    CHECK(has(&p, 0, "UPS", "http://a/"));           /* the first one wins */
    CHECK(has(&p, 1, "B", "http://c/"));
}

/* "P01=http://192.168.1.1/;P02=..." with n distinct entries. */
static void make_list(char *buf, size_t len, int n)
{
    size_t used = 0;
    buf[0] = '\0';
    for (int i = 1; i <= n; i++)
        used += snprintf(buf + used, len - used, "%sP%02d=http://192.168.1.%d/", i > 1 ? ";" : "", i, i);
}

static void test_parse_max_entries(void)
{
    char    text[1024];
    peers_t p;
    make_list(text, sizeof(text), PEERS_MAX);
    CHECK(peers_parse(text, &p) == NULL && p.count == PEERS_MAX);

    strcat(text, ";p01=http://192.168.1.99/");      /* a duplicate does not count */
    CHECK(peers_parse(text, &p) == NULL && p.count == PEERS_MAX);

    make_list(text, sizeof(text), PEERS_MAX + 1);
    const char *why = peers_parse(text, &p);
    CHECK(why != NULL && strstr(why, "12") != NULL);
    CHECK(p.count == 0);
}

/* Entry i: a 24-char label, '=', a 96-char URL: 121 characters. */
static size_t long_entry(char *dst, int i)
{
    char label[PEERS_LABEL_MAX + 1], url[PEERS_URL_MAX + 1];
    snprintf(label, sizeof(label), "Peer %02d abcdefghijklmnop", i);
    memset(url, 'a', PEERS_URL_MAX);
    memcpy(url, "http://", 7);
    url[PEERS_URL_MAX] = '\0';
    return (size_t)snprintf(dst, PEERS_LABEL_MAX + PEERS_URL_MAX + 2, "%s=%s", label, url);
}

static void test_parse_max_length(void)
{
    char    text[1024];
    peers_t p;
    size_t  n = 0;
    for (int i = 1; i <= 4; i++) {                  /* 4 x 121 + 3 separators = 487 */
        if (i > 1) text[n++] = ';';
        n += long_entry(text + n, i);
    }
    strcpy(text + n, ";Z=http://");                 /* + 10 = 497 */
    memset(text + n + 10, 'z', 15);                 /* + 15 = 512 */
    text[n + 25] = '\0';
    CHECK(strlen(text) == PEERS_TEXT_MAX);
    CHECK(peers_parse(text, &p) == NULL && p.count == 5);

    strcat(text, "z");                              /* 513 */
    const char *why = peers_parse(text, &p);
    CHECK(why != NULL && strstr(why, "512") != NULL);
    CHECK(p.count == 0);

    /* blanks around entries do not count against the limit */
    char spaced[1200];
    snprintf(spaced, sizeof(spaced), "   %.*s   ", (int)PEERS_TEXT_MAX, text);
    CHECK(peers_parse(spaced, &p) == NULL && p.count == 5);
}

static void test_parse_errors_name_the_entry(void)
{
    peers_t p;
    const char *why;
    why = peers_parse("A=http://a/;no equals sign", &p);
    CHECK(why != NULL && strstr(why, "Peer 2") != NULL && p.count == 0);
    why = peers_parse("=http://a/", &p);
    CHECK(why != NULL && strstr(why, "Peer 1") != NULL && strstr(why, "label") != NULL);
    why = peers_parse("A=http://a/;B=", &p);
    CHECK(why != NULL && strstr(why, "Peer 2") != NULL && strstr(why, "URL") != NULL);
    why = peers_parse("A=ftp://a/", &p);
    CHECK(why != NULL && strstr(why, "http://") != NULL);
    why = peers_parse("Bad|label=http://a/", &p);
    CHECK(why != NULL && strstr(why, "label") != NULL);
    why = peers_parse("A=http://a b/", &p);
    CHECK(why != NULL && strstr(why, "URL") != NULL);
    why = peers_parse("abcdefghijklmnopqrstuvwxy=http://a/", &p);    /* 25 chars */
    CHECK(why != NULL && strstr(why, "24") != NULL);
}

static void test_parse_error_does_not_echo_input(void)
{
    peers_t p;
    const char *why = peers_parse("<script>=http://a/;x=http://\"><img>", &p);
    CHECK(why != NULL && strchr(why, '<') == NULL && strchr(why, '"') == NULL);
    /* error replies are sent as text/html: no markup characters, even for a bad URL */
    why = peers_parse("A=http://a/<script>", &p);
    CHECK(why != NULL && strpbrk(why, "<>&\"'") == NULL);
    why = peers_parse("A=http://a/\"x", &p);
    CHECK(why != NULL && strpbrk(why, "<>&\"'") == NULL);
}

static void test_serialise(void)
{
    peers_t p;
    char    buf[PEERS_TEXT_MAX + 1];
    CHECK(peers_parse(" B = http://b/ \n\nA=https://a:1/x?y=z\n", &p) == NULL);
    CHECK(peers_serialise(&p, buf, sizeof(buf)) == (int)strlen("B=http://b/;A=https://a:1/x?y=z"));
    CHECK(strcmp(buf, "B=http://b/;A=https://a:1/x?y=z") == 0);

    p.count = 0;
    CHECK(peers_serialise(&p, buf, sizeof(buf)) == 0 && buf[0] == '\0');
}

static void test_serialise_buffer_limits(void)
{
    peers_t p;
    char    buf[16];
    CHECK(peers_parse("AB=http://cd/", &p) == NULL);     /* 13 characters */
    CHECK(peers_serialise(&p, buf, 14) == 13 && strcmp(buf, "AB=http://cd/") == 0);
    CHECK(peers_serialise(&p, buf, 13) == -1);            /* no room for the NUL */
    CHECK(peers_serialise(&p, buf, 0) == -1);
}

static void test_round_trip_is_canonical(void)
{
    peers_t a, b;
    char    t1[PEERS_TEXT_MAX + 1], t2[PEERS_TEXT_MAX + 1];
    CHECK(peers_parse("  X = http://x/ ;;y=http://y/\r\nx=http://dup/", &a) == NULL);
    CHECK(peers_serialise(&a, t1, sizeof(t1)) > 0);
    CHECK(peers_parse(t1, &b) == NULL);
    CHECK(peers_serialise(&b, t2, sizeof(t2)) > 0);
    CHECK(strcmp(t1, "X=http://x/;y=http://y/") == 0);
    CHECK(strcmp(t1, t2) == 0);
}

static void test_longest_list_fits_text_max(void)
{
    char    text[1024];
    peers_t p;
    char    buf[PEERS_TEXT_MAX + 1];
    size_t  n = 0;
    for (int i = 1; i <= 4; i++) {
        if (i > 1) text[n++] = ';';
        n += long_entry(text + n, i);
    }
    strcpy(text + n, ";Z=http://zzzzzzzzzzzzzzz");  /* 512 characters in all */
    CHECK(peers_parse(text, &p) == NULL);
    CHECK(peers_serialise(&p, buf, sizeof(buf)) == PEERS_TEXT_MAX);
    CHECK(strcmp(buf, text) == 0);
}

int main(void)
{
    test_label_rules();
    test_url_rules();
    test_parse_basic();
    test_parse_empty();
    test_parse_trims_blanks();
    test_parse_newlines();
    test_parse_url_keeps_equals();
    test_parse_duplicates_collapse();
    test_parse_max_entries();
    test_parse_max_length();
    test_parse_errors_name_the_entry();
    test_parse_error_does_not_echo_input();
    test_serialise();
    test_serialise_buffer_limits();
    test_round_trip_is_canonical();
    test_longest_list_fits_text_max();
    CHECK_DONE();
}
