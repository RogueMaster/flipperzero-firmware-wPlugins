// Two simulated Pods talking to each other over a deliberately hostile channel.
//
// Pod needs two Flippers to test for real. This exercises the next best thing:
// the exact wire code the device runs, with one peer's encoder feeding the
// other's parser, through a channel that repeats, fragments, corrupts and
// interleaves frames the way a half-duplex radio does.
//
// Build and run:  cc -Wall -Wextra -I.. -o test_wire test_wire.c ../pod_wire.c && ./test_wire

#include "../pod_wire.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;

static void chk(const char* name, int cond) {
    printf("%-52s %s\n", name, cond ? "PASS" : "FAIL");
    if(!cond) failures++;
}

// --- a peer -------------------------------------------------------------

#define INBOX_MAX 32

typedef struct {
    const char* who;
    uint32_t id;
    char name[POD_NAME_MAX + 1];
    uint8_t level;
    PodWire wire;
    PodMsg inbox[INBOX_MAX];
    int received;
} Peer;

static void peer_rx(void* context, const PodMsg* msg) {
    Peer* p = context;
    if(p->received < INBOX_MAX) p->inbox[p->received] = *msg;
    p->received++;
}

static void peer_init(Peer* p, const char* who, uint32_t id, const char* name, uint8_t level) {
    memset(p, 0, sizeof(*p));
    p->who = who;
    p->id = id;
    snprintf(p->name, sizeof(p->name), "%s", name);
    p->level = level;
    pod_wire_reset(&p->wire);
}

// Deliver bytes to a peer as the radio would: in arbitrary chunks.
static void deliver(Peer* to, const uint8_t* buf, size_t len, size_t chunk) {
    for(size_t off = 0; off < len; off += chunk) {
        size_t n = (len - off < chunk) ? len - off : chunk;
        pod_wire_feed(&to->wire, buf + off, n, peer_rx, to);
    }
}

// Every frame goes out more than once on real hardware.
static void deliver_repeated(Peer* to, const uint8_t* buf, size_t len, int times, size_t chunk) {
    uint8_t stream[512];
    size_t total = 0;
    for(int i = 0; i < times && total + len <= sizeof(stream); i++) {
        memcpy(stream + total, buf, len);
        total += len;
    }
    deliver(to, stream, total, chunk);
}

int main(void) {
    Peer a, b;
    uint8_t buf[POD_FRAME_MAX];
    size_t n;

    // --- round trips ----------------------------------------------------
    peer_init(&a, "A", 0xEC805897, "POD-5897", 7);
    peer_init(&b, "B", 0x1234ABCD, "DOLPHIN", 3);

    n = pod_wire_beacon(buf, sizeof(buf), a.id, 2, a.level, 0xBEEF, a.name);
    deliver(&b, buf, n, 64);
    chk("beacon arrives", b.received == 1);
    chk("beacon src", b.inbox[0].src_id == a.id);
    chk("beacon name", !strcmp(b.inbox[0].name, "POD-5897"));
    chk("beacon level", b.inbox[0].level == 7);
    chk("beacon icon", b.inbox[0].icon == 2);
    chk("beacon nonce", b.inbox[0].nonce == 0xBEEF);

    b.received = 0;
    n = pod_wire_named(buf, sizeof(buf), PodMsgChallenge, a.id, a.name, a.level, b.id, 0x5A5A);
    deliver(&b, buf, n, 64);
    chk("challenge arrives", b.received == 1);
    chk("challenge addressed to B", b.inbox[0].dst_id == b.id);
    chk("challenge battle id", b.inbox[0].battle_id == 0x5A5A);
    chk("challenge carries name", !strcmp(b.inbox[0].name, "POD-5897"));

    a.received = 0;
    n = pod_wire_taps(buf, sizeof(buf), b.id, a.id, 0x5A5A, 41);
    deliver(&a, buf, n, 64);
    chk("taps arrives", a.received == 1);
    chk("tap count", a.inbox[0].taps == 41);

    a.received = 0;
    n = pod_wire_decline(buf, sizeof(buf), b.id, a.id, 0x5A5A);
    deliver(&a, buf, n, 64);
    chk("decline arrives", a.received == 1 && a.inbox[0].type == PodMsgDecline);

    // --- the full handshake, repeated and fragmented ---------------------
    peer_init(&a, "A", 0xAAAA1111, "ALPHA", 5);
    peer_init(&b, "B", 0xBBBB2222, "BRAVO", 9);
    const uint16_t bid = 0x7F3C;

    n = pod_wire_named(buf, sizeof(buf), PodMsgChallenge, a.id, a.name, a.level, b.id, bid);
    deliver_repeated(&b, buf, n, 3, 5); // sent 3x, arriving 5 bytes at a time
    chk("handshake: challenge delivered once despite 3 sends", b.received == 1);

    n = pod_wire_named(buf, sizeof(buf), PodMsgAccept, b.id, b.name, b.level, a.id, bid);
    deliver_repeated(&a, buf, n, 3, 7);
    chk("handshake: accept delivered once", a.received == 1);
    chk("handshake: accept matches battle", a.inbox[0].battle_id == bid);
    chk("handshake: accept from B", a.inbox[0].src_id == b.id);
    chk("handshake: opponent level known", a.inbox[0].level == 9);

    int before = b.received;
    n = pod_wire_taps(buf, sizeof(buf), a.id, b.id, bid, 55);
    deliver_repeated(&b, buf, n, 3, 3);
    chk("handshake: A's taps delivered once", b.received == before + 1);
    chk("handshake: tap count intact", b.inbox[b.received - 1].taps == 55);

    before = a.received;
    n = pod_wire_taps(buf, sizeof(buf), b.id, a.id, bid, 61);
    deliver_repeated(&a, buf, n, 3, 11);
    chk("handshake: B's taps delivered once", a.received == before + 1);
    chk("handshake: both tap counts known", a.inbox[a.received - 1].taps == 61);

    // A different battle with the same peers must not be swallowed as a dup.
    before = b.received;
    n = pod_wire_named(buf, sizeof(buf), PodMsgChallenge, a.id, a.name, a.level, b.id, bid + 1);
    deliver(&b, buf, n, 64);
    chk("new battle id is not deduped", b.received == before + 1);

    // --- hostile channel -------------------------------------------------
    peer_init(&b, "B", 0xBBBB2222, "BRAVO", 9);

    uint8_t noise[600];
    for(size_t i = 0; i < sizeof(noise); i++)
        noise[i] = (uint8_t)(i * 37 + 11);
    deliver(&b, noise, sizeof(noise), 29);
    chk("pure noise yields nothing", b.received == 0);

    n = pod_wire_beacon(buf, sizeof(buf), a.id, 1, 4, 0x0101, "ALPHA");
    uint8_t mixed[256];
    size_t m = 0;
    memcpy(mixed + m, "PDPD\x01garbage", 12);
    m += 12; // false preambles
    memcpy(mixed + m, buf, n);
    m += n;
    memcpy(mixed + m, "\xff\xff\xff", 3);
    m += 3;
    deliver(&b, mixed, m, 4);
    chk("frame found amid garbage", b.received == 1 && b.inbox[0].src_id == a.id);

    peer_init(&b, "B", 0xBBBB2222, "BRAVO", 9);
    n = pod_wire_beacon(buf, sizeof(buf), 0xDEAD0001, 1, 4, 0x2222, "CORRUPT");
    buf[n - 1] ^= 0xFF; // break the CRC
    deliver(&b, buf, n, 64);
    chk("bad CRC rejected", b.received == 0);

    peer_init(&b, "B", 0xBBBB2222, "BRAVO", 9);
    n = pod_wire_beacon(buf, sizeof(buf), 0xDEAD0002, 1, 4, 0x3333, "SPLIT");
    deliver(&b, buf, n - 2, 64); // withhold the tail
    chk("truncated frame not delivered early", b.received == 0);
    deliver(&b, buf + n - 2, 2, 64); // then complete it
    chk("frame completes when the rest arrives", b.received == 1);

    // --- edges -----------------------------------------------------------
    peer_init(&b, "B", 0xBBBB2222, "BRAVO", 9);
    n = pod_wire_beacon(buf, sizeof(buf), 0x11112222, 0, 1, 0x4444, "ABCDEFGHIJKL"); // 12 = max
    deliver(&b, buf, n, 64);
    chk("max-length name round trips",
        b.received == 1 && !strcmp(b.inbox[0].name, "ABCDEFGHIJKL"));

    b.received = 0;
    n = pod_wire_beacon(buf, sizeof(buf), 0x33334444, 0, 1, 0x5555, "");
    deliver(&b, buf, n, 64);
    chk("empty name round trips", b.received == 1 && b.inbox[0].name[0] == '\0');

    b.received = 0;
    n = pod_wire_beacon(buf, sizeof(buf), 0x55556666, 0, 1, 0x6666, "WAYTOOLONGNAMEHERE");
    deliver(&b, buf, n, 64);
    chk("over-long name is truncated, not overflowed",
        b.received == 1 && strlen(b.inbox[0].name) == POD_NAME_MAX);

    chk("encoder refuses a buffer that is too small",
        pod_wire_beacon(buf, 4, 1, 0, 0, 0, "X") == 0);

    // Dedup history is finite; older entries must age out rather than block.
    peer_init(&b, "B", 0xBBBB2222, "BRAVO", 9);
    for(int i = 0; i < POD_DEDUP_N + 8; i++) {
        n = pod_wire_beacon(buf, sizeof(buf), 0x90000000u + i, 0, 1, (uint16_t)i, "N");
        deliver(&b, buf, n, 64);
    }
    chk("distinct messages are never deduped", b.received == POD_DEDUP_N + 8);

    // A burst larger than the reassembly buffer must not corrupt state.
    peer_init(&b, "B", 0xBBBB2222, "BRAVO", 9);
    uint8_t flood[POD_PARSE_BUF * 3];
    memset(flood, 0x5A, sizeof(flood));
    n = pod_wire_beacon(buf, sizeof(buf), 0x77778888, 0, 1, 0x7777, "AFTER");
    memcpy(flood + sizeof(flood) - n, buf, n); // a good frame at the very end
    deliver(&b, flood, sizeof(flood), 200);
    chk("oversized burst keeps the newest frame",
        b.received == 1 && !strcmp(b.inbox[0].name, "AFTER"));

    printf("\n%s\n", failures ? "FAILURES" : "all passed");
    return failures != 0;
}
