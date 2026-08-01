#include "mf_radio_core.h"
#include "morse_flipper_radio_host.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char calls[64];
    size_t count;
    bool tx_ok;
    bool high_ok;
    bool low_ok;
    bool rx_ok;
    bool valid;
    bool allowed;
    uint32_t default_hz;
    MfRadioTxMode prepared_mode;
} FakeHardware;

static void record(FakeHardware* fake, char call) {
    assert(fake->count + 1U < sizeof(fake->calls));
    fake->calls[fake->count++] = call;
    fake->calls[fake->count] = '\0';
}

static bool prepare_tx(void* context, uint32_t frequency_hz, MfRadioTxMode mode) {
    (void)frequency_hz;
    FakeHardware* fake = context;
    record(fake, 'T');
    fake->prepared_mode = mode;
    return fake->tx_ok;
}
static bool prepare_rx(void* context, uint32_t frequency_hz) {
    (void)frequency_hz;
    FakeHardware* fake = context;
    record(fake, 'R');
    return fake->rx_ok;
}
static bool set_level(void* context, bool level) {
    record(context, level ? 'H' : 'L');
    return level ? ((FakeHardware*)context)->high_ok : ((FakeHardware*)context)->low_ok;
}
static void stop_tx(void* context) {
    record(context, 'X');
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

static void draw_rx(
    void* context,
    Canvas* canvas,
    const char* text,
    uint8_t preview,
    bool preview_extendable) {
    (void)context;
    (void)canvas;
    (void)text;
    (void)preview;
    (void)preview_extendable;
}

static MfRadioHardwareOps hardware(FakeHardware* fake) {
    return (MfRadioHardwareOps){
        .prepare_tx = prepare_tx,
        .prepare_carrier_rx = prepare_rx,
        .set_tx_level = set_level,
        .stop_tx = stop_tx,
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
        .draw_rx_text = draw_rx,
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
        .high_ok = true,
        .low_ok = true,
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
    ops.stop_tx = NULL;
    assert(!mf_radio_core_enter(&state, &args, &ops, &result));
    ops = hardware(&fake);
    args.decoder = NULL;
    assert(!mf_radio_core_enter(&state, &args, &ops, &result));
    args = enter_args();
    assert(mf_radio_core_enter(&state, &args, &ops, &result));
    assert(result.redraw);
    assert(fake.count == 0U);

    result = mf_radio_core_set_page(&state, MfRadioPageTransmit, 10U);
    assert(result.handled);
    assert(strcmp(fake.calls, "") == 0);
    fake.count = 0U;
    fake.calls[0] = '\0';
    result = mf_radio_core_sync_tx(&state, MfRadioTxIntervalNone, 0U, true, 20U);
    assert(
        result.handled && strcmp(fake.calls, "TH") == 0 && fake.prepared_mode == MfRadioTxModeOok);
    result = mf_radio_core_sync_tx(&state, MfRadioTxIntervalMark, 100U, false, 120U);
    assert(result.handled && strcmp(fake.calls, "THL") == 0);
    mf_radio_core_tick(&state, 319U);
    assert(strcmp(fake.calls, "THL") == 0);
    result = mf_radio_core_tick(&state, 320U);
    assert(result.redraw && strcmp(fake.calls, "THLXIS") == 0);
    assert(mf_radio_core_snapshot(&state, &snapshot));
    assert(!snapshot.hardware_active && !snapshot.tx_active);
    mf_radio_core_leave(&state);
    assert(strcmp(fake.calls, "THLXIS") == 0);
    assert(mf_radio_core_snapshot(&state, &snapshot));
    assert(!snapshot.hardware_active && !snapshot.tx_active && !snapshot.monitor_tone);
    {
        size_t calls = fake.count;
        mf_radio_core_leave(&state);
        assert(fake.count == calls);
        assert(!mf_radio_core_sync_tx(&state, MfRadioTxIntervalNone, 0U, true, 130U).handled);
        assert(!mf_radio_core_tick(&state, 130U).handled);
    }

    /* CWFM holds a quiet carrier through a seven-dit/500 ms minimum hang. */
    fake.count = 0U;
    fake.calls[0] = '\0';
    assert(mf_radio_core_enter(&state, &args, &ops, &result));
    mf_radio_core_set_page(&state, MfRadioPageTransmit, 10U);
    {
        InputEvent right = {.key = InputKeyRight, .type = InputTypeShort};
        assert(mf_radio_core_input(&state, &right, 11U).handled);
    }
    assert(state.tx_mode == MfRadioTxModeCwfm);
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalNone, 0U, true, 20U);
    assert(strcmp(fake.calls, "TLH") == 0 && fake.prepared_mode == MfRadioTxModeCwfm);
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalMark, 100U, false, 120U);
    assert(strcmp(fake.calls, "TLHL") == 0 && state.tx_idle_at == 820U);
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalSpace, 30U, false, 150U);
    assert(strcmp(fake.calls, "TLHL") == 0 && state.tx_idle_at == 820U);
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalSpace, 250U, true, 400U);
    assert(strcmp(fake.calls, "TLHLH") == 0 && !state.tx_idle_pending);
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalMark, 100U, false, 500U);
    assert(strcmp(fake.calls, "TLHLHL") == 0 && state.tx_idle_at == 1200U);
    mf_radio_core_tick(&state, 1199U);
    assert(strcmp(fake.calls, "TLHLHL") == 0);
    result = mf_radio_core_tick(&state, 1200U);
    assert(result.redraw && strcmp(fake.calls, "TLHLHLXIS") == 0);
    assert(state.tx_mode == MfRadioTxModeCwfm && !state.snapshot.hardware_active);
    mf_radio_core_leave(&state);

    /* A toggle during the static hang stops RF before changing mode. */
    fake.count = 0U;
    fake.calls[0] = '\0';
    assert(mf_radio_core_enter(&state, &args, &ops, &result));
    mf_radio_core_set_page(&state, MfRadioPageTransmit, 0U);
    {
        InputEvent right = {.key = InputKeyRight, .type = InputTypeShort};
        mf_radio_core_input(&state, &right, 1U);
        mf_radio_core_sync_tx(&state, MfRadioTxIntervalNone, 0U, true, 2U);
        result = mf_radio_core_input(&state, &right, 3U);
        assert(result.handled && !result.redraw && state.tx_mode == MfRadioTxModeCwfm);
        assert(strcmp(fake.calls, "TLH") == 0);
        mf_radio_core_sync_tx(&state, MfRadioTxIntervalMark, 100U, false, 102U);
        result = mf_radio_core_input(&state, &right, 103U);
        assert(result.handled && result.redraw && state.tx_mode == MfRadioTxModeOok);
        assert(strcmp(fake.calls, "TLHLXIS") == 0);
    }
    mf_radio_core_leave(&state);

    /* Leaving the page or FAL stops an active callback exactly once. */
    fake.count = 0U;
    fake.calls[0] = '\0';
    assert(mf_radio_core_enter(&state, &args, &ops, &result));
    mf_radio_core_set_page(&state, MfRadioPageTransmit, 0U);
    {
        InputEvent right = {.key = InputKeyRight, .type = InputTypeShort};
        mf_radio_core_input(&state, &right, 1U);
    }
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalNone, 0U, true, 2U);
    mf_radio_core_set_page(&state, MfRadioPageIdle, 3U);
    assert(strcmp(fake.calls, "TLHXIS") == 0 && state.tx_mode == MfRadioTxModeOok);
    mf_radio_core_leave(&state);
    assert(strcmp(fake.calls, "TLHXIS") == 0);

    fake.count = 0U;
    fake.calls[0] = '\0';
    assert(mf_radio_core_enter(&state, &args, &ops, &result));
    mf_radio_core_set_page(&state, MfRadioPageTransmit, 0U);
    {
        InputEvent right = {.key = InputKeyRight, .type = InputTypeShort};
        mf_radio_core_input(&state, &right, 1U);
    }
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalNone, 0U, true, 2U);
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalMark, 100U, false, 102U);
    mf_radio_core_leave(&state);
    assert(strcmp(fake.calls, "TLHLXIS") == 0 && state.tx_mode == MfRadioTxModeOok);
    mf_radio_core_leave(&state);
    assert(strcmp(fake.calls, "TLHLXIS") == 0);

    /* Both async-start and static-carrier failures roll back completely. */
    fake.count = 0U;
    fake.calls[0] = '\0';
    fake.high_ok = false;
    assert(mf_radio_core_enter(&state, &args, &ops, &result));
    mf_radio_core_set_page(&state, MfRadioPageTransmit, 0U);
    {
        InputEvent right = {.key = InputKeyRight, .type = InputTypeShort};
        mf_radio_core_input(&state, &right, 1U);
    }
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalNone, 0U, true, 2U);
    assert(strcmp(fake.calls, "TLHXIS") == 0);
    assert(!state.tx_prepared && !state.snapshot.hardware_active && !state.snapshot.tx_active);
    mf_radio_core_leave(&state);
    fake.high_ok = true;

    fake.count = 0U;
    fake.calls[0] = '\0';
    fake.low_ok = false;
    assert(mf_radio_core_enter(&state, &args, &ops, &result));
    mf_radio_core_set_page(&state, MfRadioPageTransmit, 0U);
    {
        InputEvent right = {.key = InputKeyRight, .type = InputTypeShort};
        mf_radio_core_input(&state, &right, 1U);
    }
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalNone, 0U, true, 2U);
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalMark, 100U, false, 102U);
    assert(strcmp(fake.calls, "TLXIS") == 0);
    assert(!state.tx_prepared && !state.snapshot.hardware_active && !state.snapshot.tx_active);
    mf_radio_core_leave(&state);
    fake.low_ok = true;

    /* Losing TX permission tears down an active tone or static carrier immediately. */
    fake.count = 0U;
    fake.calls[0] = '\0';
    assert(mf_radio_core_enter(&state, &args, &ops, &result));
    mf_radio_core_set_page(&state, MfRadioPageTransmit, 0U);
    {
        InputEvent right = {.key = InputKeyRight, .type = InputTypeShort};
        mf_radio_core_input(&state, &right, 1U);
    }
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalNone, 0U, true, 2U);
    fake.allowed = false;
    result = mf_radio_core_sync_tx(&state, MfRadioTxIntervalNone, 0U, true, 3U);
    assert(result.handled && result.redraw && strcmp(fake.calls, "TLHXIS") == 0);
    assert(!state.tx_prepared && !state.snapshot.hardware_active && !state.snapshot.tx_active);
    mf_radio_core_leave(&state);

    fake.allowed = true;
    fake.count = 0U;
    fake.calls[0] = '\0';
    assert(mf_radio_core_enter(&state, &args, &ops, &result));
    mf_radio_core_set_page(&state, MfRadioPageTransmit, 0U);
    {
        InputEvent right = {.key = InputKeyRight, .type = InputTypeShort};
        mf_radio_core_input(&state, &right, 1U);
        mf_radio_core_sync_tx(&state, MfRadioTxIntervalNone, 0U, true, 2U);
        mf_radio_core_sync_tx(&state, MfRadioTxIntervalMark, 100U, false, 102U);
        fake.allowed = false;
        result = mf_radio_core_input(&state, &right, 103U);
        assert(result.handled && !result.redraw && state.tx_mode == MfRadioTxModeCwfm);
        assert(strcmp(fake.calls, "TLHLXIS") == 0);
        assert(!state.tx_prepared && !state.snapshot.hardware_active && !state.snapshot.tx_active);
    }
    mf_radio_core_leave(&state);
    fake.allowed = true;

    /* The CWFM hang deadline remains exact across uint32_t wrap. */
    fake.count = 0U;
    fake.calls[0] = '\0';
    assert(mf_radio_core_enter(&state, &args, &ops, &result));
    mf_radio_core_set_page(&state, MfRadioPageTransmit, 0U);
    {
        InputEvent right = {.key = InputKeyRight, .type = InputTypeShort};
        mf_radio_core_input(&state, &right, 1U);
    }
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalNone, 0U, true, UINT32_MAX - 300U);
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalMark, 100U, false, UINT32_MAX - 200U);
    assert(state.tx_idle_at == 499U && state.tx_idle_pending);
    mf_radio_core_tick(&state, 498U);
    assert(strcmp(fake.calls, "TLHL") == 0);
    mf_radio_core_tick(&state, 499U);
    assert(strcmp(fake.calls, "TLHLXIS") == 0);
    mf_radio_core_leave(&state);

    /* Faster keying still gets the fixed 500 ms minimum hang. */
    fake.count = 0U;
    fake.calls[0] = '\0';
    args.dit_ms = 60U;
    assert(mf_radio_core_enter(&state, &args, &ops, &result));
    mf_radio_core_set_page(&state, MfRadioPageTransmit, 0U);
    {
        InputEvent right = {.key = InputKeyRight, .type = InputTypeShort};
        mf_radio_core_input(&state, &right, 1U);
    }
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalNone, 0U, true, 10U);
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalMark, 60U, false, 70U);
    assert(state.tx_idle_at == 570U);
    mf_radio_core_tick(&state, 569U);
    assert(strcmp(fake.calls, "TLHL") == 0);
    mf_radio_core_tick(&state, 570U);
    assert(strcmp(fake.calls, "TLHLXIS") == 0);
    mf_radio_core_set_page(&state, MfRadioPageTransmit, 571U);
    assert(state.tx_mode == MfRadioTxModeOok);
    mf_radio_core_leave(&state);
    args = enter_args();

    fake.count = 0U;
    fake.calls[0] = '\0';
    assert(mf_radio_core_enter(&state, &args, &ops, &result));
    fake.rx_ok = false;
    result = mf_radio_core_set_page(&state, MfRadioPageReceive, 1U);
    assert(result.handled && strcmp(fake.calls, "RIS") == 0);
    assert(mf_radio_core_snapshot(&state, &snapshot));
    assert(snapshot.page == MfRadioPageIdle && !snapshot.hardware_active);
    mf_radio_core_leave(&state);
    assert(strcmp(fake.calls, "RIS") == 0);

    fake.count = 0U;
    fake.calls[0] = '\0';
    fake.rx_ok = true;
    fake.tx_ok = false;
    assert(mf_radio_core_enter(&state, &args, &ops, &result));
    mf_radio_core_set_page(&state, MfRadioPageTransmit, 1U);
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalNone, 0U, true, 2U);
    assert(strcmp(fake.calls, "TXIS") == 0);
    mf_radio_core_leave(&state);
    assert(strcmp(fake.calls, "TXIS") == 0);

    fake.count = 0U;
    fake.calls[0] = '\0';
    fake.tx_ok = true;
    args.dit_ms = 60U;
    assert(mf_radio_core_enter(&state, &args, &ops, &result));
    mf_radio_core_set_page(&state, MfRadioPageReceive, 1U);
    assert(strcmp(fake.calls, "R") == 0);
    mf_radio_core_tick(&state, 1U);
    assert(!state.snapshot.monitor_tone && !state.rx_level);
    mf_radio_core_set_page(&state, MfRadioPageIdle, 2U);
    assert(strcmp(fake.calls, "RIS") == 0);
    mf_radio_core_set_page(&state, MfRadioPageTransmit, 3U);
    assert(state.decoder_services->dit_ms(&state.decoder) == 60U);
    mf_radio_core_leave(&state);
    assert(strcmp(fake.calls, "RIS") == 0);

    puts("test_radio_fal_lifecycle: passed");
    return 0;
}
