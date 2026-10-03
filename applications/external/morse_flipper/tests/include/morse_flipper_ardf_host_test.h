#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "plugins/ardf/mf_ardf_api.h"

#define APP_ASSETS_PATH(path)        path
#define FuriWaitForever              0U
#define MORSE_FLIPPER_MAPPED_INPUT   (UINT32_MAX - 1U)
#define MORSE_FLIPPER_MAPPED_TICK    UINT32_MAX
#define MORSE_FLIPPER_ARDF_VIEW_NONE 0xFFU
#define MORSE_FLIPPER_ARDF_VIEW_TEXT 0xFEU

typedef struct {
    int unused;
} FuriMutex;
typedef struct {
    int unused;
} MorseFlipperAudioPwm;
typedef struct {
    int unused;
} GpioPin;
typedef struct DialogMessage DialogMessage;
typedef struct TextInput TextInput;

typedef enum {
    MorseFlipperPluginOwnerNone = 0,
    MorseFlipperPluginOwnerArdf = 8,
} MorseFlipperPluginOwner;

typedef enum {
    MorseFlipperPluginErrorNone = 0,
    MorseFlipperPluginErrorLoad = 2,
} MorseFlipperPluginError;

typedef struct {
    FuriMutex* mutex;
    void* manager;
    const void* api;
    void* state;
    uint8_t owner;
    uint8_t error;
    uint8_t phase;
    bool playback_active;
    bool playback_mark;
} MorseFlipperPluginSlot;

enum {
    MorseFlipperSceneMenuRf = 40,
    MorseFlipperSceneArdf,
};

enum {
    MorseFlipperCustomArdfTextDone = 0x1C01,
};

enum {
    MorseFlipperViewSettings = 10,
    MorseFlipperViewLive,
    MorseFlipperViewTextInput,
};

enum {
    GpioModeOutputPushPull,
    GpioPullNo,
    GpioSpeedLow,
    AlignCenter,
    AlignTop,
    DialogMessageButtonCenter,
};

typedef struct MorseFlipperApp {
    MorseFlipperPluginSlot plugin_slot;
    void* settings_list;
    void* scene_manager;
    void* dialogs;
    void* notifications;
    void* view_dispatcher;
    TextInput* text_input;
    MorseFlipperAudioPwm audio_pwm;
    uint32_t rf_frequency_hz;
    uint32_t ardf_loading_started_ms;
    uint8_t scene;
    uint8_t ardf_view;
    bool ardf_gpio_owned;
    bool ardf_backlight_wake_active;
    bool signal_led_on;
    bool signal_led_red;
    bool signal_led_green;
    char ardf_text[MF_ARDF_CUSTOM_CAPACITY + 1U];
} MorseFlipperApp;

extern const GpioPin gpio_ext_pc0;
extern const GpioPin gpio_ext_pc1;
extern const int sequence_display_backlight_on;
extern const int sequence_display_backlight_off;

uint32_t furi_get_tick(void);
void furi_mutex_acquire(FuriMutex* mutex, uint32_t timeout);
void furi_mutex_release(FuriMutex* mutex);
void furi_hal_gpio_init(const GpioPin* pin, int mode, int pull, int speed);
void furi_hal_gpio_write(const GpioPin* pin, bool level);
void furi_hal_vibro_on(bool on);
void notification_message(void* notifications, const int* sequence);
void morse_flipper_gpio_apply(MorseFlipperApp* app);
void morse_flipper_update_sidetone(MorseFlipperApp* app);
void morse_flipper_view_dirty(MorseFlipperApp* app);
void morse_flipper_release_all_notes(MorseFlipperApp* app);
void morse_flipper_audio_pwm_stop(MorseFlipperAudioPwm* pwm);
void morse_flipper_ensure_view(MorseFlipperApp* app, uint8_t view);
void variable_item_list_reset(void* list);
void* variable_item_list_add(
    void* list,
    const char* label,
    uint8_t values,
    void* callback,
    void* context);
void view_dispatcher_switch_to_view(void* dispatcher, uint8_t view);
void view_dispatcher_send_custom_event(void* dispatcher, uint32_t event);
void view_dispatcher_remove_view(void* dispatcher, uint8_t view);
void text_input_reset(TextInput* input);
void text_input_set_header_text(TextInput* input, const char* text);
void text_input_set_result_callback(
    TextInput* input,
    void (*callback)(void*),
    void* context,
    char* text,
    size_t text_size,
    bool clear_default_text);
void text_input_free(TextInput* input);
size_t strlcpy(char* destination, const char* source, size_t size);
bool morse_flipper_plugin_runtime_open_mapped_locked(
    MorseFlipperApp* app,
    MorseFlipperPluginOwner owner,
    uint8_t mode,
    const char* path,
    uint32_t api_version,
    uint32_t api_magic,
    uint32_t minimum_api_size,
    const void* enter_args,
    MorseFlipperMappedFalResult* initial);
bool morse_flipper_plugin_runtime_call(
    MorseFlipperApp* app,
    MorseFlipperPluginOwner owner,
    uint32_t operation,
    const void* input,
    void* output,
    uint32_t now_ms,
    MorseFlipperMappedFalResult* result);
void morse_flipper_plugin_runtime_apply_result_locked(
    MorseFlipperApp* app,
    MorseFlipperMappedFalResult result,
    uint32_t now_ms);
void morse_flipper_plugin_runtime_release_claim_locked(
    MorseFlipperApp* app,
    MorseFlipperPluginOwner owner);
void morse_flipper_plugin_runtime_detach_locked(
    MorseFlipperApp* app,
    MorseFlipperPluginOwner owner);
void morse_flipper_plugin_runtime_unload_current(MorseFlipperApp* app);
DialogMessage* dialog_message_alloc(void);
void dialog_message_free(DialogMessage* message);
void dialog_message_set_header(
    DialogMessage* message,
    const char* text,
    int x,
    int y,
    int horizontal,
    int vertical);
void dialog_message_set_text(
    DialogMessage* message,
    const char* text,
    int x,
    int y,
    int horizontal,
    int vertical);
void dialog_message_set_buttons(
    DialogMessage* message,
    const char* left,
    const char* center,
    const char* right);
int dialog_message_show(void* dialogs, DialogMessage* message);
bool morse_flipper_host_dialog(MorseFlipperApp* app, const MorseFlipperHostDialog* info);
void scene_manager_next_scene(void* manager, uint32_t scene);
void scene_manager_search_and_switch_to_another_scene(void* manager, uint32_t scene);

bool morse_flipper_ardf_host_open(MorseFlipperApp* app, uint32_t now_ms);
bool morse_flipper_ardf_host_input(MorseFlipperApp* app, const InputEvent* event, uint32_t now_ms);
void morse_flipper_ardf_host_tick(MorseFlipperApp* app, uint32_t now_ms);
bool morse_flipper_ardf_host_text_result(
    MorseFlipperApp* app,
    const char* text,
    bool accepted,
    uint32_t now_ms);
void morse_flipper_ardf_host_show_text(MorseFlipperApp* app);
bool morse_flipper_ardf_host_snapshot(const MorseFlipperApp* app, MfArdfSnapshot* snapshot);
void morse_flipper_ardf_host_close(void* context);
