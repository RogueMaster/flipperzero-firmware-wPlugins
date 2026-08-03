#include <stdio.h>
#include <string.h>

#include "plugins/common/mf_radio_tx_session.h"

typedef struct {
    char trace[64];
    size_t used;
    bool allow;
    bool prepare_ok;
    bool mark_ok;
} Mock;

static void add(Mock* mock, char ch) {
    mock->trace[mock->used++] = ch;
    mock->trace[mock->used] = '\0';
}
static bool prepare(void* c, uint32_t f, MfRadioTxMode m) {
    (void)f;
    (void)m;
    add(c, 'P');
    return ((Mock*)c)->prepare_ok;
}
static bool carrier_rx(void* c, uint32_t f) {
    (void)c;
    (void)f;
    return false;
}
static bool mark(void* c, bool on) {
    add(c, on ? '1' : '0');
    return ((Mock*)c)->mark_ok;
}
static void stop(void* c) {
    add(c, 'S');
}
static bool read_carrier(void* c) {
    (void)c;
    return false;
}
static int8_t read_rssi(void* c) {
    (void)c;
    return -100;
}
static bool valid(void* c, uint32_t f) {
    (void)c;
    return f != 0U;
}
static bool allowed(void* c, uint32_t f) {
    return ((Mock*)c)->allow && f != 0U;
}
static uint32_t default_frequency(void* c) {
    (void)c;
    return 433160000U;
}
static void idle(void* c) {
    add(c, 'I');
}
static void sleep_(void* c) {
    add(c, 'Z');
}

#define CHECK(x)                                                           \
    do {                                                                   \
        if(!(x)) {                                                         \
            fprintf(stderr, "failed %s:%d: %s\n", __FILE__, __LINE__, #x); \
            return 1;                                                      \
        }                                                                  \
    } while(0)

int main(void) {
    Mock mock = {.allow = true, .prepare_ok = true, .mark_ok = true};
    MfRadioHardwareOps ops = {
        .prepare_tx = prepare,
        .prepare_carrier_rx = carrier_rx,
        .set_tx_level = mark,
        .stop_tx = stop,
        .read_carrier = read_carrier,
        .read_rssi_dbm = read_rssi,
        .frequency_valid = valid,
        .tx_allowed = allowed,
        .default_frequency = default_frequency,
        .idle = idle,
        .sleep = sleep_,
        .context = &mock};
    MfRadioTxSession session;
    mf_radio_tx_session_init(&session, &ops);
    mf_radio_tx_session_stop(&session);
    CHECK(strcmp(mock.trace, "SIZ") == 0);
    mock.used = 0U;
    mock.trace[0] = '\0';
    CHECK(mf_radio_tx_session_start(&session, 433160000U, MfRadioTxModeOok));
    CHECK(mf_radio_tx_session_set_mark(&session, true));
    CHECK(mf_radio_tx_session_set_mark(&session, false));
    mf_radio_tx_session_stop(&session);
    CHECK(strcmp(mock.trace, "P10SIZ") == 0);
    mf_radio_tx_session_stop(&session);
    CHECK(strstr(mock.trace, "SIZSIZ") != NULL);
    mock.used = 0U;
    mock.trace[0] = '\0';
    CHECK(mf_radio_tx_session_prepare(&session, 433160000U, MfRadioTxModeCwfm));
    CHECK(strcmp(mock.trace, "P") == 0);
    CHECK(mf_radio_tx_session_set_mark(&session, true));
    CHECK(strcmp(mock.trace, "P1") == 0);
    mf_radio_tx_session_stop(&session);
    mock.used = 0U;
    mock.trace[0] = '\0';
    CHECK(mf_radio_tx_session_start(&session, 433160000U, MfRadioTxModeCwfm));
    CHECK(strcmp(mock.trace, "P0") == 0);
    CHECK(mf_radio_tx_session_set_mark(&session, true));
    CHECK(mf_radio_tx_session_set_mark(&session, false));
    CHECK(strcmp(mock.trace, "P010") == 0);
    mf_radio_tx_session_stop(&session);
    mock.allow = false;
    CHECK(!mf_radio_tx_session_start(&session, 433160000U, MfRadioTxModeCwfm));
    mock.allow = true;
    mock.prepare_ok = false;
    CHECK(!mf_radio_tx_session_start(&session, 433160000U, MfRadioTxModeOok));
    CHECK(!session.prepared && !session.mark);
    puts("test_ardf_tx_session: passed");
    return 0;
}
