// Host-side tests for protocol.c and alerts.c. Build: make -C tests
#include <stdio.h>
#include <string.h>
#include "../protocol.h"
#include "../alerts.h"
#include "../advice.h"

static int fails = 0, checks = 0;
#define CHECK(c) \
    do { \
        checks++; \
        if(!(c)) { \
            fails++; \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
        } \
    } while(0)

static void make_frame(uint8_t* f, uint8_t seq) {
    memset(f, 0, PHR_TELEMETRY_LEN);
    f[0] = 0x50; f[1] = 0x48; f[2] = 1; f[3] = 1; f[4] = seq;
    f[5] = PHR_FLAG_CPU_TEMP_VALID | PHR_FLAG_GPU_PRESENT | PHR_FLAG_GPU_TEMP_VALID;
    f[6] = 42; f[7] = 65; f[8] = 30; f[9] = 55; f[10] = 60; f[11] = 20; f[12] = 70; f[13] = 0;
    f[14] = 160; f[15] = 0; f[16] = 0x30; f[17] = 0; // ram 16.0 GB, vram 4.8 GB
    f[18] = 0x10; f[19] = 0x0E; // 3600 MHz
    f[22] = 12; f[23] = 9;
    memcpy(f + 24, "chrome", 6);
    memcpy(f + 36, "Code", 4);
    f[44] = 100; f[45] = 0;
    uint16_t crc = phr_crc16(f, 46);
    f[46] = crc & 0xFF; f[47] = crc >> 8;
}

static void fix_crc(uint8_t* f) {
    uint16_t crc = phr_crc16(f, 46);
    f[46] = crc & 0xFF; f[47] = crc >> 8;
}

static void test_crc(void) {
    CHECK(phr_crc16((const uint8_t*)"123456789", 9) == 0x29B1);
}

static void test_parse(void) {
    uint8_t f[48];
    PhrTelemetry t;
    make_frame(f, 7);
    CHECK(phr_parse_telemetry(f, 48, &t));
    CHECK(t.seq == 7 && t.cpu_load == 42 && t.cpu_temp == 65 && t.gpu_temp == 55);
    CHECK(t.ram_total_dgb == 160 && t.vram_total_dgb == 48 && t.cpu_clock_mhz == 3600);
    CHECK(strcmp(t.top_cpu_name, "chrome") == 0 && strcmp(t.top_ram_name, "Code") == 0);
    CHECK(t.uptime_h == 100);

    f[10] ^= 1; // corrupt payload
    CHECK(!phr_parse_telemetry(f, 48, &t));
    make_frame(f, 1);
    CHECK(!phr_parse_telemetry(f, 47, &t));
    CHECK(!phr_parse_telemetry(f, 49, &t));
    f[2] = 2; fix_crc(f); // unknown version
    CHECK(!phr_parse_telemetry(f, 48, &t));
    make_frame(f, 1);
    f[0] = 0x51; fix_crc(f); // wrong magic
    CHECK(!phr_parse_telemetry(f, 48, &t));
    make_frame(f, 1);
    memset(f + 24, 'A', 12); // full-width name, no NUL
    fix_crc(f);
    CHECK(phr_parse_telemetry(f, 48, &t) && strlen(t.top_cpu_name) == 12);
}

static void test_hello(void) {
    uint8_t h[12];
    CHECK(phr_build_hello(h, 3, 1, 2) == 12);
    CHECK(h[0] == 0x50 && h[1] == 0x48 && h[2] == 1 && h[3] == 0x81 && h[4] == 3);
    CHECK(h[5] == 100 && h[6] == 0 && h[7] == 1 && h[8] == 2 && h[9] == 0);
    CHECK((h[10] | (h[11] << 8)) == phr_crc16(h, 10));
}

static int cb_count; static uint8_t cb_last_seq;
static void cb(const PhrTelemetry* t, void* ctx) {
    (void)ctx; cb_count++; cb_last_seq = t->seq;
}

static void test_stream(void) {
    uint8_t a[48], b[48], c[48], buf[400];
    PhrStream s;
    make_frame(a, 1); make_frame(b, 2); make_frame(c, 3);

    // byte-at-a-time
    phr_stream_reset(&s); cb_count = 0;
    for(int i = 0; i < 48; i++) phr_stream_feed(&s, &a[i], 1, cb, NULL);
    CHECK(cb_count == 1 && cb_last_seq == 1);

    // garbage + frame + partial garbage with fake magic + frame
    size_t n = 0;
    const uint8_t junk[] = {0x00, 0x50, 0x50, 0x48, 0x07, 0xFF, 0x50};
    memcpy(buf + n, junk, sizeof(junk)); n += sizeof(junk);
    memcpy(buf + n, a, 48); n += 48;
    memcpy(buf + n, junk, sizeof(junk)); n += sizeof(junk);
    memcpy(buf + n, b, 48); n += 48;
    phr_stream_reset(&s); cb_count = 0;
    phr_stream_feed(&s, buf, n, cb, NULL);
    CHECK(cb_count == 2 && cb_last_seq == 2);

    // corrupt frame followed immediately by a good one: good one must survive
    uint8_t bad[48]; memcpy(bad, a, 48); bad[20] ^= 0x55;
    n = 0;
    memcpy(buf + n, bad, 48); n += 48;
    memcpy(buf + n, c, 48); n += 48;
    phr_stream_reset(&s); cb_count = 0;
    phr_stream_feed(&s, buf, n, cb, NULL);
    CHECK(cb_count == 1 && cb_last_seq == 3);

    // truncated frame then a full one (sender restarted mid-frame)
    n = 0;
    memcpy(buf + n, a, 20); n += 20;
    memcpy(buf + n, b, 48); n += 48;
    phr_stream_reset(&s); cb_count = 0;
    phr_stream_feed(&s, buf, n, cb, NULL);
    CHECK(cb_count == 1 && cb_last_seq == 2);

    // arbitrary chunking
    n = 0;
    for(int k = 0; k < 3; k++) { memcpy(buf + n, (k == 0 ? a : k == 1 ? b : c), 48); n += 48; }
    phr_stream_reset(&s); cb_count = 0;
    for(size_t i = 0; i < n; i += 7) phr_stream_feed(&s, buf + i, (n - i) < 7 ? (n - i) : 7, cb, NULL);
    CHECK(cb_count == 3 && cb_last_seq == 3);
}

static PhrTelemetry base_t(void) {
    PhrTelemetry t; memset(&t, 0, sizeof(t));
    t.flags = PHR_FLAG_CPU_TEMP_VALID | PHR_FLAG_GPU_PRESENT | PHR_FLAG_GPU_TEMP_VALID;
    t.cpu_temp = 50; t.gpu_temp = 50;
    return t;
}

static void test_alerts(void) {
    AlertEngine e; AlertEvent ev[AlertRuleCount];
    PhrTelemetry t = base_t();
    alerts_set_defaults(&e);
    CHECK(e.cfg[AlertCpuTemp].threshold == 90 && e.cfg[AlertGpuTemp].threshold == 85);
    CHECK(!e.cfg[AlertGpuLoad].enabled);

    // no events at normal values
    CHECK(alerts_update(&e, 0, &t, 0, ev) == 0);

    // immediate cpu temp (sustain 0)
    t.cpu_temp = 91;
    CHECK(alerts_update(&e, 1, &t, 0, ev) == 1 && ev[0].rule == AlertCpuTemp && ev[0].value == 91);
    // cooldown: no repeat for 300 s
    CHECK(alerts_update(&e, 100, &t, 0, ev) == 0);
    CHECK(alerts_update(&e, 300, &t, 0, ev) == 0);
    CHECK(alerts_update(&e, 301, &t, 0, ev) == 1);
    // hysteresis: 86 (>= 90-5) keeps active; 84 clears
    t.cpu_temp = 86;
    alerts_update(&e, 302, &t, 0, ev);
    CHECK(alerts_is_active(&e, AlertCpuTemp));
    t.cpu_temp = 84;
    alerts_update(&e, 303, &t, 0, ev);
    CHECK(!alerts_is_active(&e, AlertCpuTemp));
    // new episode fires immediately
    t.cpu_temp = 95;
    CHECK(alerts_update(&e, 304, &t, 0, ev) == 1);

    // invalid field never fires
    alerts_reset_state(&e);
    t = base_t(); t.flags &= ~PHR_FLAG_CPU_TEMP_VALID; t.cpu_temp = 120;
    CHECK(alerts_update(&e, 0, &t, 0, ev) == 0);
    t.flags &= ~PHR_FLAG_GPU_PRESENT; t.gpu_temp = 120; t.gpu_load = 100; t.vram_load = 100;
    CHECK(alerts_update(&e, 1, &t, 0, ev) == 0);

    // sustain: ram 90% for 10 s
    alerts_reset_state(&e);
    t = base_t(); t.ram_load = 92;
    int fired_at = -1;
    for(uint32_t s = 0; s < 20; s++)
        if(alerts_update(&e, 1000 + s, &t, 0, ev) && fired_at < 0) fired_at = (int)s;
    CHECK(fired_at == 10);
    // sustain interrupted resets the timer
    alerts_reset_state(&e);
    for(uint32_t s = 0; s < 8; s++) CHECK(alerts_update(&e, s, &t, 0, ev) == 0);
    t.ram_load = 70; alerts_update(&e, 8, &t, 0, ev);
    t.ram_load = 92;
    for(uint32_t s = 9; s < 18; s++) CHECK(alerts_update(&e, s, &t, 0, ev) == 0);
    CHECK(alerts_update(&e, 19, &t, 0, ev) == 1);

    // snooze on acknowledge (10 min) then cooldown/snooze both elapsed
    alerts_reset_state(&e);
    t = base_t(); t.cpu_temp = 95;
    CHECK(alerts_update(&e, 0, &t, 0, ev) == 1);
    alerts_acknowledge(&e, AlertCpuTemp, 5);
    CHECK(alerts_update(&e, 310, &t, 0, ev) == 0); // cooldown passed but snoozed
    CHECK(alerts_update(&e, 604, &t, 0, ev) == 0);
    CHECK(alerts_update(&e, 606, &t, 0, ev) == 1); // snooze ended (5+600)

    // disabled rule, signal propagates
    alerts_reset_state(&e);
    e.cfg[AlertCpuTemp].enabled = false;
    CHECK(alerts_update(&e, 0, &t, 0, ev) == 0);
    e.cfg[AlertCpuTemp].enabled = true; e.cfg[AlertCpuTemp].signal = AlertSignalLed;
    CHECK(alerts_update(&e, 1, &t, 0, ev) == 1 && ev[0].signal == AlertSignalLed);

    // battery: only on battery, fires at <= 20, clears above 25
    alerts_reset_state(&e);
    t = base_t(); t.flags |= PHR_FLAG_BATTERY_PRESENT; t.battery = 15;
    CHECK(alerts_update(&e, 0, &t, 0, ev) == 0); // plugged in
    t.flags |= PHR_FLAG_ON_BATTERY;
    CHECK(alerts_update(&e, 1, &t, 0, ev) == 1 && ev[0].rule == AlertBatteryLow);
    t.battery = 24; alerts_update(&e, 2, &t, 0, ev);
    CHECK(alerts_is_active(&e, AlertBatteryLow));
    t.battery = 26; alerts_update(&e, 3, &t, 0, ev);
    CHECK(!alerts_is_active(&e, AlertBatteryLow));

    // link lost: no telemetry, silence >= 10 s
    alerts_reset_state(&e);
    CHECK(alerts_update(&e, 0, NULL, 6, ev) == 0);
    CHECK(alerts_update(&e, 4, NULL, 10, ev) == 1 && ev[0].rule == AlertLinkLost);
    CHECK(alerts_update(&e, 5, NULL, 11, ev) == 0);
    CHECK(alerts_update(&e, 6, &t, 0, ev) == 0);
    CHECK(!alerts_is_active(&e, AlertLinkLost));
}

static void test_advice(void) {
    char l[ADVICE_MAX_LINES][ADVICE_LINE_LEN];
    PhrTelemetry t = base_t();
    strcpy(t.top_ram_name, "chrome"); strcpy(t.top_cpu_name, "obs64"); t.top_cpu_pct = 40;
    CHECK(advice_get(AlertRamLoad, &t, l) == 3 && strcmp(l[0], "Close chrome") == 0);
    CHECK(strcmp(advice_get(AlertCpuTemp, &t, l) == 3 ? l[2] : "", "Close obs64") == 0);
    CHECK(advice_get(AlertDiskLoad, NULL, l) == 3);
    for(int r = 0; r < AlertRuleCount; r++) {
        size_t n = advice_get((AlertRuleId)r, NULL, l);
        CHECK(n >= 2 && n <= 3);
        for(size_t i = 0; i < n; i++) CHECK(strlen(l[i]) > 0 && strlen(l[i]) <= 22);
    }
}

int main(void) {
    test_crc(); test_parse(); test_hello(); test_stream(); test_alerts(); test_advice();
    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
