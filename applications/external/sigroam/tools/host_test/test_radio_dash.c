#define SR_HOST_TEST 1

#include "sr_test.h"

#include "sr_bloom.h"
#include "sr_model.h"
#include "sr_rawlog.h"
#include "sr_types.h"
#include "sr_view_fmt.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * T6.5 FAP half B (docs/exec-plans/t65-fap-radio.md): model radio + radio_rev,
 * Dash BLE field prints OFF when the permission bit is off, and does not treat
 * a missing Radio: line as 0.
 */

static int streq(const char* a, const char* b) {
    if(a == NULL || b == NULL) {
        return a == b;
    }
    return strcmp(a, b) == 0;
}

int test_radio_dash_run(void);

int test_radio_dash_run(void) {
    SrBloom bloom;
    SrRawLog rawlog;
    SrModel m;
    SrEvent ev;
    char field[12];
    char line[32];
    size_t n;

    sr_test_failures = 0;

    CHECK((unsigned)SrEventRadio == 11u);
    CHECK(sizeof(SrRadioInfo) == 2u);
    CHECK(sizeof(SrEvent) == 240u);

    sr_bloom_init(&bloom);
    memset(&rawlog, 0, sizeof(rawlog));
    sr_model_init(&m, &bloom, &rawlog);
    CHECK(m.radio_rev == 0u);
    CHECK(m.radio.wifi == 0u);
    CHECK(m.radio.ble == 0u);
    CHECK(m.unknown_lines == 0u);

    /* Missing Radio: is unknown, not permission-off. Live count 0 still prints "0". */
    n = sr_fmt_ble_field(m.radio_rev, m.radio.ble, 0u, field, sizeof(field));
    CHECK(n == 1u);
    CHECK(streq(field, "0"));
    CHECK(!streq(field, "OFF"));

    memset(&ev, 0, sizeof(ev));
    ev.kind = SrEventSess;
    ev.u.sess.ap = 1u;
    ev.u.sess.ble = 0u;
    CHECK(sr_model_apply(&m, &ev, 10u) == true);
    CHECK(m.radio_rev == 0u);
    CHECK(m.unknown_lines == 0u);

    /* wifi=1 ble=0 is a measurement: rev leaves 0, Dash must say OFF not 0. */
    memset(&ev, 0, sizeof(ev));
    ev.kind = SrEventRadio;
    ev.u.radio.wifi = 1u;
    ev.u.radio.ble = 0u;
    CHECK(sr_model_apply(&m, &ev, 20u) == true);
    CHECK(m.radio_rev == 1u);
    CHECK(m.radio.wifi == 1u);
    CHECK(m.radio.ble == 0u);
    CHECK(m.unknown_lines == 0u);

    n = sr_fmt_ble_field(m.radio_rev, m.radio.ble, m.ap_ble, field, sizeof(field));
    CHECK(n == 3u);
    CHECK(streq(field, "OFF"));
    {
        char ap[12];
        sr_fmt__udec(1035u, ap, sizeof(ap));
        (void)snprintf(line, sizeof(line), "AP=%s BLE=%s", ap, field);
        CHECK(streq(line, "AP=1035 BLE=OFF"));
        CHECK(strstr(line, "BLE=0") == NULL);
    }

    /* BLE on: even ap_ble==0 is a count, not OFF. */
    memset(&ev, 0, sizeof(ev));
    ev.kind = SrEventRadio;
    ev.u.radio.wifi = 1u;
    ev.u.radio.ble = 1u;
    CHECK(sr_model_apply(&m, &ev, 30u) == true);
    CHECK(m.radio_rev == 2u);
    CHECK(m.radio.ble == 1u);
    n = sr_fmt_ble_field(m.radio_rev, m.radio.ble, 0u, field, sizeof(field));
    CHECK(streq(field, "0"));
    n = sr_fmt_ble_field(m.radio_rev, m.radio.ble, 7404u, field, sizeof(field));
    CHECK(streq(field, "7404"));

    /* reset_session keeps radio / radio_rev (board snapshot, not session counts). */
    m.ap_wifi = 9u;
    m.ap_ble = 4u;
    sr_model_reset_session(&m, false);
    CHECK(m.radio_rev == 2u);
    CHECK(m.radio.wifi == 1u);
    CHECK(m.radio.ble == 1u);
    CHECK(m.ap_wifi == 0u);
    CHECK(m.ap_ble == 0u);

    CHECK(sr_fmt_ble_field(1u, 0u, 0u, NULL, 8) == 0);
    CHECK(sr_fmt_ble_field(1u, 0u, 0u, field, 0) == 0);

    if(sr_test_failures == 0) {
        fprintf(stderr, "radio_dash: ok\n");
    }
    return sr_test_failures;
}
