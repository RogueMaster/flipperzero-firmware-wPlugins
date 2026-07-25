/* Host test for the session-summary arithmetic.
 *
 * Throughput, error rate and the human-readable formatters all do integer
 * scaling that is easy to get subtly wrong (the ms->s rounding, the KB split
 * without floating point). This pins them against the real sessionstats.c.
 *
 *   make -C test
 */

#include "helpers/sessionstats.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

static void check_u32(const char* name, uint32_t got, uint32_t want) {
    if(got != want) failures++;
    printf(
        "  %-40s %s (%lu, want %lu)\n",
        name,
        (got == want) ? "ok" : "*** FAIL ***",
        (unsigned long)got,
        (unsigned long)want);
}

static void check_str(const char* name, const char* got, const char* want) {
    const bool ok = strcmp(got, want) == 0;
    if(!ok) failures++;
    printf("  %-40s %s (\"%s\", want \"%s\")\n", name, ok ? "ok" : "*** FAIL ***", got, want);
}

int main(void) {
    char buf[32];

    printf("\nthroughput (bytes/sec, rounded)\n");
    /* 11520 bytes in 1.000 s = 11520 bps exactly. */
    check_u32("11520 B / 1000 ms", session_stats_bps(&(SessionStats){.rx_bytes = 11520, .duration_ms = 1000}), 11520);
    /* 100 B in 3000 ms = 33.3 -> 33. */
    check_u32("100 B / 3000 ms", session_stats_bps(&(SessionStats){.rx_bytes = 100, .duration_ms = 3000}), 33);
    /* 2 B in 3000 ms = 0.667 -> 1 (round to nearest, not floor). */
    check_u32("2 B / 3000 ms rounds up", session_stats_bps(&(SessionStats){.rx_bytes = 2, .duration_ms = 3000}), 1);
    /* Guard: never divide by zero. */
    check_u32("0 ms duration is safe", session_stats_bps(&(SessionStats){.rx_bytes = 500, .duration_ms = 0}), 0);
    /* A fast link long enough to overflow a 32-bit intermediate if not careful. */
    check_u32("fast link no overflow", session_stats_bps(&(SessionStats){.rx_bytes = 90000000, .duration_ms = 1000}), 90000000);

    printf("\nerror rate (permille of bytes+errors)\n");
    check_u32("no errors", session_stats_error_permille(&(SessionStats){.rx_bytes = 1000, .errors = 0}), 0);
    check_u32("all errors", session_stats_error_permille(&(SessionStats){.rx_bytes = 0, .errors = 10}), 1000);
    check_u32("half and half", session_stats_error_permille(&(SessionStats){.rx_bytes = 50, .errors = 50}), 500);
    check_u32("nothing at all is safe", session_stats_error_permille(&(SessionStats){0}), 0);

    printf("\nduration formatting\n");
    session_stats_format_duration(&(SessionStats){.duration_ms = 45000}, buf, sizeof(buf));
    check_str("45 s", buf, "45s");
    session_stats_format_duration(&(SessionStats){.duration_ms = 83000}, buf, sizeof(buf));
    check_str("83 s -> 1m 23s", buf, "1m 23s");
    session_stats_format_duration(&(SessionStats){.duration_ms = 600}, buf, sizeof(buf));
    check_str("under a second", buf, "0s");

    printf("\nbyte formatting\n");
    session_stats_format_bytes(834, buf, sizeof(buf));
    check_str("834 B", buf, "834 B");
    session_stats_format_bytes(1024, buf, sizeof(buf));
    check_str("exactly 1 KB", buf, "1.0 KB");
    session_stats_format_bytes(1536, buf, sizeof(buf));
    check_str("1.5 KB", buf, "1.5 KB");
    session_stats_format_bytes(1023, buf, sizeof(buf));
    check_str("still bytes at 1023", buf, "1023 B");

    printf("\nverdict\n");
    check_str("silent", session_stats_verdict(&(SessionStats){0}), "no traffic");
    check_str("clean", session_stats_verdict(&(SessionStats){.rx_bytes = 1000, .errors = 0}), "clean link");
    check_str("1 in 1000 is minor", session_stats_verdict(&(SessionStats){.rx_bytes = 999, .errors = 1}), "minor errors");
    check_str("garbage is wrong framing", session_stats_verdict(&(SessionStats){.rx_bytes = 100, .errors = 100}), "wrong framing?");

    printf(
        "\n%s (%d failure%s)\n\n",
        failures ? "FAILED" : "ALL PASS",
        failures,
        failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
