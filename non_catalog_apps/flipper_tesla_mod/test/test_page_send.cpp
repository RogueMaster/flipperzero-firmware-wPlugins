/*
 * test_page_send.cpp — host unit tests for esp32/.firmware/page_send.h: a slow
 * client that keeps taking data gets the whole dashboard page, one that stops
 * is dropped 400 ms later, and no page holds the loop task past 3 s.
 *
 * A fake socket stands in for lwIP: a 5760-byte send buffer (TCP_SND_BUF in
 * the pinned Arduino core) that the client empties one round trip after a
 * wait (or only part of it, for a trickle), or never once it has gone.
 *
 * Build + run:  make -C test check
 */

#include <stdio.h>
#include <string.h>

#include "page_send.h"

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, ...)                                  \
    do {                                                  \
        if(cond) {                                        \
            g_pass++;                                     \
        } else {                                          \
            g_fail++;                                     \
            printf("  FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                          \
            printf("\n");                                 \
        }                                                 \
    } while(0)

static const size_t SND_BUF = 5760;
static const uint32_t STALL_MS = 400, MAX_MS = 3000; // web_dashboard.cpp's limits
static const size_t GZ_PAGE = 48439, RAW_PAGE = 163617;

struct FakeSocket {
    uint32_t clock = 1000; // ms; not 0, so a wrap would show
    size_t queued = 0; // bytes in the send buffer
    size_t received = 0; // bytes the client has read
    uint32_t rtt_ms = 20; // a wait frees the buffer after this long
    size_t per_rtt = (size_t)-1; // most the client reads per round trip
    size_t reads_until = (size_t)-1; // the client stops reading past this
    size_t fail_after = (size_t)-1; // send() fails once this much went out
    bool wait_fails = false;
    unsigned sends = 0, waits = 0;
    uint32_t longest_wait = 0;
    uint32_t last_take = 0; // clock of the last send() that took bytes
    size_t sent = 0;

    long send(const uint8_t*, size_t n) {
        sends++;
        if(sent >= fail_after) return -1;
        size_t room = SND_BUF - queued;
        size_t take = n < room ? n : room;
        queued += take;
        sent += take;
        if(take) last_take = clock;
        return (long)take;
    }
    bool wait(uint32_t ms) {
        waits++;
        if(ms > longest_wait) longest_wait = ms;
        if(wait_fails) return false;
        if(received < reads_until && queued > 0 && ms >= rtt_ms) {
            clock += rtt_ms;
            size_t got = queued;
            if(got > per_rtt) got = per_rtt;
            if(got > reads_until - received) got = reads_until - received;
            received += got;
            queued -= got;
        } else {
            clock += ms; // nothing frees up: the whole wait passes
        }
        return true;
    }
};

static size_t run(FakeSocket& s, size_t len) {
    static uint8_t page[200000];
    return page_send(
        page,
        len,
        STALL_MS,
        MAX_MS,
        [&](const uint8_t* p, size_t n) { return s.send(p, n); },
        [&](uint32_t ms) { return s.wait(ms); },
        [&]() { return s.clock; });
}

static void test_healthy(void) {
    // The gzip page to a client 20 ms away: whole page, 8 round trips.
    FakeSocket s;
    uint32_t t0 = s.clock;
    size_t sent = run(s, GZ_PAGE);
    CHECK(sent == GZ_PAGE, "healthy: all sent (%zu)", sent);
    CHECK(s.clock - t0 <= 8 * 20, "healthy: %u ms", (unsigned)(s.clock - t0));
}

static void test_slow_client(void) {
    // In-car Wi-Fi, phone in power save: 150 ms round trips. Still progressing,
    // so the whole gzip page goes out (~1.2 s), past what a 1 s cap allowed.
    FakeSocket s;
    s.rtt_ms = 150;
    uint32_t t0 = s.clock;
    size_t sent = run(s, GZ_PAGE);
    CHECK(
        sent == GZ_PAGE && s.clock - t0 > 1000 && s.clock - t0 <= 8 * 150,
        "slow 150 ms: %zu in %u ms",
        sent,
        (unsigned)(s.clock - t0));
    // 390 ms round trips, just under the stall limit: never cut for stalling.
    FakeSocket v;
    v.rtt_ms = 390;
    t0 = v.clock;
    sent = run(v, 3 * SND_BUF);
    CHECK(sent == 3 * SND_BUF, "slow 390 ms: %zu in %u ms", sent, (unsigned)(v.clock - t0));
}

static void test_client_gone(void) {
    // Gone before the first round trip: cut 400 ms after the buffer filled.
    FakeSocket z;
    z.reads_until = 0;
    uint32_t t0 = z.clock;
    size_t sent = run(z, GZ_PAGE);
    CHECK(
        sent == SND_BUF && z.clock - t0 == STALL_MS,
        "gone at once: %zu, %u ms",
        sent,
        (unsigned)(z.clock - t0));
    // Gone after 10 KB: cut 400 ms after the last byte went out.
    FakeSocket s;
    s.reads_until = 10000;
    sent = run(s, GZ_PAGE);
    CHECK(
        sent < GZ_PAGE && s.clock - s.last_take == STALL_MS,
        "gone mid-page: %zu, cut %u ms after the last byte",
        sent,
        (unsigned)(s.clock - s.last_take));
    CHECK(
        s.longest_wait <= STALL_MS,
        "gone: no wait past the stall limit (%u)",
        (unsigned)s.longest_wait);
}

static void test_cap(void) {
    // The plain page at 150 ms round trips needs ~28 of them: cut at the cap.
    FakeSocket s;
    s.rtt_ms = 150;
    uint32_t t0 = s.clock;
    size_t sent = run(s, RAW_PAGE);
    CHECK(
        sent < RAW_PAGE && s.clock - t0 == MAX_MS,
        "cap: %zu of %zu in %u ms",
        sent,
        RAW_PAGE,
        (unsigned)(s.clock - t0));
    // A trickle (100 bytes per 200 ms) never stalls, so only the cap ends it.
    FakeSocket t;
    t.rtt_ms = 200;
    t.per_rtt = 100;
    t0 = t.clock;
    sent = run(t, GZ_PAGE);
    CHECK(
        sent < GZ_PAGE && t.clock - t0 == MAX_MS && t.longest_wait <= STALL_MS,
        "trickle: %zu in %u ms",
        sent,
        (unsigned)(t.clock - t0));
}

static void test_errors(void) {
    // Connection reset mid-page: stop at once, no waiting.
    FakeSocket s;
    s.fail_after = 3000;
    uint32_t t0 = s.clock;
    size_t sent = run(s, GZ_PAGE);
    CHECK(
        sent == SND_BUF && s.clock - t0 <= 20, "reset: %zu, %u ms", sent, (unsigned)(s.clock - t0));
    // select() fails: stop at once.
    FakeSocket w;
    w.wait_fails = true;
    t0 = w.clock;
    sent = run(w, GZ_PAGE);
    CHECK(
        sent == SND_BUF && w.waits == 1 && w.clock == t0,
        "select fails: %zu, %u waits",
        sent,
        w.waits);
    // Nothing to send.
    FakeSocket e;
    CHECK(run(e, 0) == 0 && e.sends == 0, "empty body: no send");
}

static void test_clock_wrap(void) {
    // millis() wraps during the send: both limits still hold.
    FakeSocket s;
    s.clock = 0xFFFFFF00u;
    s.reads_until = 1000;
    size_t sent = run(s, GZ_PAGE);
    CHECK(
        sent < GZ_PAGE && s.clock - s.last_take == STALL_MS,
        "wrap: stall limit holds (%u)",
        (unsigned)s.clock);
    FakeSocket c;
    c.clock = 0xFFFFF000u;
    c.rtt_ms = 150;
    uint32_t t0 = c.clock;
    sent = run(c, RAW_PAGE);
    CHECK(
        sent < RAW_PAGE && (uint32_t)(c.clock - t0) == MAX_MS,
        "wrap: cap holds (%u)",
        (unsigned)(c.clock - t0));
}

int main() {
    printf("test_page_send: dashboard page stall limit and cap\n");
    test_healthy();
    test_slow_client();
    test_client_gone();
    test_cap();
    test_errors();
    test_clock_wrap();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
