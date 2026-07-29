#include "mf_radio_core.h"
#include "mf_radio_draw.h"
#include "morse_flipper_radio_host.h"

#include <assert.h>
#include <gui/canvas.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    bool tx_ok;
    bool rx_ok;
    bool valid;
    bool allowed;
    bool carrier;
    int8_t rssi;
    unsigned tx_prepares;
    unsigned rx_prepares;
    unsigned level_writes;
} FakeHardware;

static char drawn[128];
static char draw_log[512];
static char diagnostic[32];
static int32_t diagnostic_x;
static unsigned rx_draws;
static unsigned box_draws;

static void log_drawn_text(const char* text) {
    size_t len = strlen(draw_log);
    if(len != 0U && len + 1U < sizeof(draw_log)) draw_log[len++] = '|';
    snprintf(draw_log + len, sizeof(draw_log) - len, "%s", text);
}

static bool prepare_tx(void* context, uint32_t frequency_hz) {
    (void)frequency_hz;
    FakeHardware* fake = context;
    fake->tx_prepares++;
    return fake->tx_ok;
}
static bool prepare_rx(void* context, uint32_t frequency_hz) {
    (void)frequency_hz;
    FakeHardware* fake = context;
    fake->rx_prepares++;
    return fake->rx_ok;
}
static void set_level(void* context, bool level) {
    (void)level;
    ((FakeHardware*)context)->level_writes++;
}
static bool read_carrier(void* context) {
    return ((FakeHardware*)context)->carrier;
}
static int8_t read_rssi(void* context) {
    return ((FakeHardware*)context)->rssi;
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
    (void)context;
    return MF_RADIO_DEFAULT_FREQUENCY_HZ;
}
static void idle(void* context) {
    (void)context;
}
static void sleep_radio(void* context) {
    (void)context;
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

static void draw_history(
    void* context,
    Canvas* canvas,
    const MorseFlipperRunHistory* history,
    uint8_t preview,
    bool preview_extendable,
    const char* frequency_line) {
    (void)context;
    (void)canvas;
    (void)preview;
    (void)preview_extendable;
    snprintf(drawn, sizeof(drawn), "%s|%s", morse_flipper_run_history_text(history), frequency_line);
}

static void draw_rx_text(
    void* context,
    Canvas* canvas,
    const char* text,
    uint8_t preview,
    bool preview_extendable) {
    (void)context;
    (void)canvas;
    (void)preview;
    (void)preview_extendable;
    rx_draws++;
    snprintf(drawn, sizeof(drawn), "%s", text);
}

static MfRadioEnterArgs args(void) {
    static const MfRadioDrawServices draw = {
        .struct_size = sizeof(MfRadioDrawServices),
        .history_reset = morse_flipper_run_history_reset,
        .history_append = morse_flipper_run_history_append,
        .draw_tx_history = draw_history,
        .draw_rx_text = draw_rx_text,
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

void canvas_set_font(Canvas* canvas, Font font) {
    (void)canvas;
    (void)font;
}
void canvas_set_color(Canvas* canvas, Color color) {
    (void)canvas;
    (void)color;
}
void canvas_draw_str(Canvas* canvas, int32_t x, int32_t y, const char* text) {
    (void)canvas;
    (void)y;
    snprintf(drawn, sizeof(drawn), "%s", text);
    if(strncmp(text, "cs", 2U) == 0) {
        snprintf(diagnostic, sizeof(diagnostic), "%s", text);
        diagnostic_x = x;
    }
    log_drawn_text(text);
}
void canvas_draw_str_aligned(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    Align horizontal,
    Align vertical,
    const char* text) {
    (void)canvas;
    (void)x;
    (void)y;
    (void)horizontal;
    (void)vertical;
    snprintf(drawn, sizeof(drawn), "%s", text);
    log_drawn_text(text);
}
uint32_t canvas_string_width(Canvas* canvas, const char* text) {
    (void)canvas;
    return (uint32_t)strlen(text) * 6U;
}
void canvas_draw_dot(Canvas* canvas, int32_t x, int32_t y) {
    (void)canvas;
    (void)x;
    (void)y;
}
void canvas_draw_box(Canvas* canvas, int32_t x, int32_t y, int32_t width, int32_t height) {
    (void)canvas;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    box_draws++;
}
void canvas_draw_line(Canvas* canvas, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    (void)canvas;
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
}
void canvas_draw_frame(Canvas* canvas, int32_t x, int32_t y, int32_t width, int32_t height) {
    canvas_draw_box(canvas, x, y, width, height);
}
void canvas_draw_rframe(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    int32_t radius) {
    (void)radius;
    canvas_draw_box(canvas, x, y, width, height);
}
void canvas_draw_rbox(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    int32_t radius) {
    (void)radius;
    canvas_draw_box(canvas, x, y, width, height);
}
void canvas_draw_triangle(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    int32_t base,
    int32_t height,
    CanvasDirection direction) {
    (void)canvas;
    (void)x;
    (void)y;
    (void)base;
    (void)height;
    (void)direction;
}

static InputEvent event(InputKey key, InputType type) {
    return (InputEvent){.key = key, .type = type};
}

static void tick_samples(MfRadioState* state, uint32_t* now_ms, unsigned count) {
    while(count-- != 0U) {
        mf_radio_core_tick(state, *now_ms);
        *now_ms += MF_RADIO_RX_SAMPLE_MS;
    }
}

int main(void) {
    FakeHardware fake = {
        .tx_ok = true,
        .rx_ok = true,
        .valid = true,
        .allowed = true,
        .rssi = -80,
    };
    MfRadioHardwareOps ops = hardware(&fake);
    MfRadioEnterArgs enter = args();
    MfRadioState state;
    MfRadioSnapshot snapshot = {.struct_size = sizeof(MfRadioSnapshot)};
    MorseFlipperMappedFalResult result;
    Canvas canvas = {0};
    InputEvent input;
    uint32_t now;

    assert(mf_radio_core_enter(&state, &enter, &ops, &result));
    assert(mf_radio_core_snapshot(&state, &snapshot));
    assert(snapshot.frequency_hz == MF_RADIO_DEFAULT_FREQUENCY_HZ);
    enter.frequency_hz = 0U;
    assert(mf_radio_core_enter(&state, &enter, &ops, &result));
    assert(mf_radio_core_snapshot(&state, &snapshot) && snapshot.frequency_dirty);
    enter = args();

    assert(mf_radio_core_enter(&state, &enter, &ops, &result));
    mf_radio_core_set_page(&state, MfRadioPageTransmit, 0U);
    input = event(InputKeyBack, InputTypeShort);
    assert(!mf_radio_core_input(&state, &input, 0U).handled);
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalNone, 0U, true, 1U);
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalMark, 100U, false, 101U);
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalSpace, 300U, false, 401U);
    assert(strstr(morse_flipper_run_history_text(&state.tx_history), "E") != NULL);
    drawn[0] = '\0';
    mf_radio_draw(&state, &canvas, 401U);
    assert(strstr(drawn, "khz") != NULL);
    mf_radio_core_leave(&state);

    /*
     * A partial idle flush may finish the character before the next mark.
     * The completed idle interval must still be accepted later so it can
     * promote that gap to a word separator.
     */
    assert(mf_radio_core_enter(&state, &enter, &ops, &result));
    mf_radio_core_set_page(&state, MfRadioPageTransmit, 0U);
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalNone, 0U, true, 1U);
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalMark, 60U, false, 61U);
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalSpace, 150U, false, 211U);
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalSpace, 420U, true, 481U);
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalMark, 180U, false, 661U);
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalSpace, 180U, false, 841U);
    assert(strstr(morse_flipper_run_history_text(&state.tx_history), "E T") != NULL);
    mf_radio_core_leave(&state);

    fake.allowed = false;
    fake.tx_prepares = 0U;
    assert(mf_radio_core_enter(&state, &enter, &ops, &result));
    mf_radio_core_set_page(&state, MfRadioPageTransmit, 0U);
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalNone, 0U, true, 1U);
    assert(fake.tx_prepares == 0U);
    mf_radio_draw(&state, &canvas, 1U);
    assert(strcmp(drawn, "TX Blocked") == 0);
    mf_radio_core_leave(&state);

    fake.allowed = true;
    fake.carrier = false;
    fake.rssi = -50;
    assert(mf_radio_core_enter(&state, &enter, &ops, &result));
    mf_radio_core_set_page(&state, MfRadioPageReceive, 0U);
    assert(fake.rx_prepares != 0U);
    now = 0U;
    tick_samples(&state, &now, MF_RADIO_RX_CAL_SETTLE_SAMPLES);
    assert(state.rx_calibrating);
    assert(!state.rx_level && !state.snapshot.monitor_tone && state.ticker.count == 0U);
    fake.rssi = -106;
    tick_samples(&state, &now, 2U);
    fake.rssi = -60;
    tick_samples(&state, &now, 2U);
    fake.rssi = -106;
    tick_samples(&state, &now, MF_RADIO_RX_CAL_SAMPLES - 4U);
    assert(!state.rx_calibrating);
    assert(state.snapshot.monitor_threshold_dbm == -100);
    assert(!state.rx_level && !state.snapshot.monitor_tone && state.ticker.count == 0U);
    fake.rssi = -65;
    tick_samples(&state, &now, MF_RADIO_RX_STABLE_SAMPLES);
    assert(mf_radio_core_snapshot(&state, &snapshot));
    assert(snapshot.monitor_tone);
    assert(snapshot.monitor_threshold_dbm == -100);
    /* Four-sample debounce is sufficient; a 4 dB latch would stay open here. */
    fake.rssi = -102;
    tick_samples(&state, &now, MF_RADIO_RX_STABLE_SAMPLES);
    assert(mf_radio_core_snapshot(&state, &snapshot));
    assert(!snapshot.monitor_tone);
    rx_draws = 0U;
    box_draws = 0U;
    draw_log[0] = '\0';
    state.carrier_present = true;
    state.rssi_valid = true;
    state.rssi_dbm = -81;
    state.rssi_peak_dbm = -70;
    state.snapshot.monitor_threshold_dbm = -76;
    diagnostic[0] = '\0';
    mf_radio_draw(&state, &canvas, now);
    assert(rx_draws == 1U);
    assert(box_draws >= 5U);
    assert(strstr(draw_log, "wpm 10") != NULL);
    assert(strcmp(diagnostic, "cs1 r-81 t-76") == 0);
    assert(diagnostic_x >= 0);
    assert((uint32_t)diagnostic_x + canvas_string_width(&canvas, diagnostic) <= 128U);
    state.rssi_dbm = -115;
    state.snapshot.monitor_threshold_dbm = -115;
    mf_radio_draw(&state, &canvas, now);
    assert(strcmp(diagnostic, "cs1 r-115 t-115") == 0);
    assert((uint32_t)diagnostic_x + canvas_string_width(&canvas, diagnostic) <= 128U);
    state.snapshot.monitor_threshold_dbm = -100;
    state.decoder.dit_ms = 80U;
    state.decoder.dit_sample_count = MF_RADIO_RX_AUTO_WPM_SAMPLES;
    draw_log[0] = '\0';
    mf_radio_draw(&state, &canvas, now);
    assert(strstr(draw_log, "auto wpm 15.0") != NULL);
    input = event(InputKeyOk, InputTypeShort);
    assert(mf_radio_core_input(&state, &input, now).handled);
    assert(mf_radio_core_snapshot(&state, &snapshot));
    assert(!snapshot.receive_audio_enabled && !snapshot.monitor_tone);
    input = event(InputKeyRight, InputTypeRepeat);
    mf_radio_core_input(&state, &input, now);
    assert(state.snapshot.monitor_threshold_dbm == -99);
    fake.rssi = -50;
    tick_samples(&state, &now, 40U);
    assert(state.snapshot.monitor_threshold_dbm == -99);
    input = event(InputKeyUp, InputTypeShort);
    mf_radio_core_input(&state, &input, now);
    assert(state.rx_wpm_hint == 11U);
    input = event(InputKeyBack, InputTypeLong);
    assert(mf_radio_core_input(&state, &input, now).request_exit);
    mf_radio_core_leave(&state);

    /* Each Receive entry starts a fresh, session-local calibration and offset. */
    enter.receive_audio_enabled = true;
    assert(mf_radio_core_enter(&state, &enter, &ops, &result));
    mf_radio_core_set_page(&state, MfRadioPageReceive, 1000U);
    assert(state.snapshot.monitor_threshold_dbm == -95);
    assert(state.rx_manual_threshold_offset_db == 0);
    now = 1000U;
    fake.carrier = false;
    fake.rssi = -79;
    tick_samples(
        &state,
        &now,
        MF_RADIO_RX_CAL_SETTLE_SAMPLES + MF_RADIO_RX_CAL_SAMPLES);
    assert(state.snapshot.monitor_threshold_dbm == -73);
    mf_radio_core_set_page(&state, MfRadioPageIdle, now);
    mf_radio_core_set_page(&state, MfRadioPageReceive, now);
    assert(state.rx_calibrating && state.snapshot.monitor_threshold_dbm == -95);
    assert(!state.snapshot.monitor_tone);
    mf_radio_core_leave(&state);

    /* A carrier spanning calibration gets one prompt, downward-only recovery. */
    assert(mf_radio_core_enter(&state, &enter, &ops, &result));
    mf_radio_core_set_page(&state, MfRadioPageReceive, 2000U);
    now = 2000U;
    fake.carrier = true;
    fake.rssi = -65;
    tick_samples(
        &state,
        &now,
        MF_RADIO_RX_CAL_SETTLE_SAMPLES + MF_RADIO_RX_CAL_SAMPLES);
    assert(state.snapshot.monitor_threshold_dbm == -59);
    assert(state.rx_recovery_pending && !state.snapshot.monitor_tone);
    input = event(InputKeyLeft, InputTypeShort);
    mf_radio_core_input(&state, &input, now);
    assert(state.snapshot.monitor_threshold_dbm == -60);
    fake.rssi = -50;
    tick_samples(&state, &now, MF_RADIO_RX_STABLE_SAMPLES);
    assert(state.snapshot.monitor_tone);
    /* Idle carrier sense may remain asserted on a noisy band. RSSI drop recovers. */
    fake.carrier = true;
    fake.rssi = -106;
    tick_samples(&state, &now, 1U);
    assert(!state.snapshot.monitor_tone && !state.rx_level);
    tick_samples(
        &state,
        &now,
        MF_RADIO_RX_CAL_SETTLE_SAMPLES + MF_RADIO_RX_CAL_SAMPLES - 2U);
    assert(state.rx_recovery_pending && state.snapshot.monitor_threshold_dbm == -60);
    assert(!state.snapshot.monitor_tone);
    tick_samples(&state, &now, 1U);
    assert(!state.rx_recovery_pending);
    assert(state.snapshot.monitor_threshold_dbm == -101);
    assert(!state.snapshot.monitor_tone);
    mf_radio_core_leave(&state);

    assert(mf_radio_core_enter(&state, &enter, &ops, &result));
    mf_radio_core_set_page(&state, MfRadioPageReceive, 3000U);
    now = 3000U;
    fake.carrier = true;
    fake.rssi = -100;
    tick_samples(
        &state,
        &now,
        MF_RADIO_RX_CAL_SETTLE_SAMPLES + MF_RADIO_RX_CAL_SAMPLES);
    assert(state.snapshot.monitor_threshold_dbm == -94);
    fake.carrier = false;
    fake.rssi = -70;
    tick_samples(
        &state,
        &now,
        MF_RADIO_RX_CAL_SETTLE_SAMPLES + MF_RADIO_RX_CAL_SAMPLES);
    assert(state.rx_recovery_pending);
    assert(state.snapshot.monitor_threshold_dbm == -94);
    mf_radio_core_leave(&state);

    /* Valid threshold limits still apply at both ends of the RSSI range. */
    assert(mf_radio_core_enter(&state, &enter, &ops, &result));
    mf_radio_core_set_page(&state, MfRadioPageReceive, 4000U);
    now = 4000U;
    fake.carrier = false;
    fake.rssi = -127;
    tick_samples(
        &state,
        &now,
        MF_RADIO_RX_CAL_SETTLE_SAMPLES + MF_RADIO_RX_CAL_SAMPLES);
    assert(state.snapshot.monitor_threshold_dbm == -115);
    mf_radio_core_set_page(&state, MfRadioPageIdle, now);
    mf_radio_core_set_page(&state, MfRadioPageReceive, now);
    fake.rssi = -50;
    tick_samples(
        &state,
        &now,
        MF_RADIO_RX_CAL_SETTLE_SAMPLES + MF_RADIO_RX_CAL_SAMPLES);
    assert(state.snapshot.monitor_threshold_dbm == -50);
    mf_radio_core_leave(&state);

    assert(mf_radio_core_enter(&state, &enter, &ops, &result));
    mf_radio_core_set_page(&state, MfRadioPageFrequency, 0U);
    input = event(InputKeyRight, InputTypeShort);
    mf_radio_core_input(&state, &input, 1U);
    assert(state.frequency_focus == 1U);
    input = event(InputKeyLeft, InputTypeRepeat);
    mf_radio_core_input(&state, &input, 2U);
    assert(state.frequency_focus == 0U);
    input = event(InputKeyUp, InputTypeShort);
    mf_radio_core_input(&state, &input, 3U);
    input = event(InputKeyDown, InputTypeRepeat);
    mf_radio_core_input(&state, &input, 4U);
    input = event(InputKeyBack, InputTypeShort);
    assert(mf_radio_core_input(&state, &input, 5U).request_exit);
    state.edit_khz = 100000U;
    draw_log[0] = '\0';
    mf_radio_draw(&state, &canvas, 5U);
    assert(strstr(draw_log, "RX not available") != NULL);
    assert(strstr(draw_log, "PLL lock failed") != NULL);
    mf_radio_core_leave(&state);

    puts("test_radio_fal: passed");
    return 0;
}
