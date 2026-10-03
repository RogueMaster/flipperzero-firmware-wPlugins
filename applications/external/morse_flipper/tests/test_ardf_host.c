#include "morse_flipper_ardf_host.h"
#include "morse_flipper_ardf_host_test.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

struct DialogMessage {
    int unused;
};
struct TextInput {
    int unused;
};

const GpioPin gpio_ext_pc0 = {0};
const GpioPin gpio_ext_pc1 = {0};
const int sequence_display_backlight_on = 1;
const int sequence_display_backlight_off = 2;

typedef struct {
    MfArdfSnapshot snapshot;
    MfArdfClockTime draft;
    MorseFlipperMappedFalResult next_tick;
    unsigned inputs;
    unsigned ticks;
    unsigned commands;
    unsigned texts;
    unsigned actions;
    unsigned activations;
    unsigned leaves;
    bool frequency_allowed;
} FakeArdf;

static FakeArdf ardf;
static MorseFlipperApp* active_app;
static unsigned mutex_depth;
static unsigned applies;
static unsigned allocations;
static unsigned frees;
static unsigned sidetones;
static unsigned redraws;
static unsigned notes;
static unsigned pwm_stops;
static unsigned vibro_stops;
static unsigned gpio_inits;
static unsigned gpio_writes;
static unsigned gpio_restores;
static unsigned dialogs;
static unsigned navigations;
static unsigned switches;
static unsigned backlight_ons;
static unsigned backlight_offs;
static unsigned text_resets;
static unsigned text_frees;
static unsigned text_headers;
static unsigned text_callbacks;
static unsigned custom_events;
static struct TextInput fake_text_input;

static void assert_unlocked(void) {
    assert(mutex_depth == 0U);
}

uint32_t furi_get_tick(void) {
    return 99U;
}

void furi_mutex_acquire(FuriMutex* mutex, uint32_t timeout) {
    (void)mutex;
    (void)timeout;
    assert_unlocked();
    mutex_depth++;
}

void furi_mutex_release(FuriMutex* mutex) {
    (void)mutex;
    assert(mutex_depth == 1U);
    mutex_depth--;
}

void furi_hal_gpio_init(const GpioPin* pin, int mode, int pull, int speed) {
    (void)pin;
    (void)mode;
    (void)pull;
    (void)speed;
    assert_unlocked();
    gpio_inits++;
}

void furi_hal_gpio_write(const GpioPin* pin, bool level) {
    (void)pin;
    (void)level;
    assert_unlocked();
    gpio_writes++;
}

void furi_hal_vibro_on(bool on) {
    assert_unlocked();
    assert(!on);
    vibro_stops++;
}

void notification_message(void* notifications, const int* sequence) {
    assert_unlocked();
    assert(notifications != NULL);
    if(sequence == &sequence_display_backlight_on)
        backlight_ons++;
    else if(sequence == &sequence_display_backlight_off)
        backlight_offs++;
    else
        assert(false);
}

void morse_flipper_gpio_apply(MorseFlipperApp* app) {
    (void)app;
    assert_unlocked();
    gpio_restores++;
}

void morse_flipper_update_sidetone(MorseFlipperApp* app) {
    (void)app;
    assert_unlocked();
    sidetones++;
}

void morse_flipper_view_dirty(MorseFlipperApp* app) {
    (void)app;
    assert_unlocked();
    redraws++;
}

void morse_flipper_release_all_notes(MorseFlipperApp* app) {
    (void)app;
    assert_unlocked();
    notes++;
}

void morse_flipper_audio_pwm_stop(MorseFlipperAudioPwm* pwm) {
    (void)pwm;
    assert_unlocked();
    pwm_stops++;
}

void morse_flipper_ensure_view(MorseFlipperApp* app, uint8_t view) {
    assert_unlocked();
    if(view == MorseFlipperViewTextInput && app->text_input == NULL)
        app->text_input = &fake_text_input;
}

void variable_item_list_reset(void* list) {
    assert_unlocked();
    assert(list != NULL);
}

void* variable_item_list_add(
    void* list,
    const char* label,
    uint8_t values,
    void* callback,
    void* context) {
    (void)label;
    (void)values;
    (void)callback;
    (void)context;
    assert_unlocked();
    return list;
}

void view_dispatcher_switch_to_view(void* dispatcher, uint8_t view) {
    (void)dispatcher;
    (void)view;
    assert_unlocked();
    switches++;
}

void view_dispatcher_send_custom_event(void* dispatcher, uint32_t event) {
    (void)dispatcher;
    assert(event == MorseFlipperCustomArdfTextDone);
    assert_unlocked();
    custom_events++;
}

void view_dispatcher_remove_view(void* dispatcher, uint8_t view) {
    (void)dispatcher;
    (void)view;
    assert_unlocked();
}

void text_input_reset(TextInput* input) {
    assert_unlocked();
    assert(input != NULL);
    text_resets++;
}

void text_input_set_header_text(TextInput* input, const char* text) {
    assert_unlocked();
    assert(input != NULL && strcmp(text, "Custom identifier") == 0);
    text_headers++;
}

void text_input_set_result_callback(
    TextInput* input,
    void (*callback)(void*),
    void* context,
    char* text,
    size_t text_size,
    bool clear_default_text) {
    assert_unlocked();
    assert(input != NULL && callback != NULL && context != NULL && text != NULL);
    assert(text_size == MF_ARDF_CUSTOM_CAPACITY + 1U && !clear_default_text);
    text_callbacks++;
}

void text_input_free(TextInput* input) {
    assert_unlocked();
    assert(input != NULL);
    text_frees++;
}

size_t strlcpy(char* destination, const char* source, size_t size) {
    size_t length = strlen(source);
    if(size != 0U) {
        size_t copied = length < size - 1U ? length : size - 1U;
        memcpy(destination, source, copied);
        destination[copied] = '\0';
    }
    return length;
}

static void* api_alloc(void) {
    allocations++;
    return &ardf;
}

static void api_free(void* state) {
    assert(state == &ardf);
    frees++;
}

static bool api_enter(void* state, const void* args, MorseFlipperMappedFalResult* initial) {
    const MfArdfEnterArgs* enter_args = args;
    assert(mutex_depth == 1U && state == &ardf);
    assert(enter_args->struct_size == sizeof(*enter_args));
    *initial = (MorseFlipperMappedFalResult){.redraw = true};
    return true;
}

static void api_leave(void* state) {
    assert(mutex_depth == 1U && state == &ardf);
    ardf.leaves++;
}

static MorseFlipperMappedFalResult
    api_input(void* state, const InputEvent* event, uint32_t now_ms) {
    (void)now_ms;
    assert(mutex_depth == 1U && state == &ardf && event != NULL);
    ardf.inputs++;
    if(ardf.snapshot.view != MfArdfViewClock)
        return (MorseFlipperMappedFalResult){.handled = false};
    if(ardf.snapshot.clock_state == MfArdfClockConfirm && event->key == InputKeyRight) {
        ardf.snapshot.clock_state = MfArdfClockSelect;
        ardf.snapshot.clock_field = MfArdfClockSeconds;
    } else if(ardf.snapshot.clock_state == MfArdfClockSelect && event->key == InputKeyOk) {
        ardf.snapshot.clock_state = MfArdfClockEdit;
        ardf.draft = (MfArdfClockTime){23U, 59U, 59U};
    } else if(ardf.snapshot.clock_state == MfArdfClockEdit && event->key == InputKeyUp) {
        ardf.draft.second = (uint8_t)((ardf.draft.second + 1U) % 60U);
    } else if(ardf.snapshot.clock_state == MfArdfClockConfirm && event->key == InputKeyOk) {
        if(ardf.frequency_allowed) {
            ardf.snapshot.view = MfArdfViewRun;
            ardf.snapshot.running = true;
            ardf.snapshot.gpio_owned = true;
            ardf.snapshot.run_pending = true;
        } else {
            ardf.snapshot.error = MfArdfErrorFrequency;
            ardf.snapshot.host_action = MfArdfHostActionShowError;
        }
    } else {
        return (MorseFlipperMappedFalResult){0};
    }
    return (MorseFlipperMappedFalResult){
        .handled = true,
        .redraw = true,
        .phase = ardf.snapshot.view,
        .transition = ardf.snapshot.run_pending,
        .feedback = ardf.snapshot.host_action,
    };
}

static MorseFlipperMappedFalResult api_tick(void* state, uint32_t now_ms) {
    (void)now_ms;
    assert(mutex_depth == 1U && state == &ardf);
    ardf.ticks++;
    MorseFlipperMappedFalResult result = ardf.next_tick;
    result.phase = ardf.snapshot.view;
    ardf.next_tick = (MorseFlipperMappedFalResult){.redraw = true};
    return result;
}

static void api_draw(void* state, Canvas* canvas, uint32_t now_ms) {
    (void)state;
    (void)canvas;
    (void)now_ms;
}

static MorseFlipperMappedFalResult
    api_command(void* state, uint32_t command, const void* input, void* output, uint32_t now_ms) {
    (void)now_ms;
    assert(mutex_depth == 1U && state == &ardf);
    ardf.commands++;
    MorseFlipperMappedFalResult result = {.handled = true, .redraw = true};
    if(command == MfArdfCommandPopulateSettings) {
        assert(input != NULL);
    } else if(command == MfArdfCommandHostActionInfo) {
        MfArdfHostActionInfo* info = output;
        assert(info != NULL);
        if(ardf.snapshot.host_action == MfArdfHostActionOpenTextInput)
            *info = (MfArdfHostActionInfo){.header = "Custom identifier", .text = "FOX"};
        else if(ardf.snapshot.host_action == MfArdfHostActionShowError)
            *info =
                (MfArdfHostActionInfo){.header = "ARDF error", .text = "Frequency unavailable"};
        else
            *info = (MfArdfHostActionInfo){.header = "Stop ARDF Fox?", .confirm = true};
    } else if(command == MfArdfCommandTextResult) {
        const MfArdfTextResultCommand* text = input;
        assert(text != NULL && strcmp(text->text, "FOX") == 0);
        ardf.texts++;
        ardf.snapshot.host_action = MfArdfHostActionNone;
        ardf.snapshot.settings.selected_row = MF_ARDF_CUSTOM_ROW;
        ardf.snapshot.view = MfArdfViewSettings;
    } else if(command == MfArdfCommandHostActionResult) {
        const MfArdfHostActionResultCommand* action = input;
        assert(action != NULL && action->accepted);
        ardf.actions++;
        ardf.snapshot.host_action = MfArdfHostActionNone;
        ardf.snapshot.error = MfArdfErrorNone;
        ardf.snapshot.view = MfArdfViewSettings;
        ardf.snapshot.gpio_owned = false;
    } else if(command == MfArdfCommandActivateRun) {
        assert(active_app != NULL && active_app->ardf_gpio_owned);
        assert(ardf.snapshot.run_pending);
        ardf.activations++;
        ardf.snapshot.run_pending = false;
        ardf.snapshot.transmitting = true;
        result.playback_active = true;
        result.playback_mark = true;
        result.backlight_wake = true;
    } else {
        assert(false);
    }
    result.phase = ardf.snapshot.view;
    result.feedback = ardf.snapshot.host_action;
    return result;
}

static const MfArdfApi api = {
    .fal =
        {
            .mapped =
                {
                    .magic = MF_ARDF_API_MAGIC,
                    .api_version = MF_ARDF_API_VERSION,
                    .struct_size = sizeof(MfArdfApi),
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
    (void)mode;
    assert(mutex_depth == 1U && owner == MorseFlipperPluginOwnerArdf);
    assert(strcmp(path, "plugins/morse_flipper_ardf.fal") == 0);
    assert(api_version == MF_ARDF_API_VERSION && api_magic == MF_ARDF_API_MAGIC);
    assert(minimum_api_size == sizeof(MfArdfApi));
    app->plugin_slot.owner = owner;
    app->plugin_slot.error = MorseFlipperPluginErrorNone;
    app->plugin_slot.manager = &ardf;
    app->plugin_slot.api = &api;
    app->plugin_slot.state = api.fal.mapped.alloc();
    active_app = app;
    return api.fal.mapped.enter(app->plugin_slot.state, enter_args, initial);
}

void morse_flipper_plugin_runtime_apply_result_locked(
    MorseFlipperApp* app,
    MorseFlipperMappedFalResult result,
    uint32_t now_ms) {
    (void)now_ms;
    assert(mutex_depth == 1U);
    applies++;
    app->plugin_slot.playback_active = result.playback_active;
    app->plugin_slot.playback_mark = result.playback_mark;
}

bool morse_flipper_plugin_runtime_call(
    MorseFlipperApp* app,
    MorseFlipperPluginOwner owner,
    uint32_t operation,
    const void* input,
    void* output,
    uint32_t now_ms,
    MorseFlipperMappedFalResult* result) {
    assert(app != NULL && result != NULL);
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
    morse_flipper_plugin_runtime_apply_result_locked(app, *result, now_ms);
    if(operation >= MORSE_FLIPPER_MAPPED_INPUT && output != NULL)
        (void)api.fal.command(app->plugin_slot.state, 0U, NULL, output, now_ms);
    furi_mutex_release(app->plugin_slot.mutex);
    return true;
}

void morse_flipper_plugin_runtime_release_claim_locked(
    MorseFlipperApp* app,
    MorseFlipperPluginOwner owner) {
    assert(mutex_depth == 1U && app->plugin_slot.owner == owner);
    app->plugin_slot.owner = MorseFlipperPluginOwnerNone;
}

void morse_flipper_plugin_runtime_detach_locked(
    MorseFlipperApp* app,
    MorseFlipperPluginOwner owner) {
    assert(mutex_depth == 1U && app->plugin_slot.owner == owner);
    api.fal.mapped.leave(app->plugin_slot.state);
    api.fal.mapped.free(app->plugin_slot.state);
    app->plugin_slot.owner = MorseFlipperPluginOwnerNone;
    app->plugin_slot.api = NULL;
    app->plugin_slot.state = NULL;
    app->plugin_slot.manager = NULL;
    active_app = NULL;
}

void morse_flipper_plugin_runtime_unload_current(MorseFlipperApp* app) {
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    if(app->plugin_slot.owner != MorseFlipperPluginOwnerNone)
        morse_flipper_plugin_runtime_detach_locked(app, app->plugin_slot.owner);
    furi_mutex_release(app->plugin_slot.mutex);
    morse_flipper_release_all_notes(app);
    morse_flipper_update_sidetone(app);
}

DialogMessage* dialog_message_alloc(void) {
    static DialogMessage message;
    assert_unlocked();
    dialogs++;
    return &message;
}
void dialog_message_free(DialogMessage* message) {
    (void)message;
    assert_unlocked();
}
void dialog_message_set_header(
    DialogMessage* message,
    const char* text,
    int x,
    int y,
    int horizontal,
    int vertical) {
    (void)message;
    (void)text;
    (void)x;
    (void)y;
    (void)horizontal;
    (void)vertical;
    assert_unlocked();
}
void dialog_message_set_text(
    DialogMessage* message,
    const char* text,
    int x,
    int y,
    int horizontal,
    int vertical) {
    dialog_message_set_header(message, text, x, y, horizontal, vertical);
}
void dialog_message_set_buttons(
    DialogMessage* message,
    const char* left,
    const char* center,
    const char* right) {
    (void)message;
    (void)left;
    (void)center;
    (void)right;
    assert_unlocked();
}
int dialog_message_show(void* service, DialogMessage* message) {
    (void)service;
    (void)message;
    assert_unlocked();
    return DialogMessageButtonCenter;
}

bool morse_flipper_host_dialog(MorseFlipperApp* app, const MorseFlipperHostDialog* info) {
    DialogMessage* message = dialog_message_alloc();
    dialog_message_set_header(message, info->header, 64, 16, AlignCenter, AlignTop);
    if(info->text != NULL)
        dialog_message_set_text(message, info->text, 64, 34, AlignCenter, AlignCenter);
    dialog_message_set_buttons(message, NULL, "OK", info->confirm ? "Back" : NULL);
    bool accepted = dialog_message_show(app->dialogs, message) == DialogMessageButtonCenter;
    dialog_message_free(message);
    return accepted;
}
void scene_manager_next_scene(void* manager, uint32_t scene) {
    (void)manager;
    (void)scene;
    assert_unlocked();
    navigations++;
}
void scene_manager_search_and_switch_to_another_scene(void* manager, uint32_t scene) {
    scene_manager_next_scene(manager, scene);
}

int main(void) {
    FuriMutex mutex = {0};
    struct TextInput text_input = {0};
    int settings_list = 1;
    int service = 1;
    InputEvent right = {.key = InputKeyRight, .type = InputTypeShort};
    InputEvent ok = {.key = InputKeyOk, .type = InputTypeShort};
    InputEvent up = {.key = InputKeyUp, .type = InputTypeShort};
    MorseFlipperApp app = {
        .plugin_slot.mutex = &mutex,
        .settings_list = &settings_list,
        .scene_manager = &service,
        .dialogs = &service,
        .notifications = &service,
        .view_dispatcher = &service,
        .rf_frequency_hz = 433160000U,
        .scene = MorseFlipperSceneArdf,
        .ardf_view = MORSE_FLIPPER_ARDF_VIEW_NONE,
    };

    ardf.snapshot = (MfArdfSnapshot){
        .struct_size = sizeof(MfArdfSnapshot),
        .view = MfArdfViewClock,
        .clock_state = MfArdfClockConfirm,
    };
    strcpy(ardf.snapshot.settings.custom, "FOX");
    ardf.frequency_allowed = true;
    assert(mf_ardf_api_valid(&api));
    assert(morse_flipper_ardf_host_open(&app, 10U));
    assert(applies == 1U && sidetones == 1U && redraws == 1U);

    unsigned gpio_before = gpio_writes;
    unsigned init_before = gpio_inits;
    unsigned nav_before = navigations;
    unsigned light_before = backlight_ons + backlight_offs;
    assert(morse_flipper_ardf_host_input(&app, &right, 11U));
    assert(morse_flipper_ardf_host_input(&app, &ok, 12U));
    assert(morse_flipper_ardf_host_input(&app, &up, 13U));
    assert(ardf.snapshot.clock_state == MfArdfClockEdit && ardf.draft.second == 0U);
    assert(!app.ardf_gpio_owned && !ardf.snapshot.run_pending);
    assert(gpio_writes == gpio_before && gpio_inits == init_before);
    assert(navigations == nav_before && backlight_ons + backlight_offs == light_before);

    ardf.snapshot.clock_state = MfArdfClockConfirm;
    assert(morse_flipper_ardf_host_input(&app, &ok, 14U));
    assert(ardf.activations == 1U && app.ardf_gpio_owned);
    assert(gpio_inits == init_before + 2U && backlight_ons == 1U);
    assert(app.plugin_slot.playback_active && app.plugin_slot.playback_mark);

    ardf.next_tick = (MorseFlipperMappedFalResult){.redraw = true, .backlight_off = true};
    morse_flipper_ardf_host_tick(&app, 15U);
    assert(backlight_offs == 1U && !app.ardf_backlight_wake_active);
    assert(app.ardf_gpio_owned);

    ardf.snapshot.view = MfArdfViewClock;
    ardf.snapshot.clock_state = MfArdfClockConfirm;
    ardf.snapshot.gpio_owned = false;
    app.ardf_view = MfArdfViewClock;
    app.ardf_gpio_owned = false;
    ardf.frequency_allowed = false;
    gpio_before = gpio_writes;
    init_before = gpio_inits;
    assert(morse_flipper_ardf_host_input(&app, &ok, 16U));
    assert(dialogs == 1U && ardf.actions == 1U);
    assert(gpio_writes == gpio_before && gpio_inits == init_before);
    assert(ardf.snapshot.view == MfArdfViewSettings);

    ardf.snapshot.host_action = MfArdfHostActionOpenTextInput;
    ardf.snapshot.view = MfArdfViewSettings;
    ardf.next_tick.feedback = MfArdfHostActionOpenTextInput;
    unsigned ticks_before = ardf.ticks;
    morse_flipper_ardf_host_tick(&app, 17U);
    assert(app.ardf_view == MORSE_FLIPPER_ARDF_VIEW_TEXT);
    assert(app.text_input == &fake_text_input && strcmp(app.ardf_text, "FOX") == 0);
    assert(text_resets == 1U && text_headers == 1U && text_callbacks == 1U);
    assert(navigations == 0U && app.plugin_slot.owner == MorseFlipperPluginOwnerArdf);
    morse_flipper_ardf_host_tick(&app, 18U);
    assert(ardf.ticks == ticks_before + 1U);
    assert(morse_flipper_ardf_host_text_result(&app, "FOX", true, 19U));
    assert(ardf.texts == 1U && ardf.snapshot.settings.selected_row == MF_ARDF_CUSTOM_ROW);
    assert(
        app.ardf_view == MfArdfViewSettings &&
        app.plugin_slot.owner == MorseFlipperPluginOwnerArdf);
    assert(navigations == 0U);

    ardf.snapshot.host_action = MfArdfHostActionOpenTextInput;
    ardf.next_tick.feedback = MfArdfHostActionOpenTextInput;
    morse_flipper_ardf_host_tick(&app, 20U);
    assert(app.ardf_view == MORSE_FLIPPER_ARDF_VIEW_TEXT);
    assert(morse_flipper_ardf_host_text_result(&app, "FOX", false, 21U));
    assert(ardf.texts == 2U && ardf.snapshot.settings.selected_row == MF_ARDF_CUSTOM_ROW);
    assert(app.ardf_view == MfArdfViewSettings && navigations == 0U);

    ardf.next_tick = (MorseFlipperMappedFalResult){.redraw = true, .backlight_wake = true};
    morse_flipper_ardf_host_tick(&app, 22U);
    assert(backlight_ons == 2U && app.ardf_backlight_wake_active);
    app.text_input = &text_input;
    app.ardf_gpio_owned = true;
    unsigned text_resets_before_close = text_resets;
    unsigned text_frees_before_close = text_frees;
    morse_flipper_ardf_host_close(&app);
    assert(backlight_offs == 2U && !app.ardf_backlight_wake_active);
    assert(
        text_resets == text_resets_before_close && text_frees == text_frees_before_close &&
        app.text_input == &text_input);
    app.text_input = NULL;
    assert(ardf.leaves == 1U && app.plugin_slot.owner == MorseFlipperPluginOwnerNone);
    assert(notes == 1U && pwm_stops == 1U && vibro_stops == 1U);
    assert(gpio_restores == 1U && mutex_depth == 0U);
    assert(navigations == 0U && switches >= 2U);

    for(unsigned i = 0U; i < 25U; i++) {
        assert(morse_flipper_ardf_host_open(&app, 20U + i));
        morse_flipper_ardf_host_close(&app);
    }
    assert(allocations == 26U && frees == 26U && ardf.leaves == 26U);
    assert(mutex_depth == 0U);

    puts("test_ardf_host: passed");
    return 0;
}
