/* Host test for the trigger matcher.
 *
 * The matcher has to survive two things that are easy to get wrong: a pattern
 * that overlaps itself, and a stream that arrives split at arbitrary points.
 * Both are exercised here against the real trigger.c.
 *
 *   make -C test
 */

#include "helpers/trigger.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

static void check(const char* name, bool got, bool want) {
    if(got != want) failures++;
    printf("  %-42s %s\n", name, (got == want) ? "ok" : "*** FAIL ***");
}

static void check_hits(const char* name, uint32_t got, uint32_t want) {
    if(got != want) failures++;
    printf(
        "  %-42s %s (%lu, want %lu)\n",
        name,
        (got == want) ? "ok" : "*** FAIL ***",
        (unsigned long)got,
        (unsigned long)want);
}

/** Feed a whole string in one go. */
static bool feed_str(Trigger* tr, const char* s) {
    return trigger_feed(tr, (const uint8_t*)s, strlen(s));
}

/** Feed a string one byte at a time - the worst-case chunking. */
static bool feed_bytewise(Trigger* tr, const char* s) {
    bool fired = false;
    for(const char* p = s; *p; p++) {
        if(trigger_feed(tr, (const uint8_t*)p, 1)) fired = true;
    }
    return fired;
}

int main(void) {
    Trigger* tr = trigger_alloc();

    printf("\nbasic matching\n");
    trigger_set(tr, "login:");
    check("finds pattern in a line", feed_str(tr, "debian login: "), true);

    trigger_set(tr, "login:");
    check("ignores a line without it", feed_str(tr, "starting kernel..."), false);

    trigger_set(tr, "login:");
    check("case-insensitive", feed_str(tr, "Debian LOGIN: "), true);

    printf("\nsplit across chunks (the DMA case)\n");
    trigger_set(tr, "login:");
    trigger_feed(tr, (const uint8_t*)"debian log", 10);
    check("matches when split mid-pattern", feed_str(tr, "in: root"), true);

    trigger_set(tr, "root@openwrt");
    check("matches fed one byte at a time", feed_bytewise(tr, "xx root@openwrt:/#"), true);

    printf("\nself-overlapping patterns\n");
    trigger_set(tr, "aab");
    check("'aab' found in 'aaab'", feed_str(tr, "aaab"), true);

    trigger_set(tr, "aaa");
    trigger_reset(tr);
    feed_str(tr, "aaaa");
    check_hits("'aaa' in 'aaaa' counts overlaps", trigger_hits(tr), 2);

    trigger_set(tr, "abab");
    trigger_reset(tr);
    feed_str(tr, "abababab");
    check_hits("'abab' in 'abababab'", trigger_hits(tr), 3);

    printf("\ncounting\n");
    trigger_set(tr, "err");
    trigger_reset(tr);
    feed_str(tr, "err once, err twice, ERR thrice");
    check_hits("counts every occurrence", trigger_hits(tr), 3);

    trigger_set(tr, "x");
    trigger_reset(tr);
    feed_bytewise(tr, "axbxcx");
    check_hits("single-char pattern, bytewise", trigger_hits(tr), 3);

    printf("\ndisarmed\n");
    trigger_set(tr, "");
    check("empty pattern never fires", feed_str(tr, "anything at all"), false);
    check("empty pattern is not armed", trigger_is_armed(tr), false);

    trigger_set(tr, "up");
    check("non-empty pattern is armed", trigger_is_armed(tr), true);

    trigger_free(tr);

    printf(
        "\n%s (%d failure%s)\n\n",
        failures ? "FAILED" : "ALL PASS",
        failures,
        failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
