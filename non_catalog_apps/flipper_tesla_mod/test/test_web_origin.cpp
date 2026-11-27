/*
 * test_web_origin.cpp — host unit tests for esp32/.firmware/web_origin.h: which
 * Origins the WebSocket and POST /api/cmd take commands from, which Host gets the dashboard page rather than a redirect, and who
 * gets the CAN stream on :82.
 *
 * Build + run:  make -C test check
 */

#include <stdio.h>
#include <string.h>

#include "web_origin.h"

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

// On its own access point, the default setup.
static const WebOwnNames AP = {"192.168.4.1", "", "tesla-fsd"};
// Joined to a home network, AP off.
static const WebOwnNames STA = {"", "192.168.1.57", "tesla-fsd"};
// A renamed device; the default name is then no longer ours.
static const WebOwnNames RENAMED = {"192.168.4.1", "", "Garage-Car"};

// Origins the dashboard page itself sends, by how it was opened.
static const char* const OWN_ORIGINS_AP[] = {
    "http://192.168.4.1", // the AP address, as the README says
    "http://192.168.4.1:80", // explicit default port
    "http://tesla-fsd", // bare hostname (captive DNS resolves it)
    "http://tesla-fsd.local",
    "http://tesla-fsd.lan",
    "http://tesla-fsd.home",
    "http://tesla-fsd.internal",
    "http://tesla-fsd.localdomain",
    "http://tesla-fsd.home.arpa",
    "http://TESLA-FSD.LAN", // case does not matter
    "HTTP://192.168.4.1",
    " http://192.168.4.1 ", // whitespace around the value
};

// Pages that are not the dashboard. Each would otherwise be able to drive the
// device from a browser on the same network.
static const char* const FOREIGN_ORIGINS[] = {
    "http://evil.example", // another site
    "http://192.168.4.1.evil.example", // our address as a prefix
    "http://tesla-fsd.evil.example", // our hostname as a label of theirs
    "http://tesla-fsd.local.evil.example",
    "http://tesla-fsd.lan.evil.example",
    "http://evil-tesla-fsd.lan",
    "http://tesla-fsdx.lan",
    "http://tesla-fsd.fritz.box", // a router domain that is a public domain
    "http://tesla-fsd.", // trailing dot
    "http://192.168.4.10", // a neighbour on the AP
    "http://192.168.4.1:8080", // another port on our address
    "http://192.168.4.1:81",
    "http://192.168.4.1:",
    "http://192.168.4.1/", // not an origin at all
    "http://192.168.4.1/index.html",
    "http://evil.example@192.168.4.1",
    "http://192.168.4.1@evil.example",
    "http://192.168.4.1 evil.example",
    "https://192.168.4.1", // the device serves no TLS
    "ws://192.168.4.1",
    "file://",
    "null",
    "chrome-extension://abcdefghijklmnop",
    "http://",
    "http:/192.168.4.1",
    "192.168.4.1",
    "http://0.0.0.0",
    "http://localhost",
    "http://127.0.0.1",
    "http://[::1]",
};

static void test_ws_own_page() {
    for(const char* o : OWN_ORIGINS_AP) {
        CHECK(web_ws_origin_ok(o, AP), "AP: own page refused: %s", o);
    }
    CHECK(web_ws_origin_ok("http://192.168.1.57", STA), "STA address");
    CHECK(web_ws_origin_ok("http://tesla-fsd.lan", STA), "hostname on the home network");
    CHECK(web_ws_origin_ok("http://garage-car.local", RENAMED), "renamed device, new name");
    CHECK(web_ws_origin_ok("http://192.168.4.1", RENAMED), "renamed device, address");
}

static void test_ws_no_origin() {
    // websocat, Python websockets, an ESP32 client: no Origin, not a page.
    CHECK(web_ws_origin_ok(nullptr, AP), "no Origin header");
    CHECK(web_ws_origin_ok("", AP), "empty Origin");
    CHECK(web_ws_origin_ok(" \t", AP), "blank Origin");
}

static void test_ws_foreign_pages() {
    for(const char* o : FOREIGN_ORIGINS) {
        CHECK(!web_ws_origin_ok(o, AP), "AP: foreign page taken: %s", o);
        CHECK(!web_ws_origin_ok(o, STA), "STA: foreign page taken: %s", o);
    }
    // An address we do not have right now is not ours.
    CHECK(!web_ws_origin_ok("http://192.168.4.1", STA), "AP address while the AP is off");
    CHECK(!web_ws_origin_ok("http://192.168.1.57", AP), "old STA address while on the AP");
    CHECK(!web_ws_origin_ok("http://tesla-fsd", RENAMED), "old hostname after a rename");
    CHECK(!web_ws_origin_ok("http://tesla-fsd.lan", RENAMED), "old hostname.lan after a rename");
    WebOwnNames none = {nullptr, "0.0.0.0", ""};
    CHECK(!web_ws_origin_ok("http://0.0.0.0", none), "0.0.0.0 is no address");
    CHECK(!web_ws_origin_ok("http://", none), "nothing to match");
    CHECK(web_ws_origin_ok(nullptr, none), "no Origin, no names");
}

// DNS rebinding: the attacker's page was loaded from attacker.example, whose
// name now resolves to the device (on the SoftAP every name does). Origin and
// Host then both say attacker.example, so they agree; neither is ours.
static void test_rebinding() {
    const char* rebound[] = {
        "evil.example", "evil.example:80", "tesla-fsd.evil.example", "192.168.4.1.nip.io"};
    char origin[96];
    for(const char* name : rebound) {
        snprintf(origin, sizeof(origin), "http://%s", name);
        CHECK(!web_ws_origin_ok(origin, AP), "WS, rebound %s", name);
        CHECK(!web_post_origin_ok(origin, AP), "POST, rebound %s", name);
        CHECK(!web_post_origin_ok(origin, STA), "POST (STA), rebound %s", name);
        CHECK(!web_host_header_ours(name, AP), "Host %s gets the redirect", name);
    }
}

static void test_post() {
    for(const char* o : OWN_ORIGINS_AP) {
        CHECK(web_post_origin_ok(o, AP), "AP: own page refused: %s", o);
    }
    for(const char* o : FOREIGN_ORIGINS) {
        CHECK(!web_post_origin_ok(o, AP), "AP: foreign page taken: %s", o);
    }
    CHECK(web_post_origin_ok("http://192.168.1.57", STA), "STA address");
    CHECK(web_post_origin_ok("http://tesla-fsd.lan", STA), "hostname on the home network");
    // curl, Home Assistant, a script: no Origin.
    CHECK(web_post_origin_ok(nullptr, STA), "no Origin header");
    CHECK(web_post_origin_ok("", STA), "empty Origin");
    CHECK(web_post_origin_ok(" \t", STA), "blank Origin");

    // Origin: null comes from sandboxed frames, data: URLs and file:// pages,
    // aimed at whatever Host they like, ours included. The dashboard never
    // sends it: refused, as on the WebSocket, in every spelling.
    const char* nulls[] = {"null", " null ", "\tnull", "NULL", "Null", "nullx", "null,null"};
    for(const char* o : nulls) {
        CHECK(!web_post_origin_ok(o, AP), "POST, Origin %s", o);
        CHECK(!web_post_origin_ok(o, STA), "POST (STA), Origin %s", o);
        CHECK(web_post_origin_ok(o, AP) == web_ws_origin_ok(o, AP), "POST and WS agree on %s", o);
    }
}

static void test_host_header() {
    const char* ours[] = {
        "192.168.4.1",
        "192.168.4.1:80",
        " 192.168.4.1 ",
        "\t192.168.4.1:80\t",
        "tesla-fsd",
        "tesla-fsd:80",
        "Tesla-FSD.Local",
        "tesla-fsd.home.arpa"};
    for(const char* h : ours)
        CHECK(web_host_header_ours(h, AP), "own Host refused: %s", h);
    // No Host at all (HTTP/1.0) counts as ours, as in captive_host_is_ours().
    CHECK(web_host_header_ours(nullptr, AP), "no Host");
    CHECK(web_host_header_ours("", AP), "empty Host");
    CHECK(web_host_header_ours("  ", AP), "blank Host");

    // Names the captive DNS pulled in, or a rebound name: redirected.
    const char* foreign[] = {
        "captive.apple.com",
        "connectivitycheck.gstatic.com",
        "neverssl.com",
        "www.msftconnecttest.com",
        "192.168.4.1:8080",
        "192.168.4.10",
        "tesla-fsd.fritz.box",
        "tesla-fsd.example.com",
        "192.168.4.1.",
        "192.168. 4.1",
        "192.168.4.1 :80",
        "192.168.4.1:080"};
    for(const char* h : foreign)
        CHECK(!web_host_header_ours(h, AP), "foreign Host served: %s", h);
    CHECK(web_host_header_ours("192.168.1.57", STA), "STA address");
    CHECK(!web_host_header_ours("192.168.4.1", STA), "AP address with the AP off");
}

static void test_local_domains() {
    // Every domain on the list works, as a whole label only.
    char origin[64];
    for(const char* d : WEB_LOCAL_DOMAINS) {
        snprintf(origin, sizeof(origin), "http://tesla-fsd.%s", d);
        CHECK(web_ws_origin_ok(origin, AP), "%s", origin);
        snprintf(origin, sizeof(origin), "http://tesla-fsd.%sx", d);
        CHECK(!web_ws_origin_ok(origin, AP), "%s", origin);
        snprintf(origin, sizeof(origin), "http://tesla-fsd.x.%s", d);
        CHECK(!web_ws_origin_ok(origin, AP), "%s", origin);
        snprintf(origin, sizeof(origin), "http://%s", d);
        CHECK(!web_ws_origin_ok(origin, AP), "%s", origin);
    }
    CHECK(!web_ws_origin_ok("http://tesla-fsd.arpa", AP), ".arpa alone");
}

// The CAN stream on :82, read by the dashboard page from :80.
static void test_stream() {
    // The dashboard page, however it was opened: served with a grant.
    for(const char* o : OWN_ORIGINS_AP) {
        CHECK(
            web_stream_access(o, "192.168.4.1:82", AP) == WEB_STREAM_SERVE_GRANT,
            "own page not granted: %s",
            o);
    }
    CHECK(
        web_stream_access("http://tesla-fsd.local", "tesla-fsd.local:82", AP) ==
            WEB_STREAM_SERVE_GRANT,
        "own page by hostname");
    CHECK(
        web_stream_access("http://192.168.1.57", "192.168.1.57:82", STA) == WEB_STREAM_SERVE_GRANT,
        "own page on the home network");

    // curl, SavvyCAN, a capture script: no Origin, served without a grant.
    CHECK(web_stream_access(nullptr, "192.168.4.1:82", AP) == WEB_STREAM_SERVE, "curl by address");
    CHECK(web_stream_access("", "tesla-fsd.lan:82", AP) == WEB_STREAM_SERVE, "curl by hostname");
    CHECK(
        web_stream_access(nullptr, "192.168.4.1", AP) == WEB_STREAM_SERVE, "Host without a port");
    CHECK(web_stream_access(nullptr, nullptr, AP) == WEB_STREAM_SERVE, "HTTP/1.0, no Host");

    // Any other page is refused, granted or not it could not read the stream.
    for(const char* o : FOREIGN_ORIGINS) {
        CHECK(
            web_stream_access(o, "192.168.4.1:82", AP) == WEB_STREAM_REFUSE,
            "foreign page: %s",
            o);
    }
    CHECK(
        web_stream_access("http://192.168.4.1:82", "192.168.4.1:82", AP) == WEB_STREAM_REFUSE,
        "a page served from :82 is not the dashboard");

    // Rebound name: same-origin to the attacker's page, which sends no Origin
    // on that GET. Only Host gives it away.
    const char* rebound[] = {
        "evil.example:82", "evil.example", "tesla-fsd.evil.example:82", "192.168.4.1.nip.io:82"};
    for(const char* h : rebound) {
        CHECK(web_stream_access(nullptr, h, AP) == WEB_STREAM_REFUSE, "rebound Host %s", h);
        CHECK(
            web_stream_access("http://192.168.4.1", h, AP) == WEB_STREAM_REFUSE,
            "own Origin, rebound Host %s",
            h);
    }
    // Other ports on our address: not the stream.
    const char* ports[] = {
        "192.168.4.1:80", "192.168.4.1:81", "192.168.4.1:8082", "192.168.4.1:", "192.168.4.1:082"};
    for(const char* h : ports) {
        CHECK(web_stream_access(nullptr, h, AP) == WEB_STREAM_REFUSE, "Host %s", h);
    }
    // The :80 rule is unchanged for the page and its API.
    CHECK(
        web_host_header_ours("192.168.4.1:80", AP) && !web_host_header_ours("192.168.4.1:82", AP),
        ":82 is no Host for the page");
    CHECK(
        web_host_header_ours("192.168.4.1:82", AP, ":82") &&
            web_host_header_ours("tesla-fsd:82", AP, ":82"),
        ":82 for the stream");
}

int main() {
    printf("test_web_origin: dashboard Origin / Host checks\n");
    test_ws_own_page();
    test_ws_no_origin();
    test_ws_foreign_pages();
    test_rebinding();
    test_post();
    test_host_header();
    test_local_domains();
    test_stream();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
