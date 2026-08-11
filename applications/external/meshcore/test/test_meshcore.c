/*
 * Host tests for the protocol layer.
 *
 * These exist because the Flipper cannot tell us whether we understood the
 * MeshCore wire format — only a node can, and one is not always on the bench.
 * Everything here runs on a PC in milliseconds and covers the two things that
 * are expensive to get wrong: the frame layout, and the request/reply logic in
 * meshcore_link.c.
 *
 * The vendored library is treated as the thing under test too: if upstream
 * ever changes a field offset, these fail loudly instead of silently
 * misreading a node.
 */
#include <furi.h>

#include "../config/meshcore_apply.h"
#include "../config/meshcore_json.h"
#include "../config/meshcore_preset.h"
#include "../logger/meshcore_events.h"
#include "../logger/meshcore_ping.h"
#include "../logger/meshcore_rxlog.h"
#include "../logger/meshcore_telemetry.h"
#include "../messenger/meshcore_contacts.h"
#include "../messenger/meshcore_share_uri.h"
#include "../messenger/meshcore_messages.h"
#include "../protocol/meshcore_link.h"
#include "../protocol/meshcore_route.h"
#include "fakes.h"

/* ======================================================================== *
 *  Tiny test harness
 * ======================================================================== */
static int g_checks;
static int g_failures;
static const char* g_section = "";

static void section(const char* name) {
    g_section = name;
    printf("\n-- %s\n", name);
}

#define CHECK(cond, ...)                               \
    do {                                               \
        g_checks++;                                    \
        if(!(cond)) {                                  \
            g_failures++;                              \
            printf("   FAIL [%s] line %d: ", g_section, __LINE__); \
            printf(__VA_ARGS__);                       \
            printf("\n");                              \
        }                                              \
    } while(0)

#define CHECK_EQ_U32(got, want, label)                                        \
    CHECK((uint32_t)(got) == (uint32_t)(want), "%s: got %lu, want %lu", label, \
          (unsigned long)(got), (unsigned long)(want))

#define CHECK_EQ_STR(got, want, label) \
    CHECK(strcmp((got), (want)) == 0, "%s: got \"%s\", want \"%s\"", label, (got), (want))

static void put_u32_le(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/* ======================================================================== *
 *  Framing
 * ======================================================================== */
static void test_frame_lead_byte(void) {
    section("framing: lead byte and length");

    const uint8_t payload[] = {MC_CMD_APP_START, 0x03};
    uint8_t frame[16];
    size_t n = mc_frame_encode(payload, sizeof(payload), frame, sizeof(frame));

    CHECK_EQ_U32(n, sizeof(payload) + 3, "framed length");

    /* The whole task hinges on this byte. The firmware's writeFrame() emits
     * '>' and checkRecvFrame() waits for '<', both from the node's side, so
     * what we transmit must be led by '<' (0x3C). If this ever flips, nothing
     * works and the failure looks like a dead node. */
    CHECK(frame[0] == 0x3C, "app->radio lead byte: got 0x%02X, want 0x3C ('<')", frame[0]);
    CHECK(frame[0] == MC_FRAME_APP_TO_RADIO, "lead byte should equal MC_FRAME_APP_TO_RADIO");
    CHECK(MC_FRAME_RADIO_TO_APP == 0x3E, "radio->app lead byte should be 0x3E ('>')");

    /* Length is little-endian, and excludes the 3-byte header. */
    CHECK_EQ_U32(frame[1] | (frame[2] << 8), sizeof(payload), "length field");
    CHECK(memcmp(frame + 3, payload, sizeof(payload)) == 0, "payload copied verbatim");
}

static void test_frame_roundtrip(void) {
    section("framing: encode -> feed -> poll round trip");

    const uint8_t payload[] = {MC_RESP_SELF_INFO, 1, 2, 3, 4, 5};
    uint8_t frame[32];
    size_t framed = mc_frame_encode(payload, sizeof(payload), frame, sizeof(frame));

    mc_rx_t rx;
    mc_rx_init(&rx);
    CHECK_EQ_U32(mc_rx_feed(&rx, frame, framed), framed, "bytes accepted");

    uint8_t out[MC_MAX_PAYLOAD];
    size_t out_len = 0;
    CHECK(mc_rx_poll(&rx, out, sizeof(out), &out_len) == 1, "one frame should be ready");
    CHECK_EQ_U32(out_len, sizeof(payload), "payload length");
    CHECK(memcmp(out, payload, sizeof(payload)) == 0, "payload survives the round trip");
    CHECK(mc_rx_poll(&rx, out, sizeof(out), &out_len) == 0, "no second frame");
}

static void test_frame_fragmented(void) {
    section("framing: frame split across reads");

    const uint8_t payload[] = {MC_RESP_OK, 0xAA, 0xBB, 0xCC};
    uint8_t frame[32];
    size_t framed = mc_frame_encode(payload, sizeof(payload), frame, sizeof(frame));

    mc_rx_t rx;
    mc_rx_init(&rx);
    uint8_t out[MC_MAX_PAYLOAD];
    size_t out_len = 0;

    /* One byte at a time: a real UART splits frames wherever it likes. */
    for(size_t i = 0; i + 1 < framed; i++) {
        mc_rx_feed(&rx, frame + i, 1);
        CHECK(mc_rx_poll(&rx, out, sizeof(out), &out_len) == 0, "incomplete frame must not parse");
    }
    mc_rx_feed(&rx, frame + framed - 1, 1);
    CHECK(mc_rx_poll(&rx, out, sizeof(out), &out_len) == 1, "last byte completes the frame");
    CHECK(memcmp(out, payload, sizeof(payload)) == 0, "reassembled payload matches");
}

static void test_frame_two_in_one_read(void) {
    section("framing: two frames in one read");

    const uint8_t a[] = {MC_RESP_OK};
    const uint8_t b[] = {MC_RESP_ERR, 0x07};
    uint8_t buf[32];
    size_t n = mc_frame_encode(a, sizeof(a), buf, sizeof(buf));
    n += mc_frame_encode(b, sizeof(b), buf + n, sizeof(buf) - n);

    mc_rx_t rx;
    mc_rx_init(&rx);
    mc_rx_feed(&rx, buf, n);

    uint8_t out[MC_MAX_PAYLOAD];
    size_t out_len = 0;
    CHECK(mc_rx_poll(&rx, out, sizeof(out), &out_len) == 1, "first frame");
    CHECK(out[0] == MC_RESP_OK, "first frame is OK");
    CHECK(mc_rx_poll(&rx, out, sizeof(out), &out_len) == 1, "second frame");
    CHECK(out[0] == MC_RESP_ERR, "second frame is ERR");
    CHECK(mc_rx_poll(&rx, out, sizeof(out), &out_len) == 0, "and nothing more");
}

static void test_frame_resync_after_garbage(void) {
    section("framing: resync past line noise");

    const uint8_t payload[] = {MC_RESP_OK};
    uint8_t buf[32];
    /* Noise first — what a wrong baud rate or a half-connected wire produces. */
    buf[0] = 0xAA;
    buf[1] = 0x00;
    buf[2] = 0xFF;
    size_t n = 3 + mc_frame_encode(payload, sizeof(payload), buf + 3, sizeof(buf) - 3);

    mc_rx_t rx;
    mc_rx_init(&rx);
    mc_rx_feed(&rx, buf, n);

    uint8_t out[MC_MAX_PAYLOAD];
    size_t out_len = 0;
    CHECK(mc_rx_poll(&rx, out, sizeof(out), &out_len) == 1, "valid frame found after garbage");
    CHECK(out[0] == MC_RESP_OK, "and it decodes correctly");
}

/* ======================================================================== *
 *  Command builders
 * ======================================================================== */
static void test_cmd_app_start(void) {
    section("commands: APP_START");

    uint8_t out[64];
    size_t n = mc_cmd_app_start(out, sizeof(out), "MeshCoreCfg");

    CHECK_EQ_U32(n, 1 + 1 + 6 + 11, "payload length");
    CHECK_EQ_U32(out[0], MC_CMD_APP_START, "command code");
    CHECK_EQ_U32(out[1], 3, "app protocol version");
    CHECK_EQ_U32(out[1], MESHCORE_LINK_PROTO_VER, "app version matches what the link advertises");
    for(int i = 2; i < 8; i++) CHECK_EQ_U32(out[i], 0, "reserved byte");
    CHECK(memcmp(out + 8, "MeshCoreCfg", 11) == 0, "app name");
}

static void test_cmd_set_radio_params(void) {
    section("commands: SET_RADIO_PARAMS");

    uint8_t out[32];
    /* 869.525 MHz is expressed in kHz on the wire — the header calls the
     * argument freq_hz_x1000, which is the same thing said awkwardly. */
    size_t n = mc_cmd_set_radio_params(out, sizeof(out), 869525, 250, 10, 5);

    CHECK_EQ_U32(n, 11, "payload length");
    CHECK_EQ_U32(out[0], MC_CMD_SET_RADIO_PARAMS, "command code");
    CHECK_EQ_U32((uint32_t)out[1] | ((uint32_t)out[2] << 8) | ((uint32_t)out[3] << 16) |
                     ((uint32_t)out[4] << 24),
                 869525, "frequency, kHz, little-endian");
    CHECK_EQ_U32((uint32_t)out[5] | ((uint32_t)out[6] << 8) | ((uint32_t)out[7] << 16) |
                     ((uint32_t)out[8] << 24),
                 250, "bandwidth");
    CHECK_EQ_U32(out[9], 10, "spreading factor");
    CHECK_EQ_U32(out[10], 5, "coding rate");
}

static void test_cmd_misc(void) {
    section("commands: SET_ADVERT_NAME / SEND_SELF_ADVERT / SET_TX_POWER");

    uint8_t out[64];

    size_t n = mc_cmd_set_advert_name(out, sizeof(out), "greyrock");
    CHECK_EQ_U32(n, 1 + 8, "advert name length");
    CHECK_EQ_U32(out[0], MC_CMD_SET_ADVERT_NAME, "advert name command code");
    CHECK(memcmp(out + 1, "greyrock", 8) == 0, "advert name bytes");

    n = mc_cmd_send_self_advert(out, sizeof(out), MC_ADVERT_FLOOD);
    CHECK_EQ_U32(n, 2, "self advert length");
    CHECK_EQ_U32(out[0], MC_CMD_SEND_SELF_ADVERT, "self advert command code");
    CHECK_EQ_U32(out[1], MC_ADVERT_FLOOD, "flood mode");

    n = mc_cmd_set_tx_power(out, sizeof(out), 22);
    CHECK_EQ_U32(n, 5, "tx power length");
    CHECK_EQ_U32(out[0], MC_CMD_SET_TX_POWER, "tx power command code");
    CHECK_EQ_U32(out[1], 22, "tx power dBm");
}

/* ======================================================================== *
 *  Response parsers
 * ======================================================================== */
static void test_parse_self_info(void) {
    section("parsing: SELF_INFO (the config scene_connect reads)");

    uint8_t p[128];
    memset(p, 0, sizeof(p));
    size_t i = 0;
    p[i++] = MC_RESP_SELF_INFO;

    uint8_t* b = p + 1; /* body starts after the response code */
    b[0] = 1; /* type          */
    b[1] = 22; /* tx_power      */
    b[2] = 30; /* max_tx_power  */
    /* b[3..34] public key, b[35..42] lat/lon — left zero */
    b[43] = 0; /* multi_acks       */
    b[44] = 0; /* adv_loc_policy   */
    b[45] = 0; /* telemetry_mode   */
    b[46] = 0; /* manual_add       */
    put_u32_le(b + 47, 869525); /* radio_freq, kHz */
    put_u32_le(b + 51, 250); /* radio_bw        */
    b[55] = 10; /* sf */
    b[56] = 5; /* cr */
    memcpy(b + 57, "greyrock-1", 10);
    size_t body_len = 57 + 10;

    mc_event_t ev;
    CHECK(mc_parse(p, 1 + body_len, &ev) == 1, "SELF_INFO should parse");
    CHECK_EQ_U32(ev.code, MC_RESP_SELF_INFO, "event code");
    CHECK_EQ_U32(ev.u.self_info.tx_power, 22, "tx_power");
    CHECK_EQ_U32(ev.u.self_info.max_tx_power, 30, "max_tx_power");
    CHECK_EQ_U32(ev.u.self_info.radio_freq, 869525, "radio_freq");
    CHECK_EQ_U32(ev.u.self_info.radio_bw, 250, "radio_bw");
    CHECK_EQ_U32(ev.u.self_info.radio_sf, 10, "radio_sf");
    CHECK_EQ_U32(ev.u.self_info.radio_cr, 5, "radio_cr");
    CHECK_EQ_STR(ev.u.self_info.name, "greyrock-1", "node name");

    UNUSED(i);
}

static void test_parse_device_info(void) {
    section("parsing: DEVICE_INFO (model and firmware version)");

    uint8_t p[256];
    memset(p, 0, sizeof(p));
    p[0] = MC_RESP_DEVICE_INFO;

    uint8_t* b = p + 1;
    b[0] = 13; /* fw_ver */
    b[1] = 100; /* max_contacts / 2 */
    b[2] = 20; /* max_channels */
    put_u32_le(b + 3, 123456); /* ble_pin */
    memcpy(b + 7, "09-05-2026", 10); /* build_date, 12-byte field */
    memcpy(b + 19, "Heltec_T114", 11); /* model, 40-byte field     */
    memcpy(b + 59, "v1.12.0", 7); /* ver, 20-byte field       */
    size_t body_len = 79;

    mc_event_t ev;
    CHECK(mc_parse(p, 1 + body_len, &ev) == 1, "DEVICE_INFO should parse");
    CHECK_EQ_U32(ev.u.device_info.fw_ver, 13, "fw_ver");
    CHECK_EQ_U32(ev.u.device_info.max_contacts, 200, "max_contacts is doubled on the wire");
    CHECK_EQ_U32(ev.u.device_info.max_channels, 20, "max_channels");
    CHECK_EQ_STR(ev.u.device_info.build_date, "09-05-2026", "build_date");
    CHECK_EQ_STR(ev.u.device_info.model, "Heltec_T114", "model");
    CHECK_EQ_STR(ev.u.device_info.ver, "v1.12.0", "firmware version string");
}

static void test_parse_contact(void) {
    section("parsing: CONTACT (needed by the messenger contact list)");

    uint8_t p[256];
    memset(p, 0, sizeof(p));
    p[0] = MC_RESP_CONTACT;

    uint8_t* b = p + 1;
    for(int i = 0; i < 32; i++) b[i] = (uint8_t)(0xA0 + i); /* public key */
    b[32] = 1; /* type          */
    b[33] = 0; /* flags         */
    b[34] = 2; /* out_path_len  */
    /* b[35..98] out_path (64 bytes) */
    memcpy(b + 99, "node-bravo", 10); /* adv_name, 32-byte field */
    put_u32_le(b + 131, 1750000000); /* last_advert */
    put_u32_le(b + 135, 0); /* adv_lat     */
    put_u32_le(b + 139, 0); /* adv_lon     */
    put_u32_le(b + 143, 1750000042); /* lastmod     */
    size_t body_len = 147;

    mc_event_t ev;
    CHECK(mc_parse(p, 1 + body_len, &ev) == 1, "CONTACT should parse");
    CHECK_EQ_STR(ev.u.contact.adv_name, "node-bravo", "contact name");
    CHECK_EQ_U32(ev.u.contact.last_advert, 1750000000, "last_advert (drives 'last seen')");
    CHECK_EQ_U32(ev.u.contact.lastmod, 1750000042, "lastmod");
    CHECK_EQ_U32(ev.u.contact.out_path_len, 2, "out_path_len");
    CHECK_EQ_U32(ev.u.contact.public_key[0], 0xA0, "public key first byte");
    CHECK_EQ_U32(ev.u.contact.public_key[31], 0xBF, "public key last byte");

    /* A contact record is 147 bytes plus the code byte — comfortably inside
     * MC_MAX_PAYLOAD, so the assembler never has to drop one. */
    CHECK(1 + body_len <= MC_MAX_PAYLOAD, "contact frame fits the RX buffer");
}

/* ======================================================================== *
 *  Link layer
 * ======================================================================== */
static void link_begin(MeshCoreLink* link) {
    fake_clock_reset();
    fake_uart_reset();
    fake_log_reset();
    meshcore_link_init(link);
    assert(meshcore_link_open(link, fake_log_instance(), FuriHalSerialIdUsart));
}

/* Queue a reply as it would arrive on the wire: framed, radio->app. */
static void queue_reply(const uint8_t* payload, size_t len) {
    uint8_t frame[MC_RX_BUFSZ];
    size_t n = mc_frame_encode(payload, len, frame, sizeof(frame));
    assert(n > 0);
    frame[0] = MC_FRAME_RADIO_TO_APP; /* the node leads with '>' */
    fake_uart_push_rx(frame, n);
}

static void test_link_send(void) {
    section("link: send frames a payload correctly");

    MeshCoreLink link;
    link_begin(&link);

    const uint8_t payload[] = {MC_CMD_GET_DEVICE_TIME};
    CHECK(meshcore_link_send(&link, payload, sizeof(payload)), "send should succeed");

    CHECK_EQ_U32(fake_uart_tx_len(), 4, "3 header bytes + 1 payload byte");
    CHECK_EQ_U32(fake_uart_tx_data()[0], 0x3C, "transmitted lead byte is '<'");
    CHECK_EQ_U32(fake_uart_tx_data()[3], MC_CMD_GET_DEVICE_TIME, "payload byte");
    CHECK_EQ_U32(fake_log_frame_count(true), 1, "outgoing frame is logged");

    meshcore_link_close(&link);
}

static void test_link_request_ok(void) {
    section("link: request returns the awaited reply");

    MeshCoreLink link;
    link_begin(&link);

    const uint8_t reply[] = {MC_RESP_OK};
    queue_reply(reply, sizeof(reply));

    uint8_t payload[64];
    size_t n = mc_cmd_set_tx_power(payload, sizeof(payload), 22);

    mc_event_t ev;
    CHECK(meshcore_link_request(&link, payload, n, MC_RESP_OK, &ev, 1000),
          "request should be answered");
    CHECK_EQ_U32(ev.code, MC_RESP_OK, "event code");
    CHECK_EQ_U32(fake_log_frame_count(false), 1, "incoming frame is logged");

    meshcore_link_close(&link);
}

static void test_link_request_skips_push(void) {
    section("link: unsolicited pushes do not satisfy a request");

    MeshCoreLink link;
    link_begin(&link);

    /* A node can advertise at any moment, including between our command and
     * its reply. That push must be stepped over, not mistaken for the answer. */
    uint8_t push[33];
    memset(push, 0, sizeof(push));
    push[0] = MC_PUSH_ADVERT;
    queue_reply(push, sizeof(push));

    const uint8_t reply[] = {MC_RESP_OK};
    queue_reply(reply, sizeof(reply));

    uint8_t payload[64];
    size_t n = mc_cmd_send_self_advert(payload, sizeof(payload), MC_ADVERT_ZERO_HOP);

    mc_event_t ev;
    CHECK(meshcore_link_request(&link, payload, n, MC_RESP_OK, &ev, 1000),
          "request should still find its reply behind the push");
    CHECK_EQ_U32(ev.code, MC_RESP_OK, "event code is the reply, not the push");
    CHECK_EQ_U32(fake_log_frame_count(false), 2, "both incoming frames are logged");

    meshcore_link_close(&link);
}

static void test_link_request_error(void) {
    section("link: a node-side error is reported, not swallowed");

    MeshCoreLink link;
    link_begin(&link);

    const uint8_t reply[] = {MC_RESP_ERR, 0x05};
    queue_reply(reply, sizeof(reply));

    uint8_t payload[64];
    size_t n = mc_cmd_app_start(payload, sizeof(payload), MESHCORE_LINK_APP_NAME);

    mc_event_t ev;
    CHECK(!meshcore_link_request(&link, payload, n, MC_RESP_SELF_INFO, &ev, 1000),
          "request must fail when the node returns ERR");
    CHECK_EQ_U32(ev.code, MC_RESP_ERR, "caller can tell ERR apart from a timeout");

    meshcore_link_close(&link);
}

static void test_link_request_timeout(void) {
    section("link: silence times out without spinning");

    MeshCoreLink link;
    link_begin(&link);
    /* Nothing queued: this is a node that is not there, or wired TX to TX. */

    uint8_t payload[64];
    size_t n = mc_cmd_app_start(payload, sizeof(payload), MESHCORE_LINK_APP_NAME);

    uint32_t started = furi_get_tick();
    mc_event_t ev;
    CHECK(!meshcore_link_request(&link, payload, n, MC_RESP_SELF_INFO, &ev, 1000),
          "request must fail on silence");
    CHECK_EQ_U32(ev.code, MESHCORE_LINK_NO_EVENT, "timeout is distinguishable from ERR");

    uint32_t elapsed = furi_get_tick() - started;
    CHECK(elapsed >= 1000, "must actually wait the timeout, waited %lu ms", (unsigned long)elapsed);
    CHECK(elapsed <= 1200, "must not overshoot the timeout, waited %lu ms", (unsigned long)elapsed);
    /* A deadline computed wrong shows up as a read storm rather than a hang. */
    CHECK(fake_uart_starved_reads() <= 3, "should block, not poll in a loop (%lu reads)",
          (unsigned long)fake_uart_starved_reads());

    meshcore_link_close(&link);
}

/* ======================================================================== *
 *  Event routing (the session's policy, without the session's threads)
 * ======================================================================== */
static void test_code_set(void) {
    section("routing: reply code sets");

    MeshCoreCodeSet one = meshcore_code_set_one(MC_RESP_OK);
    CHECK_EQ_U32(one.count, 1, "single-code set size");
    CHECK(meshcore_code_set_has(&one, MC_RESP_OK), "contains its code");
    CHECK(!meshcore_code_set_has(&one, MC_RESP_ERR), "and nothing else");

    const uint8_t sync[] = {
        MC_RESP_CONTACT_MSG_RECV,
        MC_RESP_CONTACT_MSG_RECV_V3,
        MC_RESP_CHANNEL_MSG_RECV,
        MC_RESP_CHANNEL_MSG_RECV_V3,
        MC_RESP_NO_MORE_MESSAGES,
    };
    MeshCoreCodeSet set = meshcore_code_set(sync, 5);
    CHECK_EQ_U32(set.count, 5, "SYNC_NEXT_MESSAGE reply set size");
    CHECK(set.count <= MESHCORE_CODE_SET_MAX, "and it fits the fixed set");
    for(size_t i = 0; i < 5; i++) {
        CHECK(meshcore_code_set_has(&set, sync[i]), "every SYNC reply code is accepted");
    }
    CHECK(!meshcore_code_set_has(&set, MC_RESP_SELF_INFO), "unrelated codes are not");

    /* Overflowing must clamp, not scribble past the array. */
    const uint8_t many[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    MeshCoreCodeSet clamped = meshcore_code_set(many, sizeof(many));
    CHECK_EQ_U32(clamped.count, MESHCORE_CODE_SET_MAX, "oversized input is clamped");
}

static void test_route(void) {
    section("routing: replies, streams and unsolicited pushes");

    MeshCoreCodeSet ok = meshcore_code_set_one(MC_RESP_OK);
    MeshCoreCodeSet self_info = meshcore_code_set_one(MC_RESP_SELF_INFO);
    MeshCoreCodeSet err = meshcore_code_set_one(MC_RESP_ERR);
    MeshCoreCodeSet end = meshcore_code_set_one(MC_RESP_END_OF_CONTACTS);

    /* Idle: everything is unsolicited. This is the case the messenger depends
     * on — a MSG_WAITING push arriving while the user is idle must reach the
     * application rather than being dropped. */
    CHECK(meshcore_route_event(false, &ok, false, MC_PUSH_MSG_WAITING) == MeshCoreRouteEvent,
          "idle push goes to the application");
    CHECK(meshcore_route_event(false, &ok, false, MC_RESP_OK) == MeshCoreRouteEvent,
          "even a reply-looking code is unsolicited when nothing is pending");

    /* In flight. */
    CHECK(meshcore_route_event(true, &self_info, false, MC_RESP_SELF_INFO) == MeshCoreRouteReply,
          "the awaited code completes the request");
    CHECK(meshcore_route_event(true, &self_info, false, MC_RESP_ERR) == MeshCoreRouteReply,
          "a node error ends the request");
    CHECK(meshcore_route_event(true, &err, false, MC_RESP_ERR) == MeshCoreRouteReply,
          "a request that awaits ERR still completes on ERR");

    /* A push during a plain request must not be mistaken for the answer. */
    CHECK(meshcore_route_event(true, &self_info, false, MC_PUSH_ADVERT) == MeshCoreRouteEvent,
          "a push mid-request stays unsolicited");

    /* Multi-code replies: SYNC_NEXT_MESSAGE can come back five different ways
     * and any of them ends the request. */
    const uint8_t sync[] = {
        MC_RESP_CONTACT_MSG_RECV,
        MC_RESP_CONTACT_MSG_RECV_V3,
        MC_RESP_CHANNEL_MSG_RECV,
        MC_RESP_CHANNEL_MSG_RECV_V3,
        MC_RESP_NO_MORE_MESSAGES,
    };
    MeshCoreCodeSet sync_set = meshcore_code_set(sync, 5);
    for(size_t i = 0; i < 5; i++) {
        CHECK(meshcore_route_event(true, &sync_set, false, sync[i]) == MeshCoreRouteReply,
              "each SYNC reply completes the drain step");
    }
    CHECK(meshcore_route_event(true, &sync_set, false, MC_PUSH_ADVERT) == MeshCoreRouteEvent,
          "an advert during a drain is still unsolicited");

    /* During a stream, everything non-terminating is offered to the collector,
     * which declines what is not its own — so pushes still get through. */
    CHECK(meshcore_route_event(true, &end, true, MC_RESP_CONTACT) == MeshCoreRouteStream,
          "contact records go to the collector");
    CHECK(meshcore_route_event(true, &end, true, MC_RESP_CONTACTS_START) == MeshCoreRouteStream,
          "the count header goes to the collector");
    CHECK(meshcore_route_event(true, &end, true, MC_RESP_END_OF_CONTACTS) == MeshCoreRouteReply,
          "the terminator completes the stream");
    CHECK(meshcore_route_event(true, &end, true, MC_PUSH_MSG_WAITING) == MeshCoreRouteStream,
          "a push mid-stream is offered to the collector first");
}

/* ======================================================================== *
 *  Message store
 * ======================================================================== */
static void test_messages_from_event(void) {
    section("messages: turning replies into stored messages");

    mc_event_t ev;
    MeshCoreMessage msg;

    /* Contact message, plain. */
    memset(&ev, 0, sizeof(ev));
    ev.code = MC_RESP_CONTACT_MSG_RECV;
    for(int i = 0; i < 6; i++) ev.u.contact_msg.pubkey_prefix[i] = (uint8_t)(0x10 + i);
    ev.u.contact_msg.sender_ts = 1750000000;
    ev.u.contact_msg.snr_q4 = MC_SNR_NONE;
    ev.u.contact_msg.path_len = MC_PATH_DIRECT;
    snprintf(ev.u.contact_msg.text, sizeof(ev.u.contact_msg.text), "ping from the hill");

    CHECK(meshcore_message_from_event(&ev, &msg), "a contact message converts");
    CHECK(msg.direction == MeshCoreMessageIncoming, "incoming");
    CHECK(!msg.is_channel, "not a channel message");
    CHECK_EQ_U32(msg.timestamp, 1750000000, "timestamp carried over");
    CHECK_EQ_STR(msg.text, "ping from the hill", "text carried over");
    CHECK_EQ_U32(msg.peer[0], 0x10, "peer prefix first byte");
    CHECK_EQ_U32(msg.peer[5], 0x15, "peer prefix last byte");

    /* The V3 variant differs only by carrying SNR; it must convert the same. */
    memset(&ev, 0, sizeof(ev));
    ev.code = MC_RESP_CONTACT_MSG_RECV_V3;
    ev.u.contact_msg.snr_q4 = 24; /* 6.0 dB */
    snprintf(ev.u.contact_msg.text, sizeof(ev.u.contact_msg.text), "v3");
    CHECK(meshcore_message_from_event(&ev, &msg), "the V3 variant converts too");
    CHECK_EQ_U32((uint8_t)msg.snr_q4, 24, "SNR preserved");

    /* Channel message. */
    memset(&ev, 0, sizeof(ev));
    ev.code = MC_RESP_CHANNEL_MSG_RECV;
    ev.u.channel_msg.channel_idx = 2;
    snprintf(ev.u.channel_msg.text, sizeof(ev.u.channel_msg.text), "alpha: net check");
    CHECK(meshcore_message_from_event(&ev, &msg), "a channel message converts");
    CHECK(msg.is_channel, "flagged as a channel message");
    CHECK_EQ_U32(msg.channel_idx, 2, "channel index");
    CHECK_EQ_STR(msg.text, "alpha: net check", "channel text keeps its sender prefix");

    /* Anything else is not a message. */
    memset(&ev, 0, sizeof(ev));
    ev.code = MC_RESP_NO_MORE_MESSAGES;
    CHECK(!meshcore_message_from_event(&ev, &msg), "NO_MORE_MESSAGES is not a message");
    ev.code = MC_RESP_OK;
    CHECK(!meshcore_message_from_event(&ev, &msg), "neither is OK");
}

static void add_message(MeshCoreMessages* store, const uint8_t* peer, const char* text, uint32_t ts) {
    MeshCoreMessage msg;
    memset(&msg, 0, sizeof(msg));
    memcpy(msg.peer, peer, MESHCORE_PEER_LEN);
    msg.direction = MeshCoreMessageIncoming;
    msg.timestamp = ts;
    snprintf(msg.text, sizeof(msg.text), "%s", text);
    meshcore_messages_add(store, &msg);
}

static void test_messages_ring(void) {
    section("messages: the ring drops the oldest");

    MeshCoreMessages store;
    meshcore_messages_reset(&store);

    const uint8_t peer[MESHCORE_PEER_LEN] = {1, 2, 3, 4, 5, 6};

    for(size_t i = 0; i < MESHCORE_MESSAGES_MAX; i++) {
        char text[16];
        snprintf(text, sizeof(text), "m%u", (unsigned)i);
        add_message(&store, peer, text, 1000 + (uint32_t)i);
    }
    CHECK_EQ_U32(store.count, MESHCORE_MESSAGES_MAX, "ring is full");
    CHECK_EQ_U32(store.total, MESHCORE_MESSAGES_MAX, "total counts everything added");
    CHECK_EQ_STR(meshcore_messages_at(&store, 0)->text, "m0", "oldest is first");

    /* Three more push the three oldest out. */
    add_message(&store, peer, "x0", 9000);
    add_message(&store, peer, "x1", 9001);
    add_message(&store, peer, "x2", 9002);

    CHECK_EQ_U32(store.count, MESHCORE_MESSAGES_MAX, "still full, never grows");
    CHECK_EQ_U32(store.total, MESHCORE_MESSAGES_MAX + 3, "total keeps climbing");
    CHECK_EQ_STR(meshcore_messages_at(&store, 0)->text, "m3", "the three oldest were dropped");
    CHECK_EQ_STR(
        meshcore_messages_at(&store, MESHCORE_MESSAGES_MAX - 1)->text, "x2", "newest is last");
    CHECK(meshcore_messages_at(&store, MESHCORE_MESSAGES_MAX) == NULL, "past the end is NULL");
}

static void test_messages_peer_filter(void) {
    section("messages: conversations are keyed by peer prefix");

    MeshCoreMessages store;
    meshcore_messages_reset(&store);

    const uint8_t alpha[MESHCORE_PEER_LEN] = {0xAA, 1, 2, 3, 4, 5};
    const uint8_t bravo[MESHCORE_PEER_LEN] = {0xBB, 1, 2, 3, 4, 5};

    add_message(&store, alpha, "from alpha", 1);
    add_message(&store, bravo, "from bravo", 2);
    add_message(&store, alpha, "alpha again", 3);

    /* A contact carries a full 32-byte key; only the first 6 are comparable,
     * because that is all an incoming message identifies the sender by. */
    uint8_t alpha_full[32];
    memset(alpha_full, 0x77, sizeof(alpha_full));
    memcpy(alpha_full, alpha, MESHCORE_PEER_LEN);

    CHECK_EQ_U32(meshcore_messages_count_for(&store, alpha_full), 2, "two from alpha");
    CHECK(meshcore_message_is_from(meshcore_messages_at(&store, 0), alpha_full), "first matches");
    CHECK(!meshcore_message_is_from(meshcore_messages_at(&store, 1), alpha_full),
          "bravo's does not");

    /* Channel traffic belongs to no contact conversation. */
    MeshCoreMessage channel;
    memset(&channel, 0, sizeof(channel));
    channel.is_channel = true;
    memcpy(channel.peer, alpha, MESHCORE_PEER_LEN);
    CHECK(!meshcore_message_is_from(&channel, alpha_full),
          "a channel message is not part of a direct chat");
}

/* ======================================================================== *
 *  Contact list
 * ======================================================================== */
static void make_contact(mc_contact_t* c, const char* name, uint32_t last_advert) {
    memset(c, 0, sizeof(*c));
    snprintf(c->adv_name, sizeof(c->adv_name), "%s", name);
    c->last_advert = last_advert;
    c->type = 1;
}

static void test_contacts_collect(void) {
    section("contacts: collecting a GET_CONTACTS stream");

    MeshCoreContacts contacts;
    meshcore_contacts_reset(&contacts);

    mc_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.code = MC_RESP_CONTACTS_START;
    ev.u.contacts_count = 3;
    CHECK(meshcore_contacts_collect(&ev, &contacts), "CONTACTS_START belongs to the stream");
    CHECK_EQ_U32(contacts.reported, 3, "announced count is remembered");

    memset(&ev, 0, sizeof(ev));
    ev.code = MC_RESP_CONTACT;
    make_contact(&ev.u.contact, "alpha", 1000);
    CHECK(meshcore_contacts_collect(&ev, &contacts), "CONTACT belongs to the stream");
    CHECK_EQ_U32(contacts.count, 1, "contact stored");
    CHECK_EQ_STR(contacts.items[0].name, "alpha", "contact name");

    /* Anything else must be declined so it can reach the application. */
    memset(&ev, 0, sizeof(ev));
    ev.code = MC_PUSH_MSG_WAITING;
    CHECK(!meshcore_contacts_collect(&ev, &contacts), "a push is not part of the stream");
    CHECK_EQ_U32(contacts.count, 1, "and does not disturb the list");
}

static void test_contacts_overflow(void) {
    section("contacts: more peers than fit");

    MeshCoreContacts contacts;
    meshcore_contacts_reset(&contacts);

    mc_contact_t c;
    for(size_t i = 0; i < MESHCORE_CONTACTS_MAX; i++) {
        char name[16];
        snprintf(name, sizeof(name), "n%u", (unsigned)i);
        /* last_advert 1000, 1001, ... so the stalest is the first one. */
        make_contact(&c, name, 1000 + (uint32_t)i);
        CHECK(meshcore_contacts_add(&contacts, &c), "fits while there is room");
    }
    CHECK_EQ_U32(contacts.count, MESHCORE_CONTACTS_MAX, "list is full");
    CHECK_EQ_U32(contacts.dropped, 0, "nothing dropped yet");

    /* A fresher peer displaces the stalest one — a list of nodes last heard
     * from months ago is not worth the screen. */
    make_contact(&c, "fresh", 9999);
    CHECK(meshcore_contacts_add(&contacts, &c), "a fresher peer is kept");
    CHECK_EQ_U32(contacts.count, MESHCORE_CONTACTS_MAX, "still full, nothing grew");
    CHECK_EQ_U32(contacts.dropped, 1, "displacement is counted");

    bool found_fresh = false;
    bool found_stalest = false;
    for(size_t i = 0; i < contacts.count; i++) {
        if(strcmp(contacts.items[i].name, "fresh") == 0) found_fresh = true;
        if(strcmp(contacts.items[i].name, "n0") == 0) found_stalest = true;
    }
    CHECK(found_fresh, "the fresher peer is in the list");
    CHECK(!found_stalest, "the stalest peer was the one dropped");

    /* A staler peer than everything held is simply refused. */
    make_contact(&c, "ancient", 1);
    CHECK(!meshcore_contacts_add(&contacts, &c), "a staler peer is refused");
    CHECK_EQ_U32(contacts.dropped, 2, "and counted");
}

static void test_contacts_sort(void) {
    section("contacts: most recently heard first");

    MeshCoreContacts contacts;
    meshcore_contacts_reset(&contacts);

    mc_contact_t c;
    make_contact(&c, "old", 100);
    meshcore_contacts_add(&contacts, &c);
    make_contact(&c, "newest", 900);
    meshcore_contacts_add(&contacts, &c);
    make_contact(&c, "middle", 500);
    meshcore_contacts_add(&contacts, &c);

    meshcore_contacts_sort_by_last_seen(&contacts);

    CHECK_EQ_STR(contacts.items[0].name, "newest", "first entry");
    CHECK_EQ_STR(contacts.items[1].name, "middle", "second entry");
    CHECK_EQ_STR(contacts.items[2].name, "old", "third entry");
}

static void test_contacts_age(void) {
    section("contacts: rendering last seen");

    char out[MESHCORE_AGE_LEN];

    meshcore_contacts_format_age(0, 1000, out, sizeof(out));
    CHECK_EQ_STR(out, "-", "unknown node clock");

    meshcore_contacts_format_age(2000, 0, out, sizeof(out));
    CHECK_EQ_STR(out, "-", "never heard from");

    /* The node's clock can legitimately run ahead of the advert timestamp. */
    meshcore_contacts_format_age(1000, 1200, out, sizeof(out));
    CHECK_EQ_STR(out, "now", "clock skew reads as now, not as a negative age");

    meshcore_contacts_format_age(1059, 1000, out, sizeof(out));
    CHECK_EQ_STR(out, "now", "under a minute");

    meshcore_contacts_format_age(1060, 1000, out, sizeof(out));
    CHECK_EQ_STR(out, "1m", "exactly a minute");

    meshcore_contacts_format_age(1000 + 3599, 1000, out, sizeof(out));
    CHECK_EQ_STR(out, "59m", "just under an hour");

    meshcore_contacts_format_age(1000 + 7200, 1000, out, sizeof(out));
    CHECK_EQ_STR(out, "2h", "hours");

    meshcore_contacts_format_age(1000 + 86400 * 3, 1000, out, sizeof(out));
    CHECK_EQ_STR(out, "3d", "days");

    /* Whatever it renders must fit the buffer the scene gives it. */
    meshcore_contacts_format_age(0xFFFFFFFFu, 1, out, sizeof(out));
    CHECK(strlen(out) < MESHCORE_AGE_LEN, "an absurd age still fits MESHCORE_AGE_LEN");
}

/* ======================================================================== *
 *  RX log push — the Logger's data source
 * ======================================================================== */
static void test_rxlog_parse(void) {
    section("rx log: decoding the 0x88 push");

    /* [0x88][snr*4][rssi][packet...] — the layout MyMesh::logRxRaw() writes. */
    uint8_t payload[8];
    payload[0] = MESHCORE_RXLOG_CODE;
    payload[1] = (uint8_t)25; /* +6.25 dB */
    payload[2] = (uint8_t)(int8_t)-92; /* dBm       */
    payload[3] = 0xDE;
    payload[4] = 0xAD;
    payload[5] = 0xBE;
    payload[6] = 0xEF;

    MeshCoreRxLog rx;
    CHECK(meshcore_rxlog_parse(payload, 7, &rx), "a well-formed push decodes");
    CHECK_EQ_U32((uint8_t)rx.snr_q4, 25, "SNR is kept in quarter-dB");
    CHECK(rx.rssi == -92, "RSSI is signed dBm: got %d", (int)rx.rssi);
    CHECK_EQ_U32(rx.raw_len, 4, "packet length is the payload minus the 3-byte head");
    CHECK(rx.raw[0] == 0xDE && rx.raw[3] == 0xEF, "packet bytes start right after RSSI");

    /* A negative SNR is the normal case at the edge of range. */
    payload[1] = (uint8_t)(int8_t)-30;
    CHECK(meshcore_rxlog_parse(payload, 7, &rx), "still decodes");
    CHECK(rx.snr_q4 == -30, "negative SNR survives the round trip");

    /* The radio can log a runt with no packet body at all. */
    CHECK(meshcore_rxlog_parse(payload, 3, &rx), "a header-only push is valid");
    CHECK_EQ_U32(rx.raw_len, 0, "with an empty packet");

    /* Rejections. */
    CHECK(!meshcore_rxlog_parse(payload, 2, &rx), "too short to hold SNR and RSSI");
    payload[0] = MC_PUSH_ADVERT;
    CHECK(!meshcore_rxlog_parse(payload, 7, &rx), "a different push is not an RX log");
}

static void test_rxlog_format(void) {
    section("rx log: rendering for CSV");

    char snr[MESHCORE_SNR_LEN];

    meshcore_rxlog_format_snr(0, snr, sizeof(snr));
    CHECK_EQ_STR(snr, "0.00", "zero");
    meshcore_rxlog_format_snr(25, snr, sizeof(snr));
    CHECK_EQ_STR(snr, "6.25", "quarter-dB steps are exact");
    meshcore_rxlog_format_snr(20, snr, sizeof(snr));
    CHECK_EQ_STR(snr, "5.00", "a whole number still shows two decimals");
    meshcore_rxlog_format_snr(-5, snr, sizeof(snr));
    CHECK_EQ_STR(snr, "-1.25", "negative values keep their sign and magnitude");
    meshcore_rxlog_format_snr(-1, snr, sizeof(snr));
    CHECK_EQ_STR(snr, "-0.25", "a negative with a zero whole part is not lost");
    /* -128 is the sentinel meshcore_c uses for "no SNR"; it must not overflow
     * on negation. */
    meshcore_rxlog_format_snr(-128, snr, sizeof(snr));
    CHECK_EQ_STR(snr, "-32.00", "the extreme negative does not wrap");

    char hex[16];
    const uint8_t data[] = {0x00, 0x0F, 0xA5, 0xFF};
    size_t n = meshcore_hex_encode(data, sizeof(data), hex, sizeof(hex));
    CHECK_EQ_U32(n, 8, "two characters per byte");
    CHECK_EQ_STR(hex, "000fa5ff", "lowercase, no separators, leading zeros kept");

    n = meshcore_hex_encode(data, sizeof(data), hex, 5);
    CHECK_EQ_U32(n, 4, "truncates to what fits");
    CHECK_EQ_STR(hex, "000f", "and stays NUL-terminated");
    CHECK(strlen(hex) < 5, "never writes past the buffer");

    n = meshcore_hex_encode(data, 0, hex, sizeof(hex));
    CHECK_EQ_U32(n, 0, "an empty packet renders as an empty field");
    CHECK_EQ_STR(hex, "", "which is what the CSV column should hold");
}

static void test_rxlog_grade(void) {
    section("rx log: grading SNR against the field threshold");

    /* The boundary is +5 dB = 20 quarter-dB. On it is good; a hair under is not.
     * These exact edges are the whole reason this is a pinned test — the walk
     * is where a mis-set threshold would quietly pass a link that will fail. */
    CHECK(meshcore_rxlog_grade_snr(20) == MeshCoreSnrGood, "+5.00 dB is GOOD");
    CHECK(meshcore_rxlog_grade_snr(21) == MeshCoreSnrGood, "above +5 is GOOD");
    CHECK(meshcore_rxlog_grade_snr(127) == MeshCoreSnrGood, "the ceiling is GOOD");
    CHECK(meshcore_rxlog_grade_snr(19) == MeshCoreSnrMarginal, "+4.75 dB is only OK");
    CHECK(meshcore_rxlog_grade_snr(0) == MeshCoreSnrMarginal, "exactly 0 dB is OK");
    CHECK(meshcore_rxlog_grade_snr(-1) == MeshCoreSnrBad, "-0.25 dB is WEAK");
    CHECK(meshcore_rxlog_grade_snr(-128) == MeshCoreSnrBad, "the no-signal floor is WEAK");

    CHECK_EQ_STR(meshcore_rxlog_grade_label(MeshCoreSnrGood), "GOOD", "good label");
    CHECK_EQ_STR(meshcore_rxlog_grade_label(MeshCoreSnrMarginal), "OK", "marginal label");
    CHECK_EQ_STR(meshcore_rxlog_grade_label(MeshCoreSnrBad), "WEAK", "bad label");
}

/* ======================================================================== *
 *  Preset parsing — the code that meets hand-edited files
 * ======================================================================== */
static void test_parse_scaled(void) {
    section("presets: decimals to wire units, without floats");

    uint32_t value = 0;

    /* The built-in preset's frequency. The wire carries whole kHz, so the
     * trailing digits are dropped -- exactly as the reference client does. */
    CHECK(meshcore_parse_scaled("868.731018", 1000, &value), "parses a long decimal");
    CHECK_EQ_U32(value, 868731, "868.731018 MHz -> kHz");

    CHECK(meshcore_parse_scaled("62.5", 1000, &value), "parses one decimal place");
    CHECK_EQ_U32(value, 62500, "62.5 kHz -> Hz");

    CHECK(meshcore_parse_scaled("250", 1000, &value), "parses a whole number");
    CHECK_EQ_U32(value, 250000, "250 kHz -> Hz");

    CHECK(meshcore_parse_scaled("0.5", 1000, &value), "parses a leading zero");
    CHECK_EQ_U32(value, 500, "0.5 -> 500");

    CHECK(meshcore_parse_scaled("7", 1, &value) && value == 7, "scale of 1 is a plain integer");

    /* Everything a hand-edited file might contain. */
    CHECK(!meshcore_parse_scaled("", 1000, &value), "empty string is rejected");
    CHECK(!meshcore_parse_scaled("abc", 1000, &value), "letters are rejected");
    CHECK(!meshcore_parse_scaled("-5", 1000, &value), "negatives are rejected");
    CHECK(!meshcore_parse_scaled("868.7x", 1000, &value), "trailing junk is rejected");
    CHECK(!meshcore_parse_scaled("99999999999", 1000, &value), "overflow is rejected");
}

static void test_json_get(void) {
    section("presets: reading values out of JSON");

    static const char* doc =
        "{\n"
        "  \"name\": \"Fest-869\",\n"
        "  \"freq_mhz\": 869.525,\n"
        "  \"sf\": 11,\n"
        "  \"advert\": true,\n"
        "  \"nested\": { \"sf\": 99 }\n"
        "}\n";

    char buffer[32];

    CHECK(meshcore_json_get(doc, "name", buffer, sizeof(buffer)), "finds a string");
    CHECK_EQ_STR(buffer, "Fest-869", "string arrives unquoted");

    CHECK(meshcore_json_get(doc, "freq_mhz", buffer, sizeof(buffer)), "finds a number");
    CHECK_EQ_STR(buffer, "869.525", "number arrives verbatim");

    uint32_t value = 0;
    CHECK(meshcore_json_get_uint(doc, "sf", &value) && value == 11, "typed integer read");

    bool flag = false;
    CHECK(meshcore_json_get_bool(doc, "advert", &flag) && flag, "typed boolean read");

    CHECK(!meshcore_json_get(doc, "absent", buffer, sizeof(buffer)), "missing key fails");

    /* A key inside a nested object must not be mistaken for a top-level one:
     * "sf" appears twice here and the outer value has to win. */
    CHECK(meshcore_json_get_uint(doc, "sf", &value) && value == 11, "nested keys do not shadow");

    /* A value that does not fit must fail rather than truncate silently. */
    char tiny[4];
    CHECK(!meshcore_json_get(doc, "name", tiny, sizeof(tiny)), "oversized value is refused");

    CHECK(
        !meshcore_json_get("{\"name\": \"unterminated", "name", buffer, sizeof(buffer)),
        "unterminated string is refused");
}

static void test_preset_builtin(void) {
    section("presets: the built-in City/daily");

    CHECK(MESHCORE_BUILTIN_PRESET_COUNT >= 1, "at least one preset ships in the binary");

    const MeshCorePreset* preset = &MESHCORE_BUILTIN_PRESETS[0];
    CHECK_EQ_STR(preset->name, "City/daily", "name");
    CHECK_EQ_U32(preset->freq_khz, 868731, "frequency in wire units");
    CHECK_EQ_U32(preset->bw_hz, 62500, "bandwidth in wire units");
    CHECK_EQ_U32(preset->sf, 7, "spreading factor");
    CHECK_EQ_U32(preset->cr, 7, "coding rate");
    CHECK_EQ_U32(preset->path_hash_bytes, 1, "path hash bytes");
    CHECK(preset->built_in, "flagged as built in");

    const char* why = NULL;
    CHECK(meshcore_preset_validate(preset, &why), "and it passes validation");

    /* The wire wants the mode, which is one less than the byte count. The
     * everyday preset stays on 1-byte (mode 0) so v1.13.0-and-older nodes relay
     * it. */
    CHECK_EQ_U32(meshcore_preset_path_hash_mode(preset), 0, "1 byte means mode 0");

    char text[MESHCORE_PRESET_FIELD_LEN];
    meshcore_preset_format_freq(preset->freq_khz, text, sizeof(text));
    CHECK_EQ_STR(text, "868.731 MHz", "frequency renders for the screen");
    meshcore_preset_format_bw(preset->bw_hz, text, sizeof(text));
    CHECK_EQ_STR(text, "62.5 kHz", "bandwidth renders for the screen");
}

static void test_preset_from_json(void) {
    section("presets: loading one from the SD card");

    MeshCorePreset preset;
    const char* why = NULL;

    static const char* good =
        "{ \"name\": \"Fest-869\", \"freq_mhz\": 869.525, \"bw_khz\": 250,"
        "  \"sf\": 11, \"cr\": 5, \"path_hash_bytes\": 1 }";

    CHECK(meshcore_preset_from_json(good, &preset, &why), "a well-formed preset loads");
    CHECK_EQ_STR(preset.name, "Fest-869", "name");
    CHECK_EQ_U32(preset.freq_khz, 869525, "MHz converted to kHz");
    CHECK_EQ_U32(preset.bw_hz, 250000, "kHz converted to Hz");
    CHECK_EQ_U32(preset.sf, 11, "spreading factor");
    CHECK_EQ_U32(preset.cr, 5, "coding rate");
    CHECK(!preset.has_node_name, "optional node name absent");
    CHECK(!preset.built_in, "loaded presets are not built in");

    /* Optional fields. */
    static const char* with_optional =
        "{ \"name\": \"Base\", \"freq_mhz\": 868, \"bw_khz\": 62.5, \"sf\": 7, \"cr\": 7,"
        "  \"path_hash_bytes\": 3, \"node_name\": \"BASE-1\", \"role\": \"repeater\" }";
    CHECK(meshcore_preset_from_json(with_optional, &preset, &why), "optional fields load");
    CHECK(preset.has_node_name && strcmp(preset.node_name, "BASE-1") == 0, "node name");
    CHECK(preset.has_role && strcmp(preset.role, "repeater") == 0, "role");
    CHECK_EQ_U32(meshcore_preset_path_hash_mode(&preset), 2, "3 bytes means mode 2");

    /* A preset written before path hash was understood keeps working, and
     * falls back to the firmware default of one byte. */
    static const char* legacy =
        "{ \"name\": \"Old\", \"freq_mhz\": 869.525, \"bw_khz\": 250, \"sf\": 10, \"cr\": 5 }";
    CHECK(meshcore_preset_from_json(legacy, &preset, &why), "a preset without path hash loads");
    CHECK_EQ_U32(preset.path_hash_bytes, 1, "and defaults to one byte");

    /* Everything a hand-edited file gets wrong. Each must fail with a reason
     * rather than load something plausible-looking. */
    struct {
        const char* json;
        const char* label;
    } bad[] = {
        {"{ \"freq_mhz\": 869, \"bw_khz\": 250, \"sf\": 10, \"cr\": 5 }", "missing name"},
        {"{ \"name\": \"x\", \"bw_khz\": 250, \"sf\": 10, \"cr\": 5 }", "missing frequency"},
        {"{ \"name\": \"x\", \"freq_mhz\": 869, \"sf\": 10, \"cr\": 5 }", "missing bandwidth"},
        {"{ \"name\": \"x\", \"freq_mhz\": 869, \"bw_khz\": 250, \"cr\": 5 }", "missing sf"},
        {"{ \"name\": \"x\", \"freq_mhz\": 869, \"bw_khz\": 250, \"sf\": 99, \"cr\": 5 }",
         "spreading factor out of range"},
        {"{ \"name\": \"x\", \"freq_mhz\": 869, \"bw_khz\": 250, \"sf\": 10, \"cr\": 1 }",
         "coding rate out of range"},
        {"{ \"name\": \"x\", \"freq_mhz\": 8690, \"bw_khz\": 250, \"sf\": 10, \"cr\": 5 }",
         "frequency out of range"},
        {"{ \"name\": \"x\", \"freq_mhz\": 869, \"bw_khz\": 250, \"sf\": 10, \"cr\": 5,"
         "  \"path_hash_bytes\": 4 }",
         "path hash out of range"},
    };

    for(size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        why = NULL;
        bool loaded = meshcore_preset_from_json(bad[i].json, &preset, &why);
        CHECK(!loaded, "%s", bad[i].label);
        CHECK(why != NULL && why[0] != '\0', "and says why: %s", why ? why : "(no reason)");
    }
}

/* ======================================================================== *
 *  Applying a preset
 * ======================================================================== */
static MeshCorePreset sample_preset(bool with_name) {
    MeshCorePreset preset;
    memset(&preset, 0, sizeof(preset));
    snprintf(preset.name, sizeof(preset.name), "City/daily");
    preset.freq_khz = 868731;
    preset.bw_hz = 62500;
    preset.sf = 7;
    preset.cr = 7;
    preset.path_hash_bytes = 2;
    if(with_name) {
        preset.has_node_name = true;
        snprintf(preset.node_name, sizeof(preset.node_name), "ROVER-1");
    }
    return preset;
}

static void test_apply_build(void) {
    section("apply: building the commands a preset turns into");

    MeshCorePreset preset = sample_preset(false);
    uint8_t out[64];

    /* Radio: one command carrying all four values, in wire units. */
    size_t len = meshcore_apply_build(&preset, MeshCoreApplyRadio, out, sizeof(out));
    CHECK_EQ_U32(len, 11, "radio payload length");
    CHECK_EQ_U32(out[0], MC_CMD_SET_RADIO_PARAMS, "radio command code");
    CHECK_EQ_U32(
        (uint32_t)out[1] | ((uint32_t)out[2] << 8) | ((uint32_t)out[3] << 16) |
            ((uint32_t)out[4] << 24),
        868731,
        "frequency goes out in kHz");
    CHECK_EQ_U32(
        (uint32_t)out[5] | ((uint32_t)out[6] << 8) | ((uint32_t)out[7] << 16) |
            ((uint32_t)out[8] << 24),
        62500,
        "bandwidth goes out in Hz");
    CHECK_EQ_U32(out[9], 7, "spreading factor");
    CHECK_EQ_U32(out[10], 7, "coding rate");

    /* Path hash: its own command, and the mandatory zero byte. */
    len = meshcore_apply_build(&preset, MeshCoreApplyPathHash, out, sizeof(out));
    CHECK_EQ_U32(len, 3, "path hash payload length");
    CHECK_EQ_U32(out[0], MESHCORE_CMD_SET_PATH_HASH_MODE, "path hash command code is 61");
    CHECK_EQ_U32(out[1], 0, "the second byte must be zero or firmware rejects it");
    CHECK_EQ_U32(out[2], 1, "2 bytes is mode 1");

    /* Name: skipped entirely when the preset does not carry one -- sending an
     * empty name would wipe the node's own. */
    CHECK(!meshcore_apply_step_applies(&preset, MeshCoreApplyName), "no name means no name step");
    CHECK_EQ_U32(
        meshcore_apply_build(&preset, MeshCoreApplyName, out, sizeof(out)),
        0,
        "and builds nothing");

    MeshCorePreset named = sample_preset(true);
    CHECK(meshcore_apply_step_applies(&named, MeshCoreApplyName), "a named preset sets the name");
    len = meshcore_apply_build(&named, MeshCoreApplyName, out, sizeof(out));
    CHECK_EQ_U32(len, 1 + 7, "name payload length");
    CHECK_EQ_U32(out[0], MC_CMD_SET_ADVERT_NAME, "name command code");
    CHECK(memcmp(out + 1, "ROVER-1", 7) == 0, "name bytes");

    /* TX power: skipped unless the preset carries one; a u32 dBm otherwise. */
    CHECK(
        !meshcore_apply_step_applies(&preset, MeshCoreApplyTxPower),
        "no tx power means no tx step");
    MeshCorePreset txp = sample_preset(false);
    txp.has_tx_power = true;
    txp.tx_power = 20;
    CHECK(meshcore_apply_step_applies(&txp, MeshCoreApplyTxPower), "a preset with tx power sets it");
    len = meshcore_apply_build(&txp, MeshCoreApplyTxPower, out, sizeof(out));
    CHECK_EQ_U32(len, 5, "tx power payload length");
    CHECK_EQ_U32(out[0], MC_CMD_SET_TX_POWER, "tx power command code");
    CHECK_EQ_U32(
        (uint32_t)out[1] | ((uint32_t)out[2] << 8) | ((uint32_t)out[3] << 16) |
            ((uint32_t)out[4] << 24),
        20,
        "tx power dBm little-endian");

    /* A buffer too small must refuse rather than write a short command. */
    uint8_t tiny[2];
    CHECK_EQ_U32(
        meshcore_apply_build(&preset, MeshCoreApplyPathHash, tiny, sizeof(tiny)),
        0,
        "an undersized buffer is refused");
}

static void test_apply_verify(void) {
    section("apply: judging the read-back");

    MeshCorePreset preset = sample_preset(true);

    mc_self_info_t info;
    memset(&info, 0, sizeof(info));
    info.radio_freq = 868731;
    info.radio_bw = 62500;
    info.radio_sf = 7;
    info.radio_cr = 7;
    snprintf(info.name, sizeof(info.name), "ROVER-1");

    CHECK(meshcore_apply_verify_radio(&preset, &info), "matching radio settings verify");
    CHECK(meshcore_apply_verify_name(&preset, &info), "matching name verifies");

    /* Each field on its own has to be able to fail, or the tick means nothing. */
    info.radio_sf = 10;
    CHECK(!meshcore_apply_verify_radio(&preset, &info), "a wrong spreading factor fails");
    info.radio_sf = 7;
    info.radio_bw = 250000;
    CHECK(!meshcore_apply_verify_radio(&preset, &info), "a wrong bandwidth fails");
    info.radio_bw = 62500;

    snprintf(info.name, sizeof(info.name), "SOMETHING-ELSE");
    CHECK(!meshcore_apply_verify_name(&preset, &info), "a wrong name fails");

    /* A preset with no name asks nothing, so it cannot fail. */
    MeshCorePreset nameless = sample_preset(false);
    CHECK(meshcore_apply_verify_name(&nameless, &info), "an unset name is not a failure");

    /* TX power: matches SELF_INFO.tx_power; an unset one asks nothing. */
    MeshCorePreset txp = sample_preset(false);
    txp.has_tx_power = true;
    txp.tx_power = 20;
    info.tx_power = 20;
    CHECK(meshcore_apply_verify_tx(&txp, &info), "matching tx power verifies");
    info.tx_power = 14;
    CHECK(!meshcore_apply_verify_tx(&txp, &info), "a wrong tx power fails");
    CHECK(meshcore_apply_verify_tx(&nameless, &info), "an unset tx power is not a failure");

    /* Path hash: reported only on fw_ver >= 10, so an old node must read as
     * unknown rather than as wrong. */
    mc_device_info_t device;
    memset(&device, 0, sizeof(device));
    bool checkable = true;
    device.have_path_hash = 0;
    CHECK(!meshcore_apply_verify_path_hash(&preset, &device, &checkable), "old firmware: no result");
    CHECK(!checkable, "and it is flagged as uncheckable, not as a failure");

    device.have_path_hash = 1;
    device.path_hash_mode = 1; /* 2 bytes */
    CHECK(
        meshcore_apply_verify_path_hash(&preset, &device, &checkable),
        "a matching path hash verifies");
    CHECK(checkable, "and is flagged as checkable");

    device.path_hash_mode = 0;
    CHECK(!meshcore_apply_verify_path_hash(&preset, &device, &checkable), "a wrong mode fails");
}

/* ======================================================================== */
/* ---- telemetry.csv ---- */

/* Count the fields in a CSV row, so a dropped or added comma is caught by the
 * shape of the line rather than by someone reading it in a field. */
static size_t csv_fields(const char* line) {
    size_t n = 1;
    for(const char* p = line; *p != '\0'; p++) {
        if(*p == ',') n++;
    }
    return n;
}

/* The header and the row have to agree, or every column after the first
 * mismatch is silently reading someone else's data. */
static void check_row_matches_header(const char* row, const char* header, const char* label) {
    CHECK(
        csv_fields(row) == csv_fields(header),
        "%s: row has %u fields, header has %u",
        label,
        (unsigned)csv_fields(row),
        (unsigned)csv_fields(header));
}

static void test_telemetry_full(void) {
    section("telemetry: a node that answers everything");

    MeshCoreTelemetry t;
    memset(&t, 0, sizeof(t));
    t.have_battery = true;
    t.battery_mv = 3947;
    t.have_core = true;
    t.uptime_secs = 12345;
    t.have_radio = true;
    t.noise_floor = -107;
    t.last_rssi = -92;
    t.last_snr_q4 = 26;
    t.tx_air_secs = 11;
    t.rx_air_secs = 22;
    t.have_packets = true;
    t.recv = 500;
    t.sent = 120;
    t.flood_tx = 60;
    t.direct_tx = 60;
    t.flood_rx = 250;
    t.direct_rx = 250;
    t.recv_errors = 3;
    t.has_recv_errors = true;

    char row[512];
    meshcore_telemetry_format(&t, "2026-07-23T18:00:00", "55.751000", "37.618000", row, sizeof(row));

    /* The tags the logger appends are not part of this function's output, so
     * compare against the header minus its three trailing columns. */
    check_row_matches_header(row, "ts,batt_pct,voltage,noise_floor,rx_total,tx_total,"
                                  "recv_errors,lat,lon,acc,raw_bat,raw_radio,raw_pkts,raw_core",
                             "full row");

    CHECK(strstr(row, ",3947,") != NULL, "batt_pct carries millivolts, as meshlog.py did");
    CHECK(strstr(row, ",3.947,") != NULL, "voltage is the same number in volts");
    CHECK(strstr(row, ",-107,") != NULL, "noise floor");
    CHECK(strstr(row, ",500,120,3,") != NULL, "rx/tx totals and recv_errors");
    CHECK(strstr(row, "mv=3947") != NULL, "raw_bat");
    CHECK(strstr(row, "nf=-107;rssi=-92;snr_q4=26;tx_air=11;rx_air=22") != NULL, "raw_radio");
    CHECK(strstr(row, "recv=500;sent=120;ftx=60;dtx=60;frx=250;drx=250") != NULL, "raw_pkts");
    /* raw_core: collected before, now actually written. up=uptime, q=queue,
     * cerr=CORE error count (both zero here from the memset). */
    CHECK(strstr(row, "up=12345;q=0;cerr=0") != NULL, "raw_core carries what was silently dropped");
}

static void test_telemetry_missing(void) {
    section("telemetry: a silent field is empty, never zero");

    MeshCoreTelemetry t;
    memset(&t, 0, sizeof(t));

    char row[512];
    meshcore_telemetry_format(&t, "2026-07-23T18:00:00", "", "", row, sizeof(row));

    check_row_matches_header(row, "ts,batt_pct,voltage,noise_floor,rx_total,tx_total,"
                                  "recv_errors,lat,lon,acc,raw_bat,raw_radio,raw_pkts,raw_core",
                             "empty row");

    /* On a discharge curve a missing sample and a real zero mean opposite
     * things, so nothing may be invented. */
    CHECK(strstr(row, ",0,") == NULL, "no field was filled in with a zero");
    CHECK_EQ_STR(row, "2026-07-23T18:00:00,,,,,,,,,,,,,", "everything else blank");
}

static void test_telemetry_old_firmware(void) {
    section("telemetry: recv_errors only exists on firmware 1.12+");

    MeshCoreTelemetry t;
    memset(&t, 0, sizeof(t));
    t.have_packets = true;
    t.recv = 7;
    t.sent = 2;
    t.recv_errors = 0;
    t.has_recv_errors = false;

    char row[512];
    meshcore_telemetry_format(&t, "T", "", "", row, sizeof(row));

    /* An older node must not be made to look like it reported zero errors.
     * raw_core is empty here — have_core is false, so no trailing CORE data. */
    CHECK_EQ_STR(
        row, "T,,,,7,2,,,,,,,recv=7;sent=2;ftx=0;dtx=0;frx=0;drx=0,", "errors left empty");
}

/* ---- ping.csv ---- */

static void test_ping_targets(void) {
    section("ping: the target table");

    MeshCorePing ping;
    meshcore_ping_init(&ping);

    CHECK(meshcore_ping_next(&ping) == NULL, "no targets, nothing to ping");

    CHECK(meshcore_ping_add(&ping, "BASE"), "first target");
    CHECK(!meshcore_ping_add(&ping, "BASE"), "duplicates are refused");
    CHECK(!meshcore_ping_add(&ping, ""), "an empty name is not a target");
    CHECK(meshcore_ping_add(&ping, "REP1"), "second target");

    /* Round robin, so no single node monopolises the air. */
    CHECK_EQ_STR(meshcore_ping_next(&ping)->name, "BASE", "first turn");
    CHECK_EQ_STR(meshcore_ping_next(&ping)->name, "REP1", "second turn");
    CHECK_EQ_STR(meshcore_ping_next(&ping)->name, "BASE", "wraps around");

    while(meshcore_ping_add(&ping, "FILLER1") || meshcore_ping_add(&ping, "FILLER2") ||
          meshcore_ping_add(&ping, "FILLER3")) {
    }
    CHECK_EQ_U32(ping.count, MESHCORE_PING_MAX_TARGETS, "table stops at its limit");
}

static void test_ping_resolve(void) {
    section("ping: a target is only pingable once its key arrives");

    MeshCorePing ping;
    meshcore_ping_init(&ping);
    meshcore_ping_add(&ping, "BASE");

    CHECK(!ping.targets[0].known, "unknown until an advert is heard");

    uint8_t key[32];
    memset(key, 0xAB, sizeof(key));
    CHECK(!meshcore_ping_resolve(&ping, "NOBODY", key), "resolving a non-target fails");
    CHECK(meshcore_ping_resolve(&ping, "BASE", key), "resolving a target succeeds");
    CHECK(ping.targets[0].known, "now pingable");
    CHECK(memcmp(ping.targets[0].pubkey, key, 32) == 0, "key stored whole");
}

static void test_ping_roundtrip(void) {
    section("ping: matching the ack that closes a measurement");

    MeshCorePing ping;
    meshcore_ping_init(&ping);
    meshcore_ping_add(&ping, "BASE");

    uint32_t rtt = 0;
    CHECK(!meshcore_ping_confirm(&ping, 0x1234, 100, &rtt), "no ack matches when none is pending");

    meshcore_ping_started(&ping, 0, 1, 0xDEADBEEF, 1000);
    CHECK(ping.in_flight, "armed");

    CHECK(!meshcore_ping_confirm(&ping, 0x0BADC0DE, 1050, &rtt), "someone else's ack is ignored");
    CHECK(ping.in_flight, "and leaves ours pending");

    CHECK(meshcore_ping_confirm(&ping, 0xDEADBEEF, 1050, &rtt), "our ack matches");
    CHECK_EQ_U32(rtt, 50, "round trip");
    CHECK(!ping.in_flight, "slot freed");

    CHECK(!meshcore_ping_confirm(&ping, 0xDEADBEEF, 1100, &rtt), "the same ack twice is ignored");
}

static void test_ping_tick_wrap(void) {
    section("ping: a tick counter that wraps mid-flight");

    MeshCorePing ping;
    meshcore_ping_init(&ping);
    meshcore_ping_add(&ping, "BASE");

    /* Sent just before the counter wraps, acked just after. Signed arithmetic
     * here would produce a round trip of about seven weeks. */
    meshcore_ping_started(&ping, 0, 1, 0x55, 0xFFFFFFF0u);
    uint32_t rtt = 0;
    CHECK(meshcore_ping_confirm(&ping, 0x55, 0x00000010u, &rtt), "ack still matches");
    CHECK_EQ_U32(rtt, 32, "elapsed time survives the wrap");
}

static void test_ping_parse_ack(void) {
    section("ping: reading the ack tag off the wire");

    uint32_t code = 0;

    /* Four payload bytes after the code byte is the short form, and it is what
     * the reference Python client accepts. meshcore_c demands eight and drops
     * this, which is why the parsing is ours. */
    const uint8_t short_form[] = {0x82, 0xF0, 0xE7, 0x11, 0x8E};
    CHECK(meshcore_ping_parse_ack(short_form, sizeof(short_form), &code), "short form accepted");
    CHECK_EQ_U32(code, 0x8E11E7F0u, "tag read little endian");

    /* The long form appends a round trip. The tag is in the same place and
     * everything after it is ignored. */
    const uint8_t long_form[] = {0x82, 0xF0, 0xE7, 0x11, 0x8E, 0x2C, 0x01, 0x00, 0x00};
    code = 0;
    CHECK(meshcore_ping_parse_ack(long_form, sizeof(long_form), &code), "long form accepted");
    CHECK_EQ_U32(code, 0x8E11E7F0u, "same tag, trailing bytes ignored");

    /* A tag one byte short is not a tag. Accepting it would match a ping
     * against three bytes of a real code and one byte of nothing. */
    CHECK(!meshcore_ping_parse_ack(short_form, 4, &code), "truncated payload rejected");

    const uint8_t other_push[] = {0x88, 0x01, 0x02, 0x03, 0x04};
    CHECK(!meshcore_ping_parse_ack(other_push, sizeof(other_push), &code), "another push rejected");

    CHECK(!meshcore_ping_parse_ack(NULL, 5, &code), "NULL rejected");

    /* The tag SENT hands out and the tag the ack carries must round-trip, or
     * every ping is a miss -- which is exactly what happened on the device. */
    MeshCorePing ping;
    meshcore_ping_init(&ping);
    meshcore_ping_add(&ping, "BASE");
    meshcore_ping_started(&ping, 0, 1, 0x8E11E7F0u, 1000);

    CHECK(meshcore_ping_parse_ack(short_form, sizeof(short_form), &code), "parse for the match");
    uint32_t rtt = 0;
    CHECK(meshcore_ping_confirm(&ping, code, 1354, &rtt), "the parsed tag closes the ping");
    CHECK_EQ_U32(rtt, 354, "round trip");
}

static void test_ping_timeout(void) {
    section("ping: giving up");

    MeshCorePing ping;
    meshcore_ping_init(&ping);
    meshcore_ping_add(&ping, "BASE");

    CHECK(!meshcore_ping_timeout(&ping), "nothing to give up on");
    meshcore_ping_started(&ping, 0, 1, 0x55, 1000);
    CHECK(meshcore_ping_timeout(&ping), "gives up on the outstanding one");
    CHECK(!meshcore_ping_timeout(&ping), "and only once");
}

static void test_ping_format(void) {
    section("ping: the row");

    char row[256];

    meshcore_ping_format("T", "BASE", 7, true, 143, "55.751000", "37.618000", row, sizeof(row));
    CHECK_EQ_STR(row, "T,BASE,7,1,143,55.751000,37.618000,", "a hit");
    check_row_matches_header(row, "ts,target,seq,ok,rtt_ms,lat,lon,acc", "hit row");

    /* Zero is a round trip that happened impossibly fast, not one that never
     * happened — a miss has to leave the column empty. */
    meshcore_ping_format("T", "BASE", 8, false, 0, "", "", row, sizeof(row));
    CHECK_EQ_STR(row, "T,BASE,8,0,,,,", "a miss");
    check_row_matches_header(row, "ts,target,seq,ok,rtt_ms,lat,lon,acc", "miss row");
}

/* ---- events.csv ---- */

static void test_events_format(void) {
    section("events: rows and kinds");

    char row[256];
    const char* header = "ts,type,info,lat,lon,acc,raw";

    meshcore_event_format(MeshCoreEventAdvert, "T", "BASE", "a1b2c3", "1.0", "2.0", row, sizeof(row));
    CHECK_EQ_STR(row, "T,advert,BASE,1.0,2.0,,a1b2c3", "advert");
    check_row_matches_header(row, header, "advert row");

    meshcore_event_format(MeshCoreEventMessage, "T", "hello", "", "", "", row, sizeof(row));
    CHECK_EQ_STR(row, "T,msg,hello,,,,", "message");
    check_row_matches_header(row, header, "message row");

    meshcore_event_format(MeshCoreEventMark, "T", "3", "", "", "", row, sizeof(row));
    CHECK_EQ_STR(row, "T,mark,3,,,,", "point mark");

    /* NULL is what an absent field looks like at the call site. */
    meshcore_event_format(MeshCoreEventMark, "T", NULL, NULL, "", "", row, sizeof(row));
    CHECK_EQ_STR(row, "T,mark,,,,,", "NULL info and raw render empty");
}

static void test_events_key_prefix(void) {
    section("events: identifying a neighbour with no name");

    uint8_t key[32];
    for(size_t i = 0; i < sizeof(key); i++) key[i] = (uint8_t)(i * 17);

    char out[24];
    meshcore_event_key_prefix(key, 6, out, sizeof(out));
    CHECK_EQ_STR(out, "001122334455", "six bytes of key, lowercase hex");

    /* A buffer too small must truncate cleanly rather than run off the end. */
    char small[5];
    meshcore_event_key_prefix(key, 6, small, sizeof(small));
    CHECK(strlen(small) < sizeof(small), "truncated output still terminates");
}

static void test_messages_encode(void) {
    section("messages: serialize to a line and back for the SD history");

    MeshCoreMessage direct;
    memset(&direct, 0, sizeof(direct));
    const uint8_t key[MESHCORE_PEER_LEN] = {0xAB, 0x01, 0x02, 0x03, 0x04, 0x05};
    memcpy(direct.peer, key, MESHCORE_PEER_LEN);
    direct.direction = MeshCoreMessageIncoming;
    direct.timestamp = 1721800000u;
    direct.snr_q4 = -20;
    direct.path_len = 3;
    /* A comma and a newline in the text must survive the round trip as data,
     * not break the line format. */
    snprintf(direct.text, sizeof(direct.text), "hi, mom\nsecond line");

    char line[256];
    size_t n = meshcore_message_encode(&direct, line, sizeof(line));
    CHECK(n > 0, "encode writes a line");
    CHECK(strchr(line, '\n') == NULL, "newline flattened so one message is one line");

    MeshCoreMessage back;
    CHECK(meshcore_message_decode(line, &back), "the line decodes");
    CHECK(memcmp(back.peer, key, MESHCORE_PEER_LEN) == 0, "peer key survives");
    CHECK_EQ_U32(back.direction, MeshCoreMessageIncoming, "direction");
    CHECK_EQ_U32(back.timestamp, 1721800000u, "timestamp");
    CHECK(back.snr_q4 == -20, "snr survives");
    CHECK_EQ_U32(back.path_len, 3, "path length");
    CHECK(!back.is_channel, "a direct message stays direct");
    CHECK_EQ_STR(back.text, "hi, mom second line", "text (comma kept, newline flattened)");

    /* Channel message: idx matters, peer is unused. */
    MeshCoreMessage chan;
    memset(&chan, 0, sizeof(chan));
    chan.is_channel = true;
    chan.channel_idx = 2;
    chan.direction = MeshCoreMessageOutgoing;
    snprintf(chan.text, sizeof(chan.text), "group hello");
    n = meshcore_message_encode(&chan, line, sizeof(line));
    CHECK(n > 0, "channel message encodes");
    CHECK(meshcore_message_decode(line, &back), "channel line decodes");
    CHECK(back.is_channel && back.channel_idx == 2, "channel idx survives");
    CHECK_EQ_U32(back.direction, MeshCoreMessageOutgoing, "outgoing direction");
    CHECK_EQ_STR(back.text, "group hello", "channel text");

    /* Garbage must be refused, not half-parsed. */
    CHECK(!meshcore_message_decode("not a message line", &back), "garbage rejected");
    CHECK(!meshcore_message_decode("0,0,0,zz0102030405,1,0,0,x", &back), "bad hex peer rejected");
}

static void test_contact_uri(void) {
    section("contact uri: parse a meshcore://contact/add share link");

    mc_contact_t c;
    const char* key_hex =
        "9cd8fcf22a47333b591d96a2b848b73f457b1bb1a3ea2453a885f9e5787765b1";

    /* The documented example, verbatim. */
    char uri[256];
    snprintf(
        uri,
        sizeof(uri),
        "meshcore://contact/add?name=Example+Contact&public_key=%s&type=1",
        key_hex);
    CHECK(meshcore_contact_uri_parse(uri, &c), "the documented example parses");
    CHECK_EQ_STR(c.adv_name, "Example Contact", "'+' decodes to a space");
    CHECK_EQ_U32(c.type, 1, "type");
    CHECK_EQ_U32(c.out_path_len, 0xFF, "new contact floods to find a path");
    CHECK(c.public_key[0] == 0x9c && c.public_key[1] == 0xd8 && c.public_key[31] == 0xb1,
          "public key decoded from hex");

    /* Params in any order, and percent-encoding in the name. */
    snprintf(
        uri, sizeof(uri), "meshcore://contact/add?type=2&public_key=%s&name=A%%20B%%2BC", key_hex);
    CHECK(meshcore_contact_uri_parse(uri, &c), "reordered params parse");
    CHECK_EQ_STR(c.adv_name, "A B+C", "%20 and %2B decode");
    CHECK_EQ_U32(c.type, 2, "type from reordered link");

    /* An unknown param (region_scope) is ignored, name is optional. */
    snprintf(
        uri, sizeof(uri), "meshcore://contact/add?public_key=%s&type=3&region_scope=eu", key_hex);
    CHECK(meshcore_contact_uri_parse(uri, &c), "unknown param ignored, name optional");
    CHECK_EQ_STR(c.adv_name, "", "no name yields an empty name");
    CHECK_EQ_U32(c.type, 3, "type");

    /* Rejections: wrong scheme, missing key, bad key length, non-hex, bad type. */
    CHECK(!meshcore_contact_uri_parse("http://example.com/", &c), "wrong scheme rejected");
    CHECK(
        !meshcore_contact_uri_parse("meshcore://contact/add?type=1", &c),
        "missing public_key rejected");

    char bad[256];
    snprintf(bad, sizeof(bad), "meshcore://contact/add?public_key=%s&type=1", "abcd");
    CHECK(!meshcore_contact_uri_parse(bad, &c), "short public_key rejected");

    /* 64 chars but one is not hex. */
    char nothex[128] = "meshcore://contact/add?public_key=";
    for(int i = 0; i < 63; i++) strcat(nothex, "a");
    strcat(nothex, "g&type=1");
    CHECK(!meshcore_contact_uri_parse(nothex, &c), "non-hex public_key rejected");

    snprintf(bad, sizeof(bad), "meshcore://contact/add?public_key=%s&type=0", key_hex);
    CHECK(!meshcore_contact_uri_parse(bad, &c), "type 0 rejected");
    snprintf(bad, sizeof(bad), "meshcore://contact/add?public_key=%s&type=5", key_hex);
    CHECK(!meshcore_contact_uri_parse(bad, &c), "type 5 rejected");
}

static void test_channel_uri(void) {
    section("channel uri: parse a meshcore://channel/add share link");

    char name[MC_NAME_LEN + 1];
    uint8_t secret[MC_SECRET_LEN];
    const char* secret_hex = "0123456789abcdef0123456789abcdef"; /* 32 hex = 16 bytes */

    char uri[256];
    snprintf(
        uri,
        sizeof(uri),
        "meshcore://channel/add?name=River+Trip&secret=%s&region_scope=eu",
        secret_hex);
    CHECK(
        meshcore_channel_uri_parse(uri, name, sizeof(name), secret),
        "a channel link parses");
    CHECK_EQ_STR(name, "River Trip", "channel name decodes");
    CHECK(secret[0] == 0x01 && secret[1] == 0x23 && secret[15] == 0xef, "secret decoded from hex");

    /* Secret is mandatory and must be exactly 32 hex chars. */
    CHECK(
        !meshcore_channel_uri_parse(
            "meshcore://channel/add?name=x", name, sizeof(name), secret),
        "missing secret rejected");
    snprintf(uri, sizeof(uri), "meshcore://channel/add?secret=%s", "abcd");
    CHECK(!meshcore_channel_uri_parse(uri, name, sizeof(name), secret), "short secret rejected");

    /* A contact link is not a channel link. */
    CHECK(
        !meshcore_channel_uri_parse(
            "meshcore://contact/add?public_key=x&type=1", name, sizeof(name), secret),
        "contact link rejected by channel parser");
}

int main(void) {
    printf("MeshCore Config — host protocol tests\n");
    printf("library version %s\n", MESHCORE_COMPANION_VERSION);

    test_frame_lead_byte();
    test_frame_roundtrip();
    test_frame_fragmented();
    test_frame_two_in_one_read();
    test_frame_resync_after_garbage();

    test_cmd_app_start();
    test_cmd_set_radio_params();
    test_cmd_misc();

    test_parse_self_info();
    test_parse_device_info();
    test_parse_contact();

    test_link_send();
    test_link_request_ok();
    test_link_request_skips_push();
    test_link_request_error();
    test_link_request_timeout();

    test_code_set();
    test_route();

    test_messages_from_event();
    test_messages_ring();
    test_messages_peer_filter();
    test_messages_encode();
    test_contact_uri();
    test_channel_uri();

    test_parse_scaled();
    test_json_get();
    test_preset_builtin();
    test_preset_from_json();

    test_apply_build();
    test_apply_verify();

    test_rxlog_parse();
    test_rxlog_format();
    test_rxlog_grade();

    test_contacts_collect();
    test_contacts_overflow();
    test_contacts_sort();
    test_contacts_age();

    test_telemetry_full();
    test_telemetry_missing();
    test_telemetry_old_firmware();

    test_ping_targets();
    test_ping_resolve();
    test_ping_roundtrip();
    test_ping_tick_wrap();
    test_ping_parse_ack();
    test_ping_timeout();
    test_ping_format();

    test_events_format();
    test_events_key_prefix();

    printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
