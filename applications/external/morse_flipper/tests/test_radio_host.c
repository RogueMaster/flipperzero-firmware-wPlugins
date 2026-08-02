#include "morse_flipper_radio_host_test.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    MfRadioSnapshot snapshot;
    unsigned set_pages;
    unsigned inputs;
    unsigned ticks;
    unsigned syncs;
    unsigned draws;
    unsigned leaves;
} FakeRadio;

static FakeRadio radio;
static unsigned mutex_depth;
static unsigned opens;
static unsigned detaches;
static unsigned sidetones;
static unsigned ptt_syncs;
static unsigned saves;
static unsigned redraws;
static unsigned releases;
static unsigned backs;
static unsigned unavailable;
static bool open_ok;

static void assert_unlocked(void) {
    assert(mutex_depth == 0U);
}

uint32_t furi_get_tick(void) {
    return 99U;
}

void furi_mutex_acquire(FuriMutex* mutex, uint32_t timeout) {
    (void)mutex;
    (void)timeout;
    assert(mutex_depth == 0U);
    mutex_depth++;
}

void furi_mutex_release(FuriMutex* mutex) {
    (void)mutex;
    assert(mutex_depth == 1U);
    mutex_depth--;
}

static void decoder_init(MorseFlipperCwDecoder* decoder, uint16_t dit_ms) {
    (void)decoder;
    (void)dit_ms;
}
static void decoder_feed(MorseFlipperCwDecoder* decoder, uint16_t duration_ms) {
    (void)decoder;
    (void)duration_ms;
}
static uint16_t decoder_dit(const MorseFlipperCwDecoder* decoder) {
    (void)decoder;
    return 60U;
}
static const char* decoder_output(const MorseFlipperCwDecoder* decoder) {
    (void)decoder;
    return "";
}
static void decoder_clear(MorseFlipperCwDecoder* decoder) {
    (void)decoder;
}
static uint8_t decoder_preview(const MorseFlipperCwDecoder* decoder) {
    (void)decoder;
    return 0U;
}
static bool decoder_extendable(const MorseFlipperCwDecoder* decoder) {
    (void)decoder;
    return false;
}

static const MfRadioDecoderServices decoder_services = {
    .struct_size = sizeof(MfRadioDecoderServices),
    .init = decoder_init,
    .feed_mark = decoder_feed,
    .feed_space = decoder_feed,
    .dit_ms = decoder_dit,
    .output = decoder_output,
    .clear_output = decoder_clear,
    .preview = decoder_preview,
    .preview_extendable = decoder_extendable,
};

uint16_t morse_flipper_current_dit_ms(const MorseFlipperApp* app) {
    (void)app;
    assert_unlocked();
    return 60U;
}

const MfRadioDecoderServices* morse_flipper_radio_decoder_services(void) {
    assert_unlocked();
    return &decoder_services;
}

void morse_flipper_run_history_reset(MorseFlipperRunHistory* history) {
    (void)history;
}

void morse_flipper_run_history_append(MorseFlipperRunHistory* history, const char* text) {
    (void)history;
    (void)text;
}

void morse_flipper_draw_tx_history_supplied(
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

void morse_flipper_draw_radio_rx_text(
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

static void* api_alloc(void) {
    return &radio;
}
static void api_free(void* state) {
    assert(state == &radio);
}
static bool api_enter(void* state, const void* args, MorseFlipperMappedFalResult* initial) {
    (void)state;
    (void)args;
    (void)initial;
    return true;
}
static void api_leave(void* state) {
    assert(mutex_depth == 1U && state == &radio);
    radio.leaves++;
}
static MorseFlipperMappedFalResult
    api_input(void* state, const InputEvent* event, uint32_t now_ms) {
    (void)now_ms;
    assert(mutex_depth == 1U && state == &radio);
    radio.inputs++;
    if(event->key == InputKeyBack)
        return (MorseFlipperMappedFalResult){.handled = true, .request_exit = true};
    if(event->key == InputKeyRight) {
        radio.snapshot.monitor_threshold_dbm = -70;
        return (MorseFlipperMappedFalResult){.handled = true, .redraw = true};
    }
    radio.snapshot.frequency_hz++;
    radio.snapshot.frequency_dirty = true;
    return (MorseFlipperMappedFalResult){.handled = true, .redraw = true};
}
static MorseFlipperMappedFalResult api_tick(void* state, uint32_t now_ms) {
    (void)now_ms;
    assert(mutex_depth == 1U && state == &radio);
    radio.ticks++;
    radio.snapshot.monitor_tone = true;
    return (MorseFlipperMappedFalResult){.redraw = true};
}
static void api_draw(void* state, Canvas* canvas, uint32_t now_ms) {
    (void)canvas;
    (void)now_ms;
    assert(mutex_depth == 1U && state == &radio);
    radio.draws++;
}
static MorseFlipperMappedFalResult
    api_command(void* state, uint32_t command, const void* input, void* output, uint32_t now_ms) {
    (void)now_ms;
    assert(mutex_depth == 1U && state == &radio);
    MorseFlipperMappedFalResult result = {.handled = true, .redraw = true};
    if(command == MfRadioCommandSetPage) {
        MfRadioPage page = *(const MfRadioPage*)input;
        radio.set_pages++;
        radio.snapshot.page = page;
        if(page == MfRadioPageIdle) {
            radio.snapshot.hardware_active = false;
            radio.snapshot.tx_active = false;
            radio.snapshot.monitor_tone = false;
        }
    } else if(command == MfRadioCommandSyncTx) {
        const MfRadioSyncTxCommand* sync = input;
        assert(sync != NULL);
        radio.syncs++;
        radio.snapshot.hardware_active = true;
        radio.snapshot.tx_active = sync->level;
    } else {
        assert(command == MfRadioCommandSnapshot);
    }
    if(output != NULL) *(MfRadioSnapshot*)output = radio.snapshot;
    return result;
}

static const MfRadioApi api = {
    .fal =
        {
            .mapped =
                {
                    .magic = MF_RADIO_API_MAGIC,
                    .api_version = MF_RADIO_API_VERSION,
                    .struct_size = sizeof(MfRadioApi),
                    .alloc = api_alloc,
                    .free = api_free,
                    .enter = api_enter,
                    .leave = api_leave,
                    .input = api_input,
                    .tick = api_tick,
                    .draw = api_draw,
                },
            .command = api_command,
        },
};

bool morse_flipper_plugin_runtime_open_mapped_locked(
    MorseFlipperApp* app,
    MorseFlipperPluginOwner owner,
    uint8_t mode,
    const char* path,
    uint32_t api_version,
    uint32_t api_magic,
    uint32_t minimum_api_size,
    const void* enter_args,
    MorseFlipperMappedFalResult* initial) {
    const MfRadioEnterArgs* args = enter_args;
    (void)mode;
    assert(mutex_depth == 1U);
    assert(owner == MorseFlipperPluginOwnerRadio);
    assert(strcmp(path, "plugins/morse_flipper_radio.fal") == 0);
    assert(api_version == MF_RADIO_API_VERSION && api_magic == MF_RADIO_API_MAGIC);
    assert(minimum_api_size == sizeof(MfRadioApi));
    assert(args->struct_size == sizeof(MfRadioEnterArgs));
    assert(mf_radio_decoder_services_valid(args->decoder));
    assert(mf_radio_draw_services_valid(args->draw));
    opens++;
    app->plugin_slot.owner = owner;
    if(!open_ok) {
        app->plugin_slot.error = MorseFlipperPluginErrorLoad;
        return false;
    }
    app->plugin_slot.error = MorseFlipperPluginErrorNone;
    app->plugin_slot.manager = &radio;
    app->plugin_slot.api = &api;
    app->plugin_slot.state = &radio;
    if(initial != NULL) initial->redraw = true;
    return true;
}

void morse_flipper_plugin_runtime_release_claim_locked(
    MorseFlipperApp* app,
    MorseFlipperPluginOwner owner) {
    assert(mutex_depth == 1U && app->plugin_slot.owner == owner);
    app->plugin_slot.owner = MorseFlipperPluginOwnerNone;
    app->plugin_slot.error = MorseFlipperPluginErrorNone;
}

void morse_flipper_plugin_runtime_detach_locked(
    MorseFlipperApp* app,
    MorseFlipperPluginOwner owner) {
    assert(mutex_depth == 1U && app->plugin_slot.owner == owner);
    api.fal.mapped.leave(app->plugin_slot.state);
    app->plugin_slot.owner = MorseFlipperPluginOwnerNone;
    app->plugin_slot.error = MorseFlipperPluginErrorNone;
    app->plugin_slot.manager = NULL;
    app->plugin_slot.api = NULL;
    app->plugin_slot.state = NULL;
    detaches++;
}

bool morse_flipper_plugin_runtime_call(
    MorseFlipperApp* app,
    MorseFlipperPluginOwner owner,
    uint32_t operation,
    const void* input,
    void* output,
    uint32_t now_ms,
    MorseFlipperMappedFalResult* result) {
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    if(app->plugin_slot.owner != owner || app->plugin_slot.api != &api) {
        furi_mutex_release(app->plugin_slot.mutex);
        return false;
    }
    if(operation == MORSE_FLIPPER_MAPPED_INPUT)
        *result = api.fal.mapped.input(app->plugin_slot.state, input, now_ms);
    else if(operation == MORSE_FLIPPER_MAPPED_TICK)
        *result = api.fal.mapped.tick(app->plugin_slot.state, now_ms);
    else
        *result = api.fal.command(app->plugin_slot.state, operation, input, output, now_ms);
    if(operation >= MORSE_FLIPPER_MAPPED_INPUT && output != NULL)
        (void)api.fal.command(app->plugin_slot.state, 0U, NULL, output, now_ms);
    furi_mutex_release(app->plugin_slot.mutex);
    return true;
}

void morse_flipper_plugin_runtime_draw(MorseFlipperApp* app, Canvas* canvas, uint32_t now_ms) {
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    bool active = app->plugin_slot.owner == MorseFlipperPluginOwnerRadio &&
                  app->plugin_slot.api == &api;
    if(active) api.fal.mapped.draw(app->plugin_slot.state, canvas, now_ms);
    furi_mutex_release(app->plugin_slot.mutex);
    if(!active) morse_flipper_draw_plugin_unavailable(canvas);
}

bool morse_flipper_plugin_runtime_snapshot(
    const MorseFlipperApp* app,
    MorseFlipperPluginSnapshot* snapshot) {
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    snapshot->owner = app->plugin_slot.owner;
    snapshot->active = app->plugin_slot.error == MorseFlipperPluginErrorNone &&
                       app->plugin_slot.manager != NULL && app->plugin_slot.api != NULL &&
                       app->plugin_slot.state != NULL;
    furi_mutex_release(app->plugin_slot.mutex);
    return true;
}

void morse_flipper_update_sidetone(MorseFlipperApp* app) {
    (void)app;
    assert_unlocked();
    sidetones++;
}
void morse_flipper_sync_ptt(MorseFlipperApp* app, uint32_t now_ms) {
    (void)app;
    (void)now_ms;
    assert_unlocked();
    ptt_syncs++;
}
void morse_flipper_save_config(const MorseFlipperApp* app) {
    (void)app;
    assert_unlocked();
    saves++;
}
void morse_flipper_view_dirty(MorseFlipperApp* app) {
    (void)app;
    assert_unlocked();
    redraws++;
}
void morse_flipper_release_all_notes(MorseFlipperApp* app) {
    (void)app;
    assert_unlocked();
    releases++;
}
void morse_flipper_handle_active_keying_event(MorseFlipperApp* app, const InputEvent* event) {
    (void)app;
    (void)event;
    assert_unlocked();
}
void morse_flipper_scene_back(MorseFlipperApp* app) {
    (void)app;
    assert_unlocked();
    backs++;
}
void morse_flipper_draw_plugin_unavailable(Canvas* canvas) {
    (void)canvas;
    assert_unlocked();
    unavailable++;
}

int main(void) {
    FuriMutex mutex = {0};
    Canvas* canvas = (Canvas*)(uintptr_t)1U;
    InputEvent event = {.key = InputKeyOk, .type = InputTypeShort};
    MfRadioApi invalid_api;
    MfRadioDrawServices invalid_draw;
    MorseFlipperApp app = {
        .plugin_slot.mutex = &mutex,
        .rf_frequency_hz = MF_RADIO_DEFAULT_FREQUENCY_HZ,
        .rf_monitor_threshold_dbm = -95,
        .rf_rx_audio_enabled = true,
        .screen = MorseFlipperScreenRf,
    };

    assert(mf_radio_api_valid(&api));
    invalid_api = api;
    invalid_api.fal.mapped.magic++;
    assert(!mf_radio_api_valid(&invalid_api));
    invalid_api = api;
    invalid_api.fal.mapped.api_version++;
    assert(!mf_radio_api_valid(&invalid_api));
    invalid_api = api;
    invalid_api.fal.mapped.struct_size--;
    assert(!mf_radio_api_valid(&invalid_api));
    invalid_api = api;
    invalid_api.fal.command = NULL;
    assert(!mf_radio_api_valid(&invalid_api));
    invalid_draw = (MfRadioDrawServices){
        .struct_size = sizeof(MfRadioDrawServices),
        .history_reset = morse_flipper_run_history_reset,
        .history_append = morse_flipper_run_history_append,
        .draw_tx_history = morse_flipper_draw_tx_history_supplied,
    };
    assert(!mf_radio_draw_services_valid(&invalid_draw));

    radio.snapshot = (MfRadioSnapshot){
        .struct_size = sizeof(MfRadioSnapshot),
        .page = MfRadioPageIdle,
        .frequency_hz = MF_RADIO_DEFAULT_FREQUENCY_HZ,
        .monitor_threshold_dbm = -95,
        .receive_audio_enabled = true,
        .tx_allowed = true,
    };
    open_ok = true;
    assert(morse_flipper_radio_host_open(&app, 10U));
    assert(opens == 1U && morse_flipper_radio_host_active(&app));
    assert(sidetones == 1U && ptt_syncs == 1U && redraws == 1U);
    assert(morse_flipper_radio_host_set_page(&app, MfRadioPageTransmit, 11U));
    morse_flipper_radio_host_sync_tx(&app, MfRadioTxIntervalNone, 0U, true, 12U);
    assert(radio.syncs == 1U && app.radio_tx_active);
    morse_flipper_radio_host_tick(&app, 13U);
    assert(radio.ticks == 1U && app.radio_monitor_tone);
    assert(morse_flipper_radio_host_input(&app, &event, 14U));
    assert(saves == 1U && app.rf_frequency_hz == MF_RADIO_DEFAULT_FREQUENCY_HZ + 1U);
    assert(app.rf_monitor_threshold_dbm == -95);
    event = (InputEvent){.key = InputKeyRight, .type = InputTypeShort};
    assert(morse_flipper_radio_host_input(&app, &event, 15U));
    assert(saves == 1U && app.rf_monitor_threshold_dbm == -95);
    morse_flipper_radio_host_draw(&app, canvas, 15U);
    assert(radio.draws == 1U && unavailable == 0U);

    event = (InputEvent){.key = InputKeyBack, .type = InputTypeShort};
    assert(morse_flipper_radio_host_input(&app, &event, 16U));
    assert(releases == 1U && backs == 1U);
    assert(radio.snapshot.page == MfRadioPageIdle && !app.radio_monitor_tone);
    assert(morse_flipper_radio_host_active(&app));

    morse_flipper_radio_host_close(&app, 17U);
    assert(detaches == 1U && radio.leaves == 1U);
    assert(!morse_flipper_radio_host_active(&app));
    assert(!app.radio_tx_active && !app.radio_monitor_tone);

    open_ok = false;
    assert(!morse_flipper_radio_host_open(&app, 20U));
    assert(app.radio_load_error && app.plugin_slot.owner == MorseFlipperPluginOwnerNone);
    morse_flipper_radio_host_draw(&app, canvas, 21U);
    assert(unavailable == 1U);
    assert(morse_flipper_radio_host_input(&app, &event, 22U));
    assert(backs == 2U);
    assert(radio.inputs == 3U && radio.ticks == 1U && radio.syncs == 1U && radio.draws == 1U);
    assert(mutex_depth == 0U);

    puts("test_radio_host: passed");
    return 0;
}
