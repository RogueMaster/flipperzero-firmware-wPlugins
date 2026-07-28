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

static MfRadioEnterArgs args(void) {
    static const MfRadioDrawServices draw = {
        .struct_size = sizeof(MfRadioDrawServices),
        .history_reset = morse_flipper_run_history_reset,
        .history_append = morse_flipper_run_history_append,
        .draw_tx_history = draw_history,
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
    (void)x;
    (void)y;
    snprintf(drawn, sizeof(drawn), "%s", text);
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

    assert(mf_radio_core_enter(&state, &enter, &ops, &result));
    assert(mf_radio_core_snapshot(&state, &snapshot));
    assert(snapshot.frequency_hz == MF_RADIO_DEFAULT_FREQUENCY_HZ);
    enter.frequency_hz = 0U;
    assert(mf_radio_core_enter(&state, &enter, &ops, &result));
    assert(mf_radio_core_snapshot(&state, &snapshot) && snapshot.frequency_dirty);
    enter = args();

    assert(mf_radio_core_enter(&state, &enter, &ops, &result));
    mf_radio_core_set_page(&state, MfRadioPageTransmit, 0U);
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalNone, 0U, true, 1U);
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalMark, 100U, false, 101U);
    mf_radio_core_sync_tx(&state, MfRadioTxIntervalSpace, 300U, false, 401U);
    assert(strstr(morse_flipper_run_history_text(&state.tx_history), "E") != NULL);
    drawn[0] = '\0';
    mf_radio_draw(&state, &canvas, 401U);
    assert(strstr(drawn, "khz") != NULL);
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
    assert(mf_radio_core_enter(&state, &enter, &ops, &result));
    mf_radio_core_set_page(&state, MfRadioPageReceive, 0U);
    assert(fake.rx_prepares != 0U);
    for(uint32_t now = 1U; now <= 33U; now += 8U) mf_radio_core_tick(&state, now);
    assert(mf_radio_core_snapshot(&state, &snapshot));
    assert(snapshot.monitor_tone);
    input = event(InputKeyOk, InputTypeShort);
    assert(mf_radio_core_input(&state, &input, 40U).handled);
    assert(mf_radio_core_snapshot(&state, &snapshot));
    assert(!snapshot.receive_audio_enabled && !snapshot.monitor_tone);
    input = event(InputKeyRight, InputTypeRepeat);
    mf_radio_core_input(&state, &input, 41U);
    assert(state.snapshot.monitor_threshold_dbm == -94);
    input = event(InputKeyUp, InputTypeShort);
    mf_radio_core_input(&state, &input, 42U);
    assert(state.rx_wpm_hint == 11U);
    mf_radio_draw(&state, &canvas, 42U);
    input = event(InputKeyBack, InputTypeLong);
    assert(mf_radio_core_input(&state, &input, 43U).request_exit);
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
    mf_radio_draw(&state, &canvas, 5U);
    mf_radio_core_leave(&state);

    puts("test_radio_fal: passed");
    return 0;
}
