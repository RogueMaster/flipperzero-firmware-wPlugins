#include "sr_test.h"

#include "sr_types.h"
#include "sr_source_codec.h"
#include "sr_source.h"

#include <string.h>

static void test_parser_zero(void) {
    SrParser p;
    memset(&p, 0, sizeof(p));
    CHECK(p.lines_seen == 0);
    CHECK(p.lines_malformed == 0);
    CHECK(p.in_session == false);
}

static void test_scan_cfg(void) {
    SrScanCfg cfg;
    memset(&cfg, 0, sizeof(cfg));
    CHECK(cfg.mirror_to_serial == false);
    cfg.mirror_to_serial = true;
    CHECK(cfg.mirror_to_serial == true);
}

static void test_fixed_buffers(void) {
    SrApRecord ap;
    char overflow[SR_SSID_MAX + 2];
    char exact[SR_SSID_MAX + 1];
    const char* mac = "AA:BB:CC:DD:EE:FF";
    const char* auth = "[WPA2_WPA3_PSK]";
    const char* dt = "2026-08-16 12:00:00";
    size_t n;

    memset(&ap, 0, sizeof(ap));

    CHECK(sizeof(ap.bssid) == (size_t)SR_BSSID_MAX + 1U);
    CHECK(sizeof(ap.ssid) == (size_t)SR_SSID_MAX + 1U);
    CHECK(sizeof(ap.auth) == (size_t)SR_AUTH_MAX + 1U);
    CHECK(sizeof(ap.datetime) == (size_t)SR_DATETIME_MAX + 1U);
    CHECK(sizeof(ap.lat) == (size_t)SR_COORD_MAX + 1U);

    CHECK(strlen(mac) == (size_t)SR_BSSID_MAX);
    CHECK(strlen(auth) == (size_t)SR_AUTH_MAX);
    CHECK(strlen(dt) == (size_t)SR_DATETIME_MAX);

    n = sr_strlcpy(ap.bssid, sizeof(ap.bssid), mac);
    CHECK(n == (size_t)SR_BSSID_MAX);
    CHECK(ap.bssid[SR_BSSID_MAX] == '\0');
    CHECK(strcmp(ap.bssid, mac) == 0);

    n = sr_strlcpy(ap.auth, sizeof(ap.auth), auth);
    CHECK(n == (size_t)SR_AUTH_MAX);
    CHECK(ap.auth[SR_AUTH_MAX] == '\0');
    CHECK(strcmp(ap.auth, auth) == 0);

    n = sr_strlcpy(ap.datetime, sizeof(ap.datetime), dt);
    CHECK(n == (size_t)SR_DATETIME_MAX);
    CHECK(ap.datetime[SR_DATETIME_MAX] == '\0');
    CHECK(strcmp(ap.datetime, dt) == 0);

    memset(exact, 'A', (size_t)SR_SSID_MAX);
    exact[SR_SSID_MAX] = '\0';
    n = sr_strlcpy(ap.ssid, sizeof(ap.ssid), exact);
    CHECK(n == (size_t)SR_SSID_MAX);
    CHECK(strlen(ap.ssid) == (size_t)SR_SSID_MAX);
    CHECK(ap.ssid[SR_SSID_MAX] == '\0');
    CHECK(ap.ssid[0] == 'A');
    CHECK(ap.ssid[SR_SSID_MAX - 1] == 'A');

    memset(overflow, 'B', (size_t)SR_SSID_MAX + 1U);
    overflow[SR_SSID_MAX + 1] = '\0';
    n = sr_strlcpy(ap.ssid, sizeof(ap.ssid), overflow);
    CHECK(n == (size_t)SR_SSID_MAX + 1U);
    CHECK(strlen(ap.ssid) == (size_t)SR_SSID_MAX);
    CHECK(ap.ssid[SR_SSID_MAX] == '\0');
    CHECK(ap.ssid[0] == 'B');
    CHECK(ap.ssid[SR_SSID_MAX - 1] == 'B');

    n = sr_strlcpy(ap.ssid, sizeof(ap.ssid), "");
    CHECK(n == 0);
    CHECK(ap.ssid[0] == '\0');

    {
        char one[1] = {'X'};
        n = sr_strlcpy(one, sizeof(one), "Z");
        CHECK(n == 1);
        CHECK(one[0] == '\0');
    }
}

static void test_event_tags(void) {
    SrEvent ev;

    memset(&ev, 0, sizeof(ev));
    CHECK(ev.kind == SrEventNone);

    ev.kind = SrEventApFound;
    ev.u.ap.cursor = 12;
    ev.u.ap.channel = 6;
    ev.u.ap.rssi = -67;
    ev.u.ap.radio = SrRadioWifi;
    sr_strlcpy(ev.u.ap.bssid, sizeof(ev.u.ap.bssid), "AA:BB:CC:DD:EE:FF");
    sr_strlcpy(ev.u.ap.ssid, sizeof(ev.u.ap.ssid), "My_SSID");
    sr_strlcpy(ev.u.ap.auth, sizeof(ev.u.ap.auth), "[WPA2_PSK]");
    CHECK(ev.kind == SrEventApFound);
    CHECK(ev.kind != SrEventBleFound);
    CHECK(ev.u.ap.cursor == 12);
    CHECK(ev.u.ap.channel == 6);
    CHECK(ev.u.ap.rssi == -67);
    CHECK(ev.u.ap.radio == SrRadioWifi);
    CHECK(strcmp(ev.u.ap.ssid, "My_SSID") == 0);

    memset(&ev, 0, sizeof(ev));
    ev.kind = SrEventBleFound;
    ev.u.ble.cursor = 0;
    ev.u.ble.channel = 0;
    ev.u.ble.rssi = -80;
    ev.u.ble.radio = SrRadioBle;
    ev.u.ble.ssid[0] = '\0';
    sr_strlcpy(ev.u.ble.bssid, sizeof(ev.u.ble.bssid), "11:22:33:44:55:66");
    sr_strlcpy(ev.u.ble.auth, sizeof(ev.u.ble.auth), "[BLE]");
    CHECK(ev.kind == SrEventBleFound);
    CHECK(ev.u.ble.cursor == 0);
    CHECK(ev.u.ble.channel == 0);
    CHECK(ev.u.ble.radio == SrRadioBle);
    CHECK(ev.u.ble.ssid[0] == '\0');
    CHECK(strcmp(ev.u.ble.auth, "[BLE]") == 0);

    memset(&ev, 0, sizeof(ev));
    ev.kind = SrEventGps;
    ev.u.gps.fix = true;
    sr_strlcpy(ev.u.gps.sats, sizeof(ev.u.gps.sats), "8");
    sr_strlcpy(ev.u.gps.lat, sizeof(ev.u.gps.lat), "31.2300000");
    sr_strlcpy(ev.u.gps.datetime, sizeof(ev.u.gps.datetime), "2026-08-16 12:00:00");
    CHECK(ev.kind == SrEventGps);
    CHECK(ev.u.gps.fix == true);
    CHECK(strcmp(ev.u.gps.sats, "8") == 0);
    CHECK(ev.u.gps.text[0] == '\0');

    memset(&ev, 0, sizeof(ev));
    ev.kind = SrEventScanStarted;
    CHECK(ev.kind == SrEventScanStarted);

    memset(&ev, 0, sizeof(ev));
    ev.kind = SrEventScanStopped;
    ev.u.stop = SrStopWifiTranRecv;
    CHECK(ev.kind == SrEventScanStopped);
    CHECK(ev.u.stop == SrStopWifiTranRecv);
    ev.u.stop = SrStopEndNmea;
    CHECK(ev.u.stop == SrStopEndNmea);
    ev.u.stop = SrStopGpsUpdates;
    CHECK(ev.u.stop == SrStopGpsUpdates);

    memset(&ev, 0, sizeof(ev));
    ev.kind = SrEventFirmware;
    ev.u.firmware.kind = SrSourceMarauder;
    sr_strlcpy(ev.u.firmware.firmware, sizeof(ev.u.firmware.firmware), "Marauder");
    sr_strlcpy(ev.u.firmware.version, sizeof(ev.u.firmware.version), "v1.14.1");
    sr_strlcpy(ev.u.firmware.hardware, sizeof(ev.u.firmware.hardware), "ESP32-C5 DevKit");
    CHECK(ev.kind == SrEventFirmware);
    CHECK(ev.u.firmware.kind == SrSourceMarauder);
    CHECK(strcmp(ev.u.firmware.firmware, "Marauder") == 0);
    CHECK(strcmp(ev.u.firmware.version, "v1.14.1") == 0);
    CHECK(strcmp(ev.u.firmware.hardware, "ESP32-C5 DevKit") == 0);

    /*
     * ADR-010: unknown is a **borrowed view**, not a copy.
     * This simulates codec behavior -- it points at the caller-supplied
     * line buffer instead of copying the contents.
     */
    memset(&ev, 0, sizeof(ev));
    ev.kind = SrEventUnknown;
    {
        char line_buf[64];
        size_t n = sr_strlcpy(line_buf, sizeof(line_buf), "not a wardrive line");
        ev.u.unknown.text = line_buf;
        ev.u.unknown.len = n;

        CHECK(ev.kind == SrEventUnknown);
        CHECK(ev.u.unknown.len == strlen("not a wardrive line"));
        CHECK(ev.u.unknown.text == line_buf); /* borrowed, not copied */
        CHECK(strcmp(ev.u.unknown.text, "not a wardrive line") == 0);

        /* Borrow semantics: once the source buffer is mutated, the event
         * sees the updated contents. */
        line_buf[0] = 'N';
        CHECK(ev.u.unknown.text[0] == 'N');
    }
}

static void test_cmd_write_contract(void) {
    char buf[16];
    char sentinel[16];
    size_t wrote;

    memset(buf, 0x5A, sizeof(buf));
    memcpy(sentinel, buf, sizeof(buf));

    wrote = sr_cmd_write(buf, 0, "cmd");
    CHECK(wrote == 0);
    CHECK(memcmp(buf, sentinel, sizeof(buf)) == 0);

    wrote = sr_cmd_write(buf, 3, "cmd");
    CHECK(wrote == 0);
    CHECK(memcmp(buf, sentinel, sizeof(buf)) == 0);

    wrote = sr_cmd_write(buf, 4, "cmd");
    CHECK(wrote == 3);
    CHECK(strcmp(buf, "cmd") == 0);

    wrote = sr_cmd_write(NULL, 16, "cmd");
    CHECK(wrote == 0);

    wrote = sr_cmd_write(buf, sizeof(buf), NULL);
    CHECK(wrote == 0);
}

static size_t dummy_start(const SrScanCfg* cfg, char* buf, size_t cap) {
    (void)cfg;
    return sr_cmd_write(buf, cap, "x");
}

static size_t dummy_stop(char* buf, size_t cap) {
    return sr_cmd_write(buf, cap, "y");
}

static bool dummy_probe(const char* line, SrFirmwareInfo* out) {
    (void)line;
    (void)out;
    return false;
}

static SrParseResult dummy_feed(SrParser* parser, const char* line, size_t len, SrEvent* out) {
    (void)parser;
    (void)line;
    (void)len;
    (void)out;
    return SrParseUnknown;
}

static bool dummy_io_probe(SrIo* io, SrFirmwareInfo* out) {
    (void)io;
    (void)out;
    return false;
}

static bool dummy_io_start(SrIo* io, const SrScanCfg* cfg) {
    (void)io;
    (void)cfg;
    return false;
}

static bool dummy_io_stop(SrIo* io) {
    (void)io;
    return false;
}

static void test_vtable_shape(void) {
    const SrSourceCodec codec = {
        .name = "native",
        .build_start_cmd = dummy_start,
        .build_stop_cmd = dummy_stop,
        .probe_line = dummy_probe,
        .feed_line = dummy_feed,
    };
    const SigRoamSource src = {
        .codec = &codec,
        .probe = dummy_io_probe,
        .start_wardrive = dummy_io_start,
        .stop = dummy_io_stop,
    };
    char buf[8];
    SrScanCfg cfg;
    memset(&cfg, 0, sizeof(cfg));

    CHECK(src.codec == &codec);
    CHECK(strcmp(codec.name, "native") == 0);
    CHECK(codec.build_start_cmd(&cfg, buf, sizeof(buf)) == 1);
    CHECK(strcmp(buf, "x") == 0);
    CHECK(codec.build_stop_cmd(buf, sizeof(buf)) == 1);
    CHECK(strcmp(buf, "y") == 0);
    CHECK(codec.probe_line("", NULL) == false);
    CHECK(codec.feed_line(NULL, "", 0, NULL) == SrParseUnknown);
    CHECK(src.probe(NULL, NULL) == false);
    CHECK(src.start_wardrive(NULL, &cfg) == false);
    CHECK(src.stop(NULL) == false);
}

static void test_kinds_distinct(void) {
    CHECK(SrSourceUnknown != SrSourceMarauder);
    CHECK(SrSourceMarauder != SrSourceGhostesp);
    CHECK(SrSourceGhostesp != SrSourceNative);
    CHECK(SrParseOk != SrParseNeedMore);
    CHECK(SrParseNeedMore != SrParseUnknown);
    CHECK(SrParseUnknown != SrParseMalformed);
    CHECK(SrEventApFound != SrEventBleFound);
    CHECK(SrEventBleFound != SrEventGps);
    CHECK(SrEventScanStarted != SrEventScanStopped);
    CHECK(SrStopWifiTranRecv != SrStopEndNmea);
    CHECK(SrStopEndNmea != SrStopGpsUpdates);
    CHECK(SrRadioWifi != SrRadioBle);
}

int test_source_iface_run(void) {
    sr_test_failures = 0;
    test_parser_zero();
    test_scan_cfg();
    test_fixed_buffers();
    test_event_tags();
    test_cmd_write_contract();
    test_vtable_shape();
    test_kinds_distinct();
    return sr_test_failures;
}
