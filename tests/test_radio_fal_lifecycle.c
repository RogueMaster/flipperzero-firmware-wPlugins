#include "mf_radio_core.h"
#include "morse_flipper_radio_host.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char calls[64];
    size_t count;
    bool tx_ok;
    bool rx_ok;
    bool valid;
    bool allowed;
    uint32_t default_hz;
} FakeHardware;

static void record(FakeHardware* fake, char call) {
    assert(fake->count + 1U < sizeof(fake->calls));
    fake->calls[fake->count++] = call;
    fake->calls[fake->count] = '\0';
}

static bool prepare_tx(void* context, uint32_t frequency_hz) {
    (void)frequency_hz;
    FakeHardware* fake = context;
    record(fake, 'T');
    return fake->tx_ok;
}
static bool prepare_rx(void* context, uint32_t frequency_hz) {
    (void)frequency_hz;
    FakeHardware* fake = context;
    record(fake, 'R');
    return fake->rx_ok;
}
static void set_level(void* context, bool level) {
    record(context, level ? 'H' : 'L');
}
static bool read_carrier(void* context) {
    (void)context;
    return false;
}
static int8_t read_rssi(void* context) {
    (void)context;
    return -100;
}
static bool frequency_valid(void* context, uint32_t frequency_hz) {
    (void)frequency_hz;
    return ((FakeHardware*)context)->valid;
}
static bool tx_allowed(void* context, uint32_t frequency_hz) {
    (void)frequency_hz;
    return ((FakeHardware*)context)->allowed;
}
static uint32_t default_frequency(void* context) {
    return ((FakeHardware*)context)->default_hz;
}
static void idle(void* context) {
    record(context, 'I');
}
static void sleep_radio(void* context) {
    record(context, 'S');
}

static void draw_tx(
    void* context,
    Canvas* canvas,
    const MorseFlipperRunHistory* history,
    uint8_t preview,
    bool preview_extendable,
    const char* frequency_line) {
    (void)context;
    (void)canvas;
    (void)history;
    (void)preview;
    (void)preview_extendable;
    (void)frequency_line;
}

static MfRadioHardwareOps hardware(FakeHardware* fake) {
    return (MfRadioHardwareOps){
        .prepare_tx = prepare_tx,
        .prepare_carrier_rx = prepare_rx,
        .set_tx_level = set_level,
        .read_carrier = read_carrier,
        .read_rssi_dbm = read_rssi,
        .frequency_valid = frequency_valid,
        .tx_allowed = tx_allowed,
        .default_frequency = default_frequency,
        .idle = idle,
        .sleep = sleep_radio,
        .context = fake,
    };
}

static MfRadioEnterArgs enter_args(void) {
    static const MfRadioDrawServices draw = {
        .struct_size = sizeof(MfRadioDrawServices),
        .history_reset = morse_flipper_run_history_reset,
        .history_append = morse_flipper_run_history_append,
        .draw_tx_history = draw_tx,
    };
    return (MfRadioEnterArgs){
        .struct_size = sizeof(MfRadioEnterArgs),
        .frequency_hz = MF_RADIO_DEFAULT_FREQUENCY_HZ,
        .dit_ms = 100U,
        .monitor_threshold_dbm = -95,
        .receive_audio_enabled = true,
        .decoder = morse_flipper_radio_decoder_services(),
        .draw = &draw,
    };
}

int main(void) {
    FakeHardware fake = {
        .tx_ok = true,
        .rx_ok = true,
        .valid = true,
        .allowed = true,
        .default_hz = MF_RADIO_DEFAULT_FREQUENCY_HZ,
    };
    MfRadioHardwareOps ops = hardware(&fake);
    MfRadioEnterArgs args = enter_args();
    MfRadioState state;
    MfRadioSnapshot snapshot = {.struct_size = sizeof(MfRadioSnapshot)};
    MorseFlipperMappedFalResult result;

    assert(!mf_radio_core_enter(NULL, &args, &ops, &result));
    assert(!mf_radio_core_enter(&state, NULL, &ops, &result));
    args.decoder = NULL;
    assert(!mf_radio_core_enter(&state, &args, &ops, &result));
    args = enter_args();
    assert(mf_radio_core_enter(&state, &args, &ops, &result));
    assert(result.redraw);
    assert(fake.count == 0U);

    result = mf_radio_core_set_page(&state, MfRadioPageTransmit, 10U);
    assert(result.handled);
    assert(strcmp(fake.calls, "S") == 0);
    fake.count = 0U;
    fake.calls[0] = '\0';
    result = mf_radio_core_sync_tx(&state, MfRadioTxIntervalNone, 0U, true, 20U);
    assert(result.handled && strcmp(fake.calls, "TH") == 0);
    result = mf_radio_core_sync_tx(&state, MfRadioTxIntervalMark, 100U, false, 120U);
    assert(result.handled && strcmp(fake.calls, "THL") == 0);
    mf_radio_core_leave(&state);
    assert(strcmp(fake.calls, "THLLIS") == 0);
    assert(mf_radio_core_snapshot(&state, &snapshot));
    assert(!snapshot.hardware_active && !snapshot.tx_active && !snapshot.monitor_tone);
    {
        size_t calls = fake.count;
        mf_radio_core_leave(&state);
        assert(fake.count == calls);
        assert(!mf_radio_core_sync_tx(
                    &state, MfRadioTxIntervalNone, 0U, true, 130U)
                    .handled);
        assert(!mf_radio_core_tick(&state, 130U).handled);
    }

    fake.count = 0U;
    fake.calls[0] = '\0';
    assert(mf_radio_core_enter(&state, &args, &ops, &result));
    fake.rx_ok = false;
    result = mf_radio_core_set_page(&state, MfRadioPageReceive, 1U);
    assert(result.handled && strcmp(fake.calls, "SR") == 0);
    assert(mf_radio_core_snapshot(&state, &snapshot));
    assert(snapshot.page == MfRadioPageIdle && !snapshot.hardware_active);
    mf_radio_core_leave(&state);
    assert(strcmp(fake.calls, "SRS") == 0);

    fake.count = 0U;
    fake.calls[0] = '\0';
    fake.rx_ok = true;
    fake.tx_ok = false;
    assert(mf_radio_core_enter(&state, &args, &ops, &result));
    mf_radio_core_set_page(&state, MfRadioPageTransmit, 1U);
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalNone, 0U, true, 2U);
    assert(strcmp(fake.calls, "ST") == 0);
    mf_radio_core_leave(&state);
    assert(strcmp(fake.calls, "STS") == 0);

    fake.count = 0U;
    fake.calls[0] = '\0';
    fake.tx_ok = true;
    assert(mf_radio_core_enter(&state, &args, &ops, &result));
    mf_radio_core_set_page(&state, MfRadioPageReceive, 1U);
    assert(strcmp(fake.calls, "SR") == 0);
    mf_radio_core_leave(&state);
    assert(strcmp(fake.calls, "SRIS") == 0);

    puts("test_radio_fal_lifecycle: passed");
    return 0;
}
