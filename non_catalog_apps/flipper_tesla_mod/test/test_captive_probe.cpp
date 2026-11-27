/*
 * test_captive_probe.cpp — host unit tests for the captive-portal decision
 * logic in esp32/.firmware/captive_probe.h (client table + probe classification).
 *
 * The header has no Arduino dependencies, so the "who gets Apple's Success page"
 * rule is checked here; captive_portal.cpp only feeds it the request's IP, path
 * and User-Agent.
 *
 * Build + run:  make -C test check
 */

#include <initializer_list>
#include <stdio.h>
#include <string.h>

#include "captive_probe.h"

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

// User-Agents as sent by the respective probes / browsers.
static const char UA_APPLE_PROBE[] = "CaptiveNetworkSupport-355.200.27 wispr";
static const char UA_IOS_SHEET[] =
    "Mozilla/5.0 (iPhone; CPU iPhone OS 12_0 like Mac OS X) AppleWebKit/605.1.15 "
    "(KHTML, like Gecko) Mobile/16A366";
static const char UA_ANDROID[] =
    "Dalvik/2.1.0 (Linux; U; Android 14; Pixel 8 Build/UQ1A.240105.004)";
static const char UA_WINDOWS[] = "Microsoft NCSI";

static const char* const APPLE_PATHS[] = {"/hotspot-detect.html", "/library/test/success.html"};
static const char* const OTHER_PATHS[] = {
    "/generate_204",
    "/gen_204", // Android
    "/ncsi.txt",
    "/connecttest.txt",
    "/redirect", // Windows
    "/success.txt",
    "/canonical.html", // Firefox
    "/",
    "/api/status",
    "/favicon.ico",
};

// Host headers of requests our DNS catch-all pulled in, vs. the portal's own.
static const char AP_IP[] = "192.168.4.1";
static const char* const FOREIGN_HOSTS[] = {
    "captive.apple.com",
    "www.example.com",
    "connectivitycheck.gstatic.com",
    "192.168.1.1",
    "192.168.4.10",
    "192.168.4.1:8080",
    "192.168.4.1.example.com",
};
static const char* const OWN_HOSTS[] = {
    "192.168.4.1",
    "192.168.4.1:80",
    " 192.168.4.1",
    "192.168.4.1:80 \t",
    "",
    "  ",
    nullptr,
};
static const char* const STRAY_PATHS[] = {"/anything", "/favicon.ico", "/v1/sync", "/index.html"};

// SoftAP client address 192.168.4.<n>; the table only compares for equality.
static uint32_t ap_client(uint8_t n) {
    return 192u | (168u << 8) | (4u << 16) | ((uint32_t)n << 24);
}

static void test_unknown_client_is_redirected() {
    CaptiveClients clients;
    uint32_t phone = ap_client(2);
    CHECK(!clients.known(phone), "fresh table knows nobody");
    for(const char* path : APPLE_PATHS) {
        CHECK(
            captive_probe_reply(clients, phone, path, UA_APPLE_PROBE) == CAPTIVE_REPLY_REDIRECT,
            "unknown client, %s",
            path);
        CHECK(
            captive_probe_reply(clients, phone, path, UA_IOS_SHEET) == CAPTIVE_REPLY_REDIRECT,
            "unknown client (sheet UA), %s",
            path);
    }
    CHECK(
        captive_probe_reply(clients, phone, "/anything", UA_APPLE_PROBE) == CAPTIVE_REPLY_REDIRECT,
        "unknown client, Apple UA on another path");
}

static void test_known_client_gets_success() {
    CaptiveClients clients;
    uint32_t phone = ap_client(2);
    clients.note(true, phone, UA_IOS_SHEET);
    CHECK(clients.known(phone), "dashboard load recorded");
    for(const char* path : APPLE_PATHS) {
        CHECK(
            captive_probe_reply(clients, phone, path, UA_APPLE_PROBE) ==
                CAPTIVE_REPLY_APPLE_SUCCESS,
            "known client, %s",
            path);
        // The sheet / Safari fetch the same URL with a browser UA, and
        // User-Agent may not have been collected at all.
        CHECK(
            captive_probe_reply(clients, phone, path, UA_IOS_SHEET) == CAPTIVE_REPLY_APPLE_SUCCESS,
            "known client (sheet UA), %s",
            path);
        CHECK(
            captive_probe_reply(clients, phone, path, nullptr) == CAPTIVE_REPLY_APPLE_SUCCESS,
            "known client (no UA), %s",
            path);
    }
    CHECK(
        captive_probe_reply(clients, phone, "/anything", UA_APPLE_PROBE) ==
            CAPTIVE_REPLY_APPLE_SUCCESS,
        "known client, Apple UA on an arbitrary path");
    CHECK(
        captive_probe_reply(clients, phone, nullptr, UA_APPLE_PROBE) ==
            CAPTIVE_REPLY_APPLE_SUCCESS,
        "known client, Apple UA, no path");

    // Success is per client: the phone's dashboard load says nothing about others.
    uint32_t laptop = ap_client(3);
    CHECK(
        captive_probe_reply(clients, laptop, "/hotspot-detect.html", UA_APPLE_PROBE) ==
            CAPTIVE_REPLY_REDIRECT,
        "another client is still redirected");
}

static void test_other_probes_unaffected() {
    CaptiveClients clients;
    uint32_t phone = ap_client(2);
    clients.note(true, phone, UA_ANDROID);
    CHECK(clients.known(phone), "client recorded");
    for(const char* path : OTHER_PATHS) {
        for(const char* ua : {UA_ANDROID, UA_WINDOWS, UA_IOS_SHEET, "", (const char*)nullptr}) {
            CHECK(
                captive_probe_reply(clients, phone, path, ua) == CAPTIVE_REPLY_REDIRECT,
                "known client, %s, UA \"%s\"",
                path,
                ua ? ua : "(null)");
        }
    }
    CHECK(
        captive_probe_reply(clients, phone, nullptr, nullptr) == CAPTIVE_REPLY_REDIRECT,
        "no path, no UA");
    // Exact-path match only.
    CHECK(
        captive_probe_reply(clients, phone, "/hotspot-detect.htmlx", UA_ANDROID) ==
            CAPTIVE_REPLY_REDIRECT,
        "longer path is not the Apple probe");
    CHECK(
        captive_probe_reply(clients, phone, "/hotspot-detect", UA_ANDROID) ==
            CAPTIVE_REPLY_REDIRECT,
        "shorter path is not the Apple probe");
}

static void test_table_wraps() {
    CaptiveClients clients;
    for(uint8_t n = 1; n <= 8; n++)
        clients.note(true, ap_client(n), UA_IOS_SHEET);
    for(uint8_t n = 1; n <= 8; n++)
        CHECK(clients.known(ap_client(n)), "8 clients fit: .%d", n);

    clients.note(true, ap_client(9), UA_IOS_SHEET); // 9th overwrites the oldest
    CHECK(!clients.known(ap_client(1)), "oldest entry overwritten");
    for(uint8_t n = 2; n <= 9; n++)
        CHECK(clients.known(ap_client(n)), "kept after wrap: .%d", n);

    clients.note(true, ap_client(10), UA_IOS_SHEET);
    CHECK(!clients.known(ap_client(2)), "second-oldest overwritten next");
    for(uint8_t n = 3; n <= 10; n++)
        CHECK(clients.known(ap_client(n)), "kept after 2nd wrap: .%d", n);

    // An overwritten client is back to the redirect, and can be recorded again.
    CHECK(
        captive_probe_reply(clients, ap_client(1), "/hotspot-detect.html", UA_APPLE_PROBE) ==
            CAPTIVE_REPLY_REDIRECT,
        "overwritten client is redirected again");
    clients.note(true, ap_client(1), UA_IOS_SHEET);
    CHECK(clients.known(ap_client(1)), "overwritten client re-recorded");
    CHECK(!clients.known(ap_client(3)), "re-recording took the next-oldest slot");

    // Many wraps: always exactly the last 8 distinct clients.
    CaptiveClients many;
    for(uint8_t n = 1; n <= 200; n++)
        many.note(true, ap_client(n), nullptr);
    for(uint8_t n = 1; n <= 192; n++)
        CHECK(!many.known(ap_client(n)), "old client forgotten: .%d", n);
    for(uint8_t n = 193; n <= 200; n++)
        CHECK(many.known(ap_client(n)), "recent client kept: .%d", n);
}

static void test_duplicate_notes_use_one_slot() {
    CaptiveClients clients;
    uint32_t first = ap_client(1);
    // The same client reloads the dashboard between every other client's visit.
    clients.note(true, first, UA_IOS_SHEET);
    for(uint8_t n = 2; n <= 8; n++) {
        clients.note(true, ap_client(n), UA_IOS_SHEET);
        clients.note(true, first, UA_IOS_SHEET);
    }
    for(uint8_t n = 1; n <= 8; n++)
        CHECK(clients.known(ap_client(n)), "reloads used one slot: .%d", n);

    // Reloads with a full table overwrite nobody.
    for(int i = 0; i < 50; i++)
        clients.note(true, first, UA_IOS_SHEET);
    for(int i = 0; i < 5; i++)
        clients.note(true, ap_client(5), UA_IOS_SHEET);
    for(uint8_t n = 1; n <= 8; n++)
        CHECK(clients.known(ap_client(n)), "full-table reload kept: .%d", n);

    // ...and did not move the overwrite position: the oldest goes first.
    clients.note(true, ap_client(9), UA_IOS_SHEET);
    CHECK(!clients.known(first), "a new client still takes the oldest slot");
    for(uint8_t n = 2; n <= 9; n++)
        CHECK(clients.known(ap_client(n)), "others kept: .%d", n);
}

static void test_inactive_portal_ignores_notes() {
    CaptiveClients clients;
    uint32_t phone = ap_client(2);
    clients.note(false, phone, UA_IOS_SHEET); // STA mode: not our clients to track
    CHECK(!clients.known(phone), "inactive portal records nothing");
    for(const char* path : APPLE_PATHS) {
        CHECK(
            captive_probe_reply(clients, phone, path, UA_APPLE_PROBE) == CAPTIVE_REPLY_REDIRECT,
            "inactive portal never answers Success, %s",
            path);
    }
    // An inactive note must not disturb entries either.
    clients.note(true, phone, UA_IOS_SHEET);
    for(uint8_t n = 10; n < 30; n++)
        clients.note(false, ap_client(n), UA_IOS_SHEET);
    CHECK(clients.known(phone), "inactive notes overwrite nothing");
}

static void test_checker_load_not_counted() {
    CaptiveClients clients;
    uint32_t phone = ap_client(2);
    // Dashboard root fetched by the connectivity checker itself (a bare-host
    // probe, or a followed redirect) is not the user seeing the dashboard.
    clients.note(true, phone, UA_APPLE_PROBE);
    CHECK(!clients.known(phone), "the checker fetching / is not a dashboard view");
    clients.note(true, phone, UA_IOS_SHEET);
    CHECK(clients.known(phone), "the sheet loading / is");

    CaptiveClients no_ua;
    no_ua.note(true, phone, nullptr);
    CHECK(no_ua.known(phone), "a load without a User-Agent still counts");
    CaptiveClients empty_ua;
    empty_ua.note(true, phone, "");
    CHECK(empty_ua.known(phone), "a load with an empty User-Agent still counts");
}

static void test_unspecified_address() {
    CaptiveClients clients;
    CHECK(!clients.known(0), "0.0.0.0 is never known (empty table)");
    clients.note(true, 0, UA_IOS_SHEET); // remoteIP() of a dead socket
    CHECK(!clients.known(0), "0.0.0.0 is never recorded");
    CHECK(
        captive_probe_reply(clients, 0, "/hotspot-detect.html", UA_APPLE_PROBE) ==
            CAPTIVE_REPLY_REDIRECT,
        "0.0.0.0 is redirected");
    // ...and takes no slot, empty table or full.
    for(uint8_t n = 1; n <= 8; n++)
        clients.note(true, ap_client(n), UA_IOS_SHEET);
    for(int i = 0; i < 3; i++)
        clients.note(true, 0, UA_IOS_SHEET);
    for(uint8_t n = 1; n <= 8; n++)
        CHECK(clients.known(ap_client(n)), "all 8 slots kept: .%d", n);
    CHECK(!clients.known(0), "0.0.0.0 still unknown with a full table");
}

static void test_ua_matching() {
    CHECK(!captive_ua_is_apple_probe(nullptr), "NULL UA");
    CHECK(!captive_ua_is_apple_probe(""), "empty UA");
    CHECK(captive_ua_is_apple_probe(UA_APPLE_PROBE), "iOS probe UA");
    CHECK(captive_ua_is_apple_probe("CaptiveNetworkSupport-491.100.3 wispr"), "newer probe build");
    CHECK(captive_ua_is_apple_probe("CaptiveNetworkSupport"), "bare token");
    CHECK(captive_ua_is_apple_probe("foo CaptiveNetworkSupport/1.0"), "token mid-string");
    CHECK(!captive_ua_is_apple_probe(UA_IOS_SHEET), "iOS captive sheet is a browser");
    CHECK(!captive_ua_is_apple_probe(UA_ANDROID), "Android");
    CHECK(!captive_ua_is_apple_probe(UA_WINDOWS), "Windows NCSI");
    CHECK(!captive_ua_is_apple_probe("CaptiveNetworkSuppor"), "truncated token");
    CHECK(!captive_ua_is_apple_probe("wispr"), "wispr alone");
}

// ── Catch-all (no route matched) ─────────────────────────────────────────────

static void test_not_found_apple_probe_from_known_client() {
    CaptiveClients clients;
    uint32_t phone = ap_client(2);
    clients.note(true, phone, UA_IOS_SHEET);
    for(const char* host : FOREIGN_HOSTS) {
        CHECK(
            captive_not_found_reply(
                clients, true, phone, "/some/probe", UA_APPLE_PROBE, host, AP_IP) ==
                CAPTIVE_REPLY_APPLE_SUCCESS,
            "known client, Apple UA, Host %s",
            host);
    }
    // The Apple check comes first, whatever Host says.
    for(const char* host : OWN_HOSTS) {
        CHECK(
            captive_not_found_reply(
                clients, true, phone, "/some/probe", UA_APPLE_PROBE, host, AP_IP) ==
                CAPTIVE_REPLY_APPLE_SUCCESS,
            "known client, Apple UA, Host \"%s\"",
            host ? host : "(null)");
    }
    // A non-GET to an Apple probe path lands here too.
    for(const char* path : APPLE_PATHS) {
        CHECK(
            captive_not_found_reply(
                clients, true, phone, path, UA_IOS_SHEET, "captive.apple.com", AP_IP) ==
                CAPTIVE_REPLY_APPLE_SUCCESS,
            "known client, %s by path",
            path);
    }
}

static void test_not_found_known_client_foreign_host() {
    CaptiveClients clients;
    uint32_t phone = ap_client(2);
    clients.note(true, phone, UA_IOS_SHEET);
    // Background traffic of a device that now routes everything through us:
    // a 404 carrying a plain link, never a redirect a client follows on its
    // own into the whole dashboard.
    for(const char* host : FOREIGN_HOSTS) {
        for(const char* path : STRAY_PATHS) {
            for(const char* ua :
                {UA_IOS_SHEET, UA_ANDROID, UA_WINDOWS, "", (const char*)nullptr}) {
                CHECK(
                    captive_not_found_reply(clients, true, phone, path, ua, host, AP_IP) ==
                        CAPTIVE_REPLY_DASHBOARD_LINK,
                    "known client, Host %s, %s",
                    host,
                    path);
            }
        }
    }
}

static void test_not_found_own_host() {
    CaptiveClients clients;
    uint32_t phone = ap_client(2), stranger = ap_client(3);
    clients.note(true, phone, UA_IOS_SHEET);
    // A missing path on the portal itself is a plain 404 for everyone.
    for(const char* host : OWN_HOSTS) {
        for(const char* path : STRAY_PATHS) {
            CHECK(
                captive_not_found_reply(clients, true, phone, path, UA_IOS_SHEET, host, AP_IP) ==
                    CAPTIVE_REPLY_NOT_FOUND,
                "known client, own Host \"%s\", %s",
                host ? host : "(null)",
                path);
            CHECK(
                captive_not_found_reply(clients, true, stranger, path, UA_ANDROID, host, AP_IP) ==
                    CAPTIVE_REPLY_NOT_FOUND,
                "unknown client, own Host \"%s\", %s",
                host ? host : "(null)",
                path);
        }
        // Apple's checker addressing the portal directly, before the dashboard
        // was loaded: not a Success, and nothing to lure either.
        CHECK(
            captive_not_found_reply(
                clients, true, stranger, "/some/probe", UA_APPLE_PROBE, host, AP_IP) ==
                CAPTIVE_REPLY_NOT_FOUND,
            "unknown client, Apple UA, own Host \"%s\"",
            host ? host : "(null)");
    }
}

static void test_not_found_unknown_client_foreign_host() {
    CaptiveClients clients;
    uint32_t phone = ap_client(2), stranger = ap_client(3);
    clients.note(true, phone, UA_IOS_SHEET);
    // The captive lure: an unknown client asking for some other site is sent
    // to the dashboard, exactly as before.
    for(const char* host : FOREIGN_HOSTS) {
        for(const char* path : STRAY_PATHS) {
            for(const char* ua :
                {UA_APPLE_PROBE, UA_IOS_SHEET, UA_ANDROID, UA_WINDOWS, "", (const char*)nullptr}) {
                CHECK(
                    captive_not_found_reply(clients, true, stranger, path, ua, host, AP_IP) ==
                        CAPTIVE_REPLY_REDIRECT,
                    "unknown client, Host %s, %s",
                    host,
                    path);
            }
        }
        for(const char* path : APPLE_PATHS) {
            CHECK(
                captive_not_found_reply(clients, true, stranger, path, UA_IOS_SHEET, host, AP_IP) ==
                    CAPTIVE_REPLY_REDIRECT,
                "unknown client, Host %s, %s",
                host,
                path);
        }
    }
}

static void test_not_found_portal_inactive() {
    // STA mode, or the DNS server never came up: there is no lure to run, so
    // an unmatched request is just a missing path.
    CaptiveClients clients;
    uint32_t phone = ap_client(2), stranger = ap_client(3);
    clients.note(false, phone, UA_IOS_SHEET);
    for(const char* host : FOREIGN_HOSTS) {
        for(const char* path : STRAY_PATHS) {
            CHECK(
                captive_not_found_reply(clients, false, stranger, path, UA_ANDROID, host, AP_IP) ==
                    CAPTIVE_REPLY_NOT_FOUND,
                "inactive, Host %s, %s",
                host,
                path);
        }
        CHECK(
            captive_not_found_reply(
                clients, false, stranger, "/some/probe", UA_APPLE_PROBE, host, AP_IP) ==
                CAPTIVE_REPLY_NOT_FOUND,
            "inactive, Apple UA, Host %s",
            host);
        for(const char* path : APPLE_PATHS) {
            CHECK(
                captive_not_found_reply(
                    clients, false, stranger, path, UA_APPLE_PROBE, host, AP_IP) ==
                    CAPTIVE_REPLY_NOT_FOUND,
                "inactive, %s, Host %s",
                path,
                host);
        }
    }
    for(const char* host : OWN_HOSTS) {
        CHECK(
            captive_not_found_reply(
                clients, false, stranger, "/anything", UA_ANDROID, host, AP_IP) ==
                CAPTIVE_REPLY_NOT_FOUND,
            "inactive, own Host \"%s\"",
            host ? host : "(null)");
    }
    // The device's LAN address as Host, AP address unset: the STA-mode request.
    CHECK(
        captive_not_found_reply(
            clients, false, stranger, "/favicon.ico", UA_WINDOWS, "192.168.1.77", "0.0.0.0") ==
            CAPTIVE_REPLY_NOT_FOUND,
        "inactive, LAN Host");

    // Clients recorded before the portal went down get nothing special either.
    CaptiveClients was_active;
    was_active.note(true, phone, UA_IOS_SHEET);
    CHECK(
        captive_not_found_reply(
            was_active, false, phone, "/some/probe", UA_APPLE_PROBE, "captive.apple.com", AP_IP) ==
            CAPTIVE_REPLY_NOT_FOUND,
        "inactive, known client, Apple UA: no Success");
    CHECK(
        captive_not_found_reply(
            was_active, false, phone, "/anything", UA_IOS_SHEET, "www.example.com", AP_IP) ==
            CAPTIVE_REPLY_NOT_FOUND,
        "inactive, known client, foreign Host: no link");
    // ...and the same requests with the portal up, for contrast.
    CHECK(
        captive_not_found_reply(
            was_active, true, phone, "/some/probe", UA_APPLE_PROBE, "captive.apple.com", AP_IP) ==
            CAPTIVE_REPLY_APPLE_SUCCESS,
        "active, known client, Apple UA");
    CHECK(
        captive_not_found_reply(
            was_active, true, phone, "/anything", UA_IOS_SHEET, "www.example.com", AP_IP) ==
            CAPTIVE_REPLY_DASHBOARD_LINK,
        "active, known client, foreign Host");
    CHECK(
        captive_not_found_reply(
            was_active, true, stranger, "/anything", UA_ANDROID, "www.example.com", AP_IP) ==
            CAPTIVE_REPLY_REDIRECT,
        "active, unknown client, foreign Host");
}

static void test_not_found_follows_ap_address() {
    // The AP address is whatever the SoftAP was given, not a constant.
    CaptiveClients clients;
    uint32_t phone = ap_client(2), stranger = ap_client(3);
    clients.note(true, phone, UA_IOS_SHEET);
    static const char other_ap[] = "10.0.0.1";
    CHECK(
        captive_not_found_reply(clients, true, stranger, "/x", UA_ANDROID, "10.0.0.1", other_ap) ==
            CAPTIVE_REPLY_NOT_FOUND,
        "unknown client, Host is the configured AP address");
    CHECK(
        captive_not_found_reply(
            clients, true, phone, "/x", UA_IOS_SHEET, "10.0.0.1:80", other_ap) ==
            CAPTIVE_REPLY_NOT_FOUND,
        "known client, Host is the configured AP address");
    CHECK(
        captive_not_found_reply(
            clients, true, stranger, "/x", UA_ANDROID, "192.168.4.1", other_ap) ==
            CAPTIVE_REPLY_REDIRECT,
        "unknown client, default AP address is foreign under another AP address");
    CHECK(
        captive_not_found_reply(
            clients, true, phone, "/x", UA_IOS_SHEET, "192.168.4.1", other_ap) ==
            CAPTIVE_REPLY_DASHBOARD_LINK,
        "known client, default AP address is foreign under another AP address");
}

static void test_host_matching() {
    CHECK(captive_host_is_ours("192.168.4.1", AP_IP), "AP address");
    CHECK(captive_host_is_ours("192.168.4.1:80", AP_IP), "AP address with the default port");
    CHECK(captive_host_is_ours("", AP_IP), "empty Host");
    CHECK(captive_host_is_ours(nullptr, AP_IP), "no Host");
    CHECK(captive_host_is_ours(nullptr, nullptr), "no Host, no AP address");
    CHECK(!captive_host_is_ours("192.168.4.10", AP_IP), "AP address is only a prefix");
    CHECK(
        !captive_host_is_ours("192.168.4.1.example.com", AP_IP),
        "AP address as a subdomain label");
    CHECK(!captive_host_is_ours("192.168.4.1:8080", AP_IP), "another port");
    CHECK(!captive_host_is_ours("192.168.4.1:8", AP_IP), "truncated port");
    CHECK(!captive_host_is_ours("192.168.4.1:", AP_IP), "empty port");
    CHECK(!captive_host_is_ours("192.168.4", AP_IP), "shorter than the AP address");
    CHECK(!captive_host_is_ours("captive.apple.com", AP_IP), "a hostname");
    CHECK(!captive_host_is_ours("192.168.4.1", "10.0.0.1"), "a different AP address");
    CHECK(!captive_host_is_ours("192.168.4.1", ""), "AP address unknown (empty)");
    CHECK(!captive_host_is_ours("192.168.4.1", nullptr), "AP address unknown (null)");
    CHECK(!captive_host_is_ours(":80", ""), "port only, AP address unknown");

    // Optional whitespace around the field value is legal HTTP, and WebServer
    // does not strip it on GET.
    CHECK(captive_host_is_ours(" 192.168.4.1", AP_IP), "leading space");
    CHECK(captive_host_is_ours("\t192.168.4.1", AP_IP), "leading tab");
    CHECK(captive_host_is_ours("192.168.4.1 ", AP_IP), "trailing space");
    CHECK(captive_host_is_ours("  192.168.4.1:80\t ", AP_IP), "whitespace around host:port");
    CHECK(captive_host_is_ours("   ", AP_IP), "whitespace only counts as no Host");
    CHECK(captive_host_is_ours("\t", nullptr), "whitespace only, AP address unknown");
    CHECK(!captive_host_is_ours("192.168.4.1 :80", AP_IP), "space before the port");
    CHECK(!captive_host_is_ours("192.168.4.1: 80", AP_IP), "space inside the port");
    CHECK(!captive_host_is_ours("192.168. 4.1", AP_IP), "space inside the address");
    CHECK(!captive_host_is_ours("192.168.4.1 x", AP_IP), "junk after the address");
    CHECK(!captive_host_is_ours("192.168.4.1:80 0", AP_IP), "junk after the port");
    CHECK(!captive_host_is_ours(" 192.168.4.10 ", AP_IP), "prefix match with whitespace");
    CHECK(!captive_host_is_ours(" captive.apple.com", AP_IP), "a hostname with whitespace");

    // Hosts compare case-insensitively. An IPv4 literal has no case, so this
    // only shows with a name.
    CHECK(captive_host_is_ours("FSD.Local", "fsd.local"), "upper-case Host");
    CHECK(captive_host_is_ours("fsd.local:80", "FSD.LOCAL"), "upper-case own name");
    CHECK(!captive_host_is_ours("fsd.locals", "fsd.local"), "longer name");
    CHECK(!captive_host_is_ours("fsd.loca", "fsd.local"), "shorter name");
}

static void test_success_body() {
    // What http://captive.apple.com/hotspot-detect.html serves: 69 bytes,
    // upper-case tags, one trailing LF. iOS compares against this page.
    static const char apple[] =
        "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>\n";
    CHECK(
        strlen(CAPTIVE_APPLE_SUCCESS_BODY) == 69,
        "body length %u",
        (unsigned)strlen(CAPTIVE_APPLE_SUCCESS_BODY));
    CHECK(strcmp(CAPTIVE_APPLE_SUCCESS_BODY, apple) == 0, "body matches Apple's");
}

int main() {
    printf("test_captive_probe: captive-portal probe/client-table host tests\n");
    test_unknown_client_is_redirected();
    test_known_client_gets_success();
    test_other_probes_unaffected();
    test_table_wraps();
    test_duplicate_notes_use_one_slot();
    test_inactive_portal_ignores_notes();
    test_checker_load_not_counted();
    test_unspecified_address();
    test_ua_matching();
    test_not_found_apple_probe_from_known_client();
    test_not_found_known_client_foreign_host();
    test_not_found_own_host();
    test_not_found_unknown_client_foreign_host();
    test_not_found_portal_inactive();
    test_not_found_follows_ap_address();
    test_host_matching();
    test_success_body();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
