// scenes/protopirate_scene_sub_decode.c
#include "../protopirate_app_i.h"
#ifdef ENABLE_SUB_DECODE_SCENE

#ifndef PROTOPIRATE_SUB_DECODE_PLUGIN_BUILD

void protopirate_scene_sub_decode_on_enter(void* context) {
    protopirate_tool_scene_on_enter(context, ProtoPirateToolScenePluginKindSubDecode);
}

bool protopirate_scene_sub_decode_on_event(void* context, SceneManagerEvent event) {
    return protopirate_tool_scene_on_event(context, event);
}

void protopirate_scene_sub_decode_on_exit(void* context) {
    protopirate_tool_scene_on_exit(context);
}

#else

#include "../helpers/protopirate_storage.h"
#include "../helpers/radio_device_loader.h"
#include "../helpers/raw_file_reader.h"
#include "../protopirate_history.h"
#include "../helpers/protopirate_psa_bf_host.h"
#include "../protocols/protocol_items.h"
#include "core/core_defines.h"
#include "core/record.h"
#include "storage/storage.h"
#include <dialogs/dialogs.h>
#include <stdio.h>
#include <string.h>
#include <lib/subghz/types.h>

#ifdef PROTOPIRATE_SUB_DECODE_PLUGIN_BUILD
#include "protopirate_sub_decode_plugin_icons.h"
#else
#include "proto_pirate_icons.h"
#endif

#define TAG "ProtoPirateSubDecode"

static const ProtoPirateToolSceneHostApi* g_tool_scene_host_api = NULL;

#define protopirate_ensure_receiver_view(app) g_tool_scene_host_api->ensure_receiver_view(app)
#define protopirate_ensure_widget(app)        g_tool_scene_host_api->ensure_widget(app)
#define protopirate_radio_init(app)           g_tool_scene_host_api->radio_init(app)
#define protopirate_rx_stack_resume_after_tx(app) \
    g_tool_scene_host_api->rx_stack_resume_after_tx(app)
#define protopirate_preset_init(app, preset_name, frequency, preset_data, preset_data_size) \
    g_tool_scene_host_api->preset_init(app, preset_name, frequency, preset_data, preset_data_size)
#define protopirate_refresh_protocol_registry(app, ensure_receiver_ready) \
    g_tool_scene_host_api->refresh_protocol_registry(app, ensure_receiver_ready)
#define protopirate_apply_protocol_registry_for_context(                       \
    app, preset_name, frequency, preset_data, preset_data_size, protocol_name) \
    g_tool_scene_host_api->apply_protocol_registry_for_context(                \
        app, preset_name, frequency, preset_data, preset_data_size, protocol_name)
#define protopirate_get_frequency_modulation_str(                \
    app, frequency, frequency_size, modulation, modulation_size) \
    g_tool_scene_host_api->get_frequency_modulation_str(         \
        app, frequency, frequency_size, modulation, modulation_size)
#define radio_device_loader_is_external(radio_device) \
    g_tool_scene_host_api->radio_device_is_external(radio_device)
#define protopirate_view_receiver_add_data_statusbar(   \
    receiver, frequency, modulation, history, external) \
    g_tool_scene_host_api->receiver_add_data_statusbar( \
        receiver, frequency, modulation, history, external)
#define protopirate_view_receiver_get_idx_menu(receiver) \
    g_tool_scene_host_api->receiver_get_idx_menu(receiver)
#define protopirate_view_receiver_set_idx_menu(receiver, idx) \
    g_tool_scene_host_api->receiver_set_idx_menu(receiver, idx)
#define protopirate_view_receiver_set_callback(receiver, callback, context) \
    g_tool_scene_host_api->receiver_set_callback(receiver, callback, context)
#define protopirate_view_receiver_set_sub_decode_mode(receiver, enabled) \
    g_tool_scene_host_api->receiver_set_sub_decode_mode(receiver, enabled)
#define protopirate_view_receiver_set_sub_decode_progress(receiver, progress) \
    g_tool_scene_host_api->receiver_set_sub_decode_progress(receiver, progress)
#define protopirate_view_receiver_reset_menu(receiver) \
    g_tool_scene_host_api->receiver_reset_menu(receiver)
#define protopirate_view_receiver_sync_menu_from_history(receiver, history) \
    g_tool_scene_host_api->receiver_sync_menu_from_history(receiver, history)
#define protopirate_psa_bf_plugin_ensure_loaded(app) \
    g_tool_scene_host_api->psa_bf_plugin_ensure_loaded(app)
#define protopirate_psa_bf_context_release(app) g_tool_scene_host_api->psa_bf_context_release(app)

#define SUBGHZ_APP_FOLDER EXT_PATH("subghz")

#define SAMPLES_TO_READ_PER_TICK 2048

#define SUB_DECODE_MAX_FILE_SIZE     (2U * 1024U * 1024U)
#define SUB_DECODE_PRESET_NAME_MAX   48U
#define SUB_DECODE_CUSTOM_PRESET_MAX 1024U

// Decode state machine
typedef enum {
    DecodeStateIdle,
    DecodeStateOpenFile,
    DecodeStateReadHeader,
    DecodeStateStartingWorker,
    DecodeStateDecodingRaw,
    DecodeStateShowHistory,
    DecodeStateShowSignalInfo,
    DecodeStateShowFailure,
    DecodeStateDone,
} DecodeState;

typedef enum {
    SubDecodeFailureGeneric,
    SubDecodeFailureCancelled,
    SubDecodeFailureNotRaw,
    SubDecodeFailureNoMatch,
} SubDecodeFailureKind;

// Context for the whole decode operation
typedef struct {
    DecodeState state;
    SubDecodeFailureKind failure_kind;
    uint8_t worker_startup_delay;
    uint64_t decode_elapsed_us;

    FuriString* file_path;
    FuriString* protocol_name;
    FuriString* result;
    FuriString* error_info;
    uint32_t frequency;

    Storage* storage;
    FlipperFormat* ff;

    ProtoPirateHistory* history;
    bool owns_history;
    uint16_t signal_count;
    uint16_t selected_history_index;
    bool showing_signal_info;
    bool signal_info_left_is_emulate;
    bool previous_preset_saved;
    char previous_preset_name[SUB_DECODE_PRESET_NAME_MAX];
    uint32_t previous_frequency;
    uint8_t* previous_preset_data;
    size_t previous_preset_data_size;
    uint8_t* custom_preset_data;
    size_t custom_preset_data_size;

    RawFileReader* raw_reader;
} SubDecodeContext;

static SubDecodeContext* g_decode_ctx = NULL;

static void protopirate_sub_decode_clear_custom_preset(SubDecodeContext* ctx) {
    if(ctx && ctx->custom_preset_data) {
        free(ctx->custom_preset_data);
        ctx->custom_preset_data = NULL;
        ctx->custom_preset_data_size = 0U;
    }
}

static bool protopirate_sub_decode_try_load_custom_preset(
    SubDecodeContext* ctx,
    FlipperFormat* flipper_format) {
    furi_check(ctx);
    furi_check(flipper_format);

    if(ctx->custom_preset_data && ctx->custom_preset_data_size > 0U) {
        return true;
    }

    protopirate_sub_decode_clear_custom_preset(ctx);

    uint32_t value_count = 0;
    flipper_format_rewind(flipper_format);
    if(!flipper_format_get_value_count(flipper_format, "Custom_preset_data", &value_count) ||
       value_count == 0U || value_count > SUB_DECODE_CUSTOM_PRESET_MAX) {
        return false;
    }

    uint8_t* preset_data = malloc(value_count);
    if(!preset_data) {
        return false;
    }

    flipper_format_rewind(flipper_format);
    if(!flipper_format_read_hex(flipper_format, "Custom_preset_data", preset_data, value_count)) {
        free(preset_data);
        return false;
    }

    ctx->custom_preset_data = preset_data;
    ctx->custom_preset_data_size = value_count;
    return true;
}

static const char* protopirate_sub_decode_find_matching_preset_name(
    ProtoPirateApp* app,
    const uint8_t* preset_data,
    size_t preset_data_size) {
    if(!app || !app->setting || !preset_data || preset_data_size == 0U) {
        return NULL;
    }

    const size_t preset_count = subghz_setting_get_preset_count(app->setting);
    for(size_t i = 0; i < preset_count; i++) {
        uint8_t* candidate_data = subghz_setting_get_preset_data(app->setting, i);
        size_t candidate_size = subghz_setting_get_preset_data_size(app->setting, i);
        if(candidate_data && candidate_size == preset_data_size &&
           memcmp(candidate_data, preset_data, preset_data_size) == 0) {
            return subghz_setting_get_preset_name(app->setting, i);
        }
    }

    return NULL;
}

static void protopirate_scene_sub_decode_update_receiver_statusbar(
    ProtoPirateApp* app,
    const SubDecodeContext* ctx) {
    char frequency_str[16] = {0};
    char modulation_str[8] = {0};
    char history_stat_str[16] = {0};

    protopirate_get_frequency_modulation_str(
        app, frequency_str, sizeof(frequency_str), modulation_str, sizeof(modulation_str));
    if(ctx && ctx->frequency > 0U) {
        snprintf(
            frequency_str,
            sizeof(frequency_str),
            "%03lu.%02lu",
            (unsigned long)((ctx->frequency / 1000000UL) % 1000UL),
            (unsigned long)((ctx->frequency / 10000UL) % 100UL));
    }

    const uint16_t signal_count =
        (ctx && ctx->history) ? protopirate_history_get_item(ctx->history) : 0U;
    snprintf(
        history_stat_str, sizeof(history_stat_str), "%u/%u", signal_count, PROTOPIRATE_HISTORY_MAX);

    bool is_external =
        app->txrx->radio_device ? radio_device_loader_is_external(app->txrx->radio_device) : false;
    protopirate_view_receiver_add_data_statusbar(
        app->protopirate_receiver, frequency_str, modulation_str, history_stat_str, is_external);
}

static void protopirate_scene_sub_decode_update_receiver_progress(
    ProtoPirateApp* app,
    const SubDecodeContext* ctx) {
    uint8_t progress = 0;
    bool should_update = true;
    if(ctx) {
        if(ctx->state == DecodeStateShowHistory || ctx->state == DecodeStateShowSignalInfo ||
           ctx->state == DecodeStateDone) {
            progress = 100;
        } else if(ctx->raw_reader) {
            progress = raw_file_reader_get_progress(ctx->raw_reader);
        } else if(ctx->state == DecodeStateShowFailure) {
            should_update = false;
        }
    }
    if(should_update) {
        protopirate_view_receiver_set_sub_decode_progress(app->protopirate_receiver, progress);
    }
}

void protopirate_subdecode_psa_bf_complete_refresh(void* app) {
    ProtoPirateApp* a = (ProtoPirateApp*)app;
    SubDecodeContext* ctx = g_decode_ctx;
    if(!a || !ctx) return;
    ctx->state = DecodeStateShowSignalInfo;
    view_dispatcher_send_custom_event(a->view_dispatcher, ProtoPirateCustomEventSubDecodeUpdate);
}

// Forward declaration
static void protopirate_scene_sub_decode_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context);

// Receiver view callback for history navigation
static void
    protopirate_scene_sub_decode_receiver_callback(ProtoPirateCustomEvent event, void* context);

// Callback when receiver successfully decodes a signal
static void protopirate_sub_decode_receiver_callback(
    SubGhzReceiver* receiver,
    SubGhzProtocolDecoderBase* decoder_base,
    void* context) {
    UNUSED(receiver);
    furi_check(context);
    ProtoPirateApp* app = context;
    SubDecodeContext* ctx = g_decode_ctx;

    if(!ctx || ctx->state != DecodeStateDecodingRaw) {
        return;
    }

    FURI_LOG_I(TAG, "=== SIGNAL DECODED FROM FILE ===");

    const uint32_t virtual_tick = (uint32_t)(ctx->decode_elapsed_us / 1000ULL);
    if(protopirate_history_add_to_history_at(
           ctx->history, decoder_base, app->txrx->preset, virtual_tick)) {
        ctx->signal_count++;
        FURI_LOG_I(TAG, "Added signal %u to history", ctx->signal_count);

        view_dispatcher_send_custom_event(
            app->view_dispatcher, ProtoPirateCustomEventSubDecodeUpdate);
    }
}

static void close_file_handles(SubDecodeContext* ctx) {
    if(ctx->ff) {
        flipper_format_free(ctx->ff);
        ctx->ff = NULL;
    }
    if(ctx->storage) {
        furi_record_close(RECORD_STORAGE);
        ctx->storage = NULL;
    }
}

static bool protopirate_scene_sub_decode_is_active_decode(const SubDecodeContext* ctx) {
    if(!ctx) return false;

    return ctx->state == DecodeStateOpenFile || ctx->state == DecodeStateReadHeader ||
           ctx->state == DecodeStateStartingWorker || ctx->state == DecodeStateDecodingRaw;
}

static bool
    protopirate_scene_sub_decode_cancel_active_decode(ProtoPirateApp* app, SubDecodeContext* ctx) {
    if(!app || !ctx || !protopirate_scene_sub_decode_is_active_decode(ctx)) {
        return false;
    }

    if(ctx->raw_reader) {
        raw_file_reader_free(ctx->raw_reader);
        ctx->raw_reader = NULL;
    }
    close_file_handles(ctx);

    if(app->txrx && app->txrx->receiver) {
        subghz_receiver_set_rx_callback(app->txrx->receiver, NULL, NULL);
    }

    furi_string_set(ctx->error_info, "Cancelled");
    furi_string_set(ctx->result, "Decode cancelled");
    ctx->failure_kind = SubDecodeFailureCancelled;
    ctx->state = DecodeStateShowFailure;
    return true;
}

static void
    protopirate_scene_sub_decode_receiver_callback(ProtoPirateCustomEvent event, void* context) {
    ProtoPirateApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

static void protopirate_scene_sub_decode_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    ProtoPirateApp* app = context;

    if(type == InputTypeShort || type == InputTypeLong) {
        if(result == GuiButtonTypeCenter) {
            SubDecodeContext* ctx = g_decode_ctx;
            if(ctx && !ctx->showing_signal_info) {
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, ProtoPirateCustomEventViewReceiverBack);
            }
        } else if(result == GuiButtonTypeRight) {
            // Save button in signal info view
            view_dispatcher_send_custom_event(
                app->view_dispatcher, ProtoPirateCustomEventSubDecodeSave);
        } else if(result == GuiButtonTypeLeft) {
            SubDecodeContext* ctx = g_decode_ctx;
            const uint32_t left_event = (ctx && ctx->signal_info_left_is_emulate) ?
                                            ProtoPirateCustomEventSubDecodeEmulate :
                                            ProtoPirateCustomEventSubDecodeBruteforceStart;
            view_dispatcher_send_custom_event(app->view_dispatcher, left_event);
        }
    }
}

static void
    protopirate_scene_sub_decode_draw_failure(ProtoPirateApp* app, const SubDecodeContext* ctx) {
    widget_reset(app->widget);

    const char* title = "Decode failed";
    char body[88] = {0};

    switch(ctx ? ctx->failure_kind : SubDecodeFailureGeneric) {
    case SubDecodeFailureCancelled:
        title = "Decode cancelled";
        snprintf(body, sizeof(body), "Decode stopped!\nNothing saved.");
        break;
    case SubDecodeFailureNotRaw:
        title = "RAW file needed";
        snprintf(body, sizeof(body), "Needs a SubGhz\nRAW capture\nfile.");
        break;
    case SubDecodeFailureNoMatch:
        title = "No match";
        if(ctx && ctx->frequency > 0U) {
            snprintf(
                body,
                sizeof(body),
                "%03lu.%02lu MHz\nNo ProtoPirate\nprotocol detected.",
                (unsigned long)((ctx->frequency / 1000000UL) % 1000UL),
                (unsigned long)((ctx->frequency / 10000UL) % 100UL));
        } else {
            snprintf(body, sizeof(body), "No ProtoPirate\nprotocol detected\nin this signal.");
        }
        break;
    default:
        snprintf(
            body,
            sizeof(body),
            "%s",
            (ctx && !furi_string_empty(ctx->result)) ? furi_string_get_cstr(ctx->result) :
                                                       "Try another RAW file.");
        break;
    }

    widget_add_string_element(app->widget, 64, 0, AlignCenter, AlignTop, FontPrimary, title);
    widget_add_icon_element(app->widget, 2, 16, &I_WarningDolphin_45x42);
    widget_add_string_multiline_element(
        app->widget, 50, 15, AlignLeft, AlignTop, FontSecondary, body);
    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "OK", protopirate_scene_sub_decode_widget_callback, app);
}

static bool protopirate_scene_sub_decode_select_file(SubDecodeContext* ctx) {
    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, ".sub", &I_subghz_10px);
    browser_options.base_path = SUBGHZ_APP_FOLDER;
    browser_options.hide_ext = false;

    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
    if(furi_string_empty(ctx->file_path)) {
        furi_string_set(ctx->file_path, SUBGHZ_APP_FOLDER);
    }

    const bool selected =
        dialog_file_browser_show(dialogs, ctx->file_path, ctx->file_path, &browser_options);
    furi_record_close(RECORD_DIALOGS);

    if(selected) {
        FURI_LOG_I(TAG, "Selected file: %s", furi_string_get_cstr(ctx->file_path));
    }
    return selected;
}

static void protopirate_scene_sub_decode_prepare_receiver_view(ProtoPirateApp* app) {
    protopirate_view_receiver_set_sub_decode_mode(app->protopirate_receiver, true);
    protopirate_view_receiver_reset_menu(app->protopirate_receiver);
    protopirate_view_receiver_set_idx_menu(app->protopirate_receiver, 0);
    protopirate_view_receiver_set_callback(
        app->protopirate_receiver, protopirate_scene_sub_decode_receiver_callback, app);
    protopirate_scene_sub_decode_update_receiver_progress(app, g_decode_ctx);
    protopirate_scene_sub_decode_update_receiver_statusbar(app, g_decode_ctx);
    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewReceiver);
}

static void protopirate_scene_sub_decode_reset_for_file(SubDecodeContext* ctx) {
    if(ctx->raw_reader) {
        raw_file_reader_free(ctx->raw_reader);
        ctx->raw_reader = NULL;
    }
    close_file_handles(ctx);
    protopirate_sub_decode_clear_custom_preset(ctx);
    protopirate_history_reset(ctx->history);
    furi_string_reset(ctx->protocol_name);
    furi_string_reset(ctx->result);
    furi_string_reset(ctx->error_info);

    ctx->failure_kind = SubDecodeFailureGeneric;
    ctx->signal_count = 0;
    ctx->selected_history_index = 0;
    ctx->showing_signal_info = false;
    ctx->signal_info_left_is_emulate = false;
    ctx->worker_startup_delay = 0;
    ctx->decode_elapsed_us = 0;
}

static bool protopirate_scene_sub_decode_open_browser_for_next_file(ProtoPirateApp* app) {
    SubDecodeContext* ctx = g_decode_ctx;
    if(!ctx) return false;

    const bool selected = protopirate_scene_sub_decode_select_file(ctx);
    if(!selected) {
        return false;
    }

    protopirate_scene_sub_decode_reset_for_file(ctx);
    ctx->state = DecodeStateOpenFile;
    protopirate_scene_sub_decode_prepare_receiver_view(app);
    return true;
}

void protopirate_scene_sub_decode_on_enter(void* context) {
    ProtoPirateApp* app = context;

    if(!protopirate_ensure_receiver_view(app) || !protopirate_ensure_widget(app)) {
        notification_message(app->notifications, &sequence_error);
        app->tool_scene_nav_pending = TOOL_SCENE_NAV_POP;
        return;
    }

    if(!app->radio_initialized && !protopirate_radio_init(app)) {
        FURI_LOG_E(TAG, "Failed to initialize radio for sub decode scene");
        notification_message(app->notifications, &sequence_error);
        app->tool_scene_nav_pending = TOOL_SCENE_NAV_POP;
        return;
    }

    if(app->txrx && app->txrx->history) {
        protopirate_history_release_scratch(app->txrx->history);
    }

    protopirate_rx_stack_resume_after_tx(app);
    if(!app->txrx->receiver) {
        FURI_LOG_E(TAG, "Failed to allocate receiver for sub decode scene");
        notification_message(app->notifications, &sequence_error);
        app->tool_scene_nav_pending = TOOL_SCENE_NAV_POP;
        return;
    }

    FURI_LOG_I(TAG, "Sub decode scene enter - Free heap: %zu", memmgr_get_free_heap());

    g_decode_ctx = malloc(sizeof(SubDecodeContext));
    if(!g_decode_ctx) {
        FURI_LOG_E(TAG, "Failed to allocate decode context");
        app->tool_scene_nav_pending = TOOL_SCENE_NAV_POP;
        return;
    }
    memset(g_decode_ctx, 0, sizeof(SubDecodeContext));

    if(app->txrx && app->txrx->preset && app->txrx->preset->name) {
        snprintf(
            g_decode_ctx->previous_preset_name,
            sizeof(g_decode_ctx->previous_preset_name),
            "%s",
            furi_string_get_cstr(app->txrx->preset->name));
        g_decode_ctx->previous_frequency = app->txrx->preset->frequency;
        g_decode_ctx->previous_preset_data = app->txrx->preset->data;
        g_decode_ctx->previous_preset_data_size = app->txrx->preset->data_size;
        g_decode_ctx->previous_preset_saved = true;
    }

    FURI_LOG_I(TAG, "After decode context alloc - Free heap: %zu", memmgr_get_free_heap());

    // Allocate history
    bool owns_history = false;
    if(!app->txrx->history) {
        app->txrx->history = protopirate_history_alloc();
        if(!app->txrx->history) {
            FURI_LOG_E(TAG, "Failed to allocate history!");
            free(g_decode_ctx);
            g_decode_ctx = NULL;
            notification_message(app->notifications, &sequence_error);
            app->tool_scene_nav_pending = TOOL_SCENE_NAV_POP;
            return;
        }
        owns_history = true;
    }

    g_decode_ctx->file_path = furi_string_alloc();
    g_decode_ctx->protocol_name = furi_string_alloc();
    g_decode_ctx->result = furi_string_alloc();
    g_decode_ctx->error_info = furi_string_alloc();
    g_decode_ctx->state = DecodeStateIdle;
    g_decode_ctx->history = app->txrx->history;
    g_decode_ctx->owns_history = owns_history;
    //protopirate_history_reset(g_decode_ctx->history);
    g_decode_ctx->signal_count = 0;
    g_decode_ctx->selected_history_index = 0;
    g_decode_ctx->raw_reader = NULL;
    g_decode_ctx->worker_startup_delay = 0;

    FURI_LOG_I(TAG, "After context setup - Free heap: %zu", memmgr_get_free_heap());

    furi_string_set(g_decode_ctx->file_path, SUBGHZ_APP_FOLDER);

    if(protopirate_scene_sub_decode_select_file(g_decode_ctx)) {
        protopirate_scene_sub_decode_reset_for_file(g_decode_ctx);
        g_decode_ctx->state = DecodeStateOpenFile;
        protopirate_scene_sub_decode_prepare_receiver_view(app);
    } else {
        app->tool_scene_nav_pending = TOOL_SCENE_NAV_POP;
    }
}

bool protopirate_scene_sub_decode_on_event(void* context, SceneManagerEvent event) {
    ProtoPirateApp* app = context;
    bool consumed = false;
    SubDecodeContext* ctx = g_decode_ctx;

    if(!ctx) return false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == ProtoPirateCustomEventSubDecodeUpdate) {
            // Update receiver view with new history items (when signals are detected during decoding)
            if(ctx->state == DecodeStateDecodingRaw) {
                protopirate_scene_sub_decode_update_receiver_statusbar(app, ctx);
                consumed = true;
            } else if(
                ctx->state == DecodeStateShowHistory ||
                (ctx->state == DecodeStateDone && !ctx->showing_signal_info)) {
                // Rebuild history view
                uint16_t history_count = protopirate_history_get_item(ctx->history);
                if(history_count > 0) {
                    protopirate_view_receiver_sync_menu_from_history(
                        app->protopirate_receiver, ctx->history);

                    protopirate_view_receiver_set_idx_menu(
                        app->protopirate_receiver, ctx->selected_history_index);

                    protopirate_scene_sub_decode_update_receiver_statusbar(app, ctx);
                }
            }
            consumed = true;
        } else if(event.event == ProtoPirateCustomEventSubDecodeSave) {
            // Save the file (same as receiver_info)
            FlipperFormat* ff =
                protopirate_history_get_raw_data(ctx->history, ctx->selected_history_index);

            if(ff) {
                // Extract protocol name
                FuriString* protocol = furi_string_alloc();
                protopirate_storage_get_capture_display_protocol(ff, protocol);

                FuriString* saved_path = furi_string_alloc();
                if(protopirate_storage_save_capture(
                       ff, furi_string_get_cstr(protocol), saved_path)) {
                    notification_message(app->notifications, &sequence_success);
                } else {
                    notification_message(app->notifications, &sequence_error);
                }

                furi_string_free(protocol);
                furi_string_free(saved_path);
            } else {
                FURI_LOG_E(
                    TAG,
                    "No flipper format data available, item: %d",
                    ctx->selected_history_index);
                notification_message(app->notifications, &sequence_error);
            }
            consumed = true;
        }
#ifdef ENABLE_EMULATE_FEATURE
        else if(
            event.event == ProtoPirateCustomEventSubDecodeEmulate &&
            app->emulate_feature_enabled && !app->emulate_disabled_for_loaded) {
            FlipperFormat* ff =
                protopirate_history_get_raw_data(ctx->history, ctx->selected_history_index);
            if(ff && protopirate_storage_save_capture_to_path(ff, PROTOPIRATE_TEMP_FILE)) {
                protopirate_history_release_scratch(ctx->history);
                if(app->loaded_file_path) furi_string_free(app->loaded_file_path);
                app->loaded_file_path = furi_string_alloc_set(PROTOPIRATE_TEMP_FILE);
                FURI_LOG_I(
                    TAG,
                    "Emulate from sub-decode temp file: %s",
                    furi_string_get_cstr(app->loaded_file_path));
                app->tool_scene_nav_pending = TOOL_SCENE_NAV_NEXT;
                app->tool_scene_nav_target = ProtoPirateSceneEmulate;
            } else {
                FURI_LOG_E(
                    TAG, "Failed to prepare emulate capture %u", ctx->selected_history_index);
                notification_message(app->notifications, &sequence_error);
            }
            consumed = true;
        }
#endif
        else if(event.event == ProtoPirateCustomEventSubDecodeBruteforceStart) {
            app->txrx->idx_menu_chosen = ctx->selected_history_index;
            if(protopirate_psa_bf_plugin_ensure_loaded(app) && app->psa_bf_plugin &&
               app->psa_bf_plugin->on_scene_event(app, ProtoPiratePsaBfContextSubDecode, event)) {
                if(app->psa_bf_plugin->is_running(app)) {
                    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewWidget);
                }
            }
            consumed = true;
            return consumed;

        } else if(event.event == ProtoPirateCustomEventPsaBruteforceComplete) {
            app->txrx->idx_menu_chosen = ctx->selected_history_index;
            if(app->psa_bf_plugin) {
                app->psa_bf_plugin->on_scene_event(app, ProtoPiratePsaBfContextSubDecode, event);
            }
            ctx->state = DecodeStateShowSignalInfo;
            view_dispatcher_send_custom_event(
                app->view_dispatcher, ProtoPirateCustomEventSubDecodeUpdate);
            consumed = true;
            return consumed;

        } else if(event.event == ProtoPirateCustomEventViewReceiverOK) {
            // User selected a signal from history - show signal info
            uint16_t idx = protopirate_view_receiver_get_idx_menu(app->protopirate_receiver);
            uint16_t history_count = protopirate_history_get_item(ctx->history);
            if(idx < history_count) {
                ctx->selected_history_index = idx;
                ctx->state = DecodeStateShowSignalInfo;
                ctx->showing_signal_info = true;
                // Trigger state handler
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, ProtoPirateCustomEventSubDecodeUpdate);
            }
            consumed = true;
        } else if(event.event == ProtoPirateCustomEventViewReceiverBack) {
            if(protopirate_scene_sub_decode_cancel_active_decode(app, ctx)) {
                consumed = true;
                return consumed;
            }

            if(!protopirate_scene_sub_decode_open_browser_for_next_file(app)) {
                protopirate_history_reset(ctx->history);
                app->tool_scene_nav_pending = TOOL_SCENE_NAV_SEARCH_PREVIOUS;
                app->tool_scene_nav_target = ProtoPirateSceneStart;
            }
            consumed = true;
        }
    }

    if(event.type == SceneManagerEventTypeTick) {
        consumed = true;

        app->txrx->idx_menu_chosen = ctx->selected_history_index;
        if(app->psa_bf_plugin && app->psa_bf_plugin->is_running(app) &&
           app->psa_bf_plugin->on_scene_event(app, ProtoPiratePsaBfContextSubDecode, event)) {
            return consumed;
        }

        FURI_LOG_D(TAG, "Tick: state=%d", ctx->state);

        switch(ctx->state) {
        case DecodeStateOpenFile: {
            FURI_LOG_I(TAG, "OpenFile: Starting - Free heap: %zu", memmgr_get_free_heap());
            ctx->storage = furi_record_open(RECORD_STORAGE);
            FURI_LOG_D(TAG, "OpenFile: Storage opened");
            ctx->ff = flipper_format_file_alloc(ctx->storage);
            FURI_LOG_D(TAG, "OpenFile: FlipperFormat allocated");

            if(!flipper_format_file_open_existing(ctx->ff, furi_string_get_cstr(ctx->file_path))) {
                FURI_LOG_E(TAG, "OpenFile: Failed to open file");
                furi_string_set(ctx->result, "Failed to open file");
                furi_string_set(ctx->error_info, "File open failed");
                close_file_handles(ctx);
                ctx->state = DecodeStateShowFailure;
                notification_message(app->notifications, &sequence_error);
            } else {
                FURI_LOG_I(TAG, "OpenFile: File opened successfully");
                ctx->state = DecodeStateReadHeader;
            }
            break;
        }

        case DecodeStateReadHeader: {
            FURI_LOG_I(TAG, "ReadHeader: Starting - Free heap: %zu", memmgr_get_free_heap());

            FuriString* temp_str = furi_string_alloc();
            uint32_t version = 0;
            bool success = false;

            do {
                FURI_LOG_D(TAG, "ReadHeader: Reading header");
                if(!flipper_format_read_header(ctx->ff, temp_str, &version)) {
                    furi_string_set(ctx->result, "Invalid file format");
                    furi_string_set(ctx->error_info, "Invalid header");
                    break;
                }

                FURI_LOG_D(TAG, "ReadHeader: Header type: %s", furi_string_get_cstr(temp_str));
                if(furi_string_cmp_str(temp_str, "Flipper SubGhz RAW File") != 0) {
                    furi_string_set(ctx->result, "Not a RAW SubGhz file");
                    furi_string_set(ctx->error_info, "Not RAW SubGhz file");
                    ctx->failure_kind = SubDecodeFailureNotRaw;
                    break;
                }

                FURI_LOG_D(TAG, "ReadHeader: Reading protocol");
                if(!flipper_format_read_string(ctx->ff, FF_PROTOCOL, ctx->protocol_name)) {
                    furi_string_set(ctx->result, "Missing Protocol");
                    furi_string_set(ctx->error_info, "No protocol field");
                    break;
                }

                FURI_LOG_D(TAG, "ReadHeader: Rewinding for frequency");
                flipper_format_rewind(ctx->ff);
                flipper_format_read_header(ctx->ff, temp_str, &version);
                ctx->frequency = 433920000;
                flipper_format_read_uint32(ctx->ff, FF_FREQUENCY, &ctx->frequency, 1);

                FURI_LOG_I(
                    TAG,
                    "Protocol: %s, Freq: %lu",
                    furi_string_get_cstr(ctx->protocol_name),
                    ctx->frequency);
                protopirate_scene_sub_decode_update_receiver_statusbar(app, ctx);

                success = true;
            } while(false);

            furi_string_free(temp_str);
            FURI_LOG_D(TAG, "ReadHeader: Freed temp_str");

            if(!success) {
                FURI_LOG_E(TAG, "ReadHeader: Failed, closing handles");
                close_file_handles(ctx);
                ctx->state = DecodeStateShowFailure;
                notification_message(app->notifications, &sequence_error);
            } else if(furi_string_cmp_str(ctx->protocol_name, "RAW") == 0) {
                FURI_LOG_I(TAG, "ReadHeader: RAW file detected, closing handles");
                close_file_handles(ctx);
                FURI_LOG_D(TAG, "ReadHeader: Handles closed");

                ctx->state = DecodeStateStartingWorker;
                FURI_LOG_I(
                    TAG,
                    "ReadHeader: State set to StartingWorker - Free heap: %zu",
                    memmgr_get_free_heap());
            } else {
                FURI_LOG_W(TAG, "ReadHeader: Non-RAW protocol not supported");
                close_file_handles(ctx);
                furi_string_set(ctx->error_info, "Only RAW supported");
                ctx->failure_kind = SubDecodeFailureNotRaw;
                ctx->state = DecodeStateShowFailure;
            }

            FURI_LOG_I(TAG, "ReadHeader: Complete, next state: %d", ctx->state);
            break;
        }

        case DecodeStateStartingWorker: {
            FURI_LOG_I(
                TAG,
                "StartingWorker: Entry - delay=%u, Free heap: %zu",
                ctx->worker_startup_delay,
                memmgr_get_free_heap());

            if(ctx->worker_startup_delay < 3) {
                ctx->worker_startup_delay++;
                FURI_LOG_D(TAG, "StartingWorker: Delay tick %u", ctx->worker_startup_delay);
                break;
            }
            ctx->worker_startup_delay = 0;

            FURI_LOG_I(TAG, "StartingWorker: Reading file metadata");

            Storage* storage = furi_record_open(RECORD_STORAGE);
            if(!storage) {
                FURI_LOG_E(TAG, "Failed to open storage");
                break;
            }
            FlipperFormat* fff_data_file = flipper_format_file_alloc(storage);
            if(!fff_data_file) {
                FURI_LOG_E(TAG, "Failed to allocate FlipperFormat");
                furi_record_close(RECORD_STORAGE);
                break;
            }

            FuriString* temp_str = furi_string_alloc();
            bool setup_ok = false;

            do {
                if(!flipper_format_file_open_existing(
                       fff_data_file, furi_string_get_cstr(ctx->file_path))) {
                    FURI_LOG_E(TAG, "Error opening file for metadata");
                    break;
                }

                uint32_t version = 0;
                if(!flipper_format_read_header(fff_data_file, temp_str, &version)) {
                    FURI_LOG_E(TAG, "Missing or incorrect header");
                    break;
                }

                if(strcmp(furi_string_get_cstr(temp_str), "Flipper SubGhz RAW File") != 0 ||
                   version != 1) {
                    FURI_LOG_E(TAG, "Not a valid RAW file");
                    break;
                }

                if(!flipper_format_read_uint32(fff_data_file, FF_FREQUENCY, &ctx->frequency, 1)) {
                    FURI_LOG_E(TAG, "Missing Frequency");
                    break;
                }

                if(!flipper_format_read_string(fff_data_file, FF_PRESET, temp_str)) {
                    FURI_LOG_E(TAG, "Missing Preset");
                    break;
                }

                const char* preset_name_long = furi_string_get_cstr(temp_str);
                const char* preset_name_short = pp_get_short_preset_name(preset_name_long);
                uint8_t* preset_data = NULL;
                size_t preset_data_size = 0U;

                if(pp_preset_name_is_custom_marker(preset_name_long)) {
                    if(protopirate_sub_decode_try_load_custom_preset(ctx, fff_data_file)) {
                        preset_data = ctx->custom_preset_data;
                        preset_data_size = ctx->custom_preset_data_size;
                        const char* matched_preset_name =
                            protopirate_sub_decode_find_matching_preset_name(
                                app, preset_data, preset_data_size);
                        if(matched_preset_name) {
                            preset_name_short = matched_preset_name;
                            FURI_LOG_I(
                                TAG,
                                "Loaded RAW custom preset data (%u bytes), matched preset %s",
                                (unsigned)preset_data_size,
                                preset_name_short);
                        } else {
                            preset_name_short = "Custom";
                            FURI_LOG_I(
                                TAG,
                                "Loaded RAW custom preset data (%u bytes), no preset-name match",
                                (unsigned)preset_data_size);
                        }
                    } else {
                        FURI_LOG_W(TAG, "RAW custom preset data missing, falling back to AM650");
                        preset_name_short = "AM650";
                    }
                }

                if(!preset_data) {
                    size_t preset_index = subghz_setting_get_preset_count(app->setting);
                    for(size_t i = 0; i < subghz_setting_get_preset_count(app->setting); i++) {
                        if(!strcmp(
                               subghz_setting_get_preset_name(app->setting, i),
                               preset_name_short)) {
                            preset_index = i;
                            break;
                        }
                    }
                    if(preset_index >= subghz_setting_get_preset_count(app->setting)) {
                        preset_name_short = "AM650";
                        for(size_t i = 0; i < subghz_setting_get_preset_count(app->setting); i++) {
                            if(!strcmp(
                                   subghz_setting_get_preset_name(app->setting, i),
                                   preset_name_short)) {
                                preset_index = i;
                                break;
                            }
                        }
                        if(preset_index >= subghz_setting_get_preset_count(app->setting)) {
                            FURI_LOG_E(TAG, "Failed to get preset index!");
                            break;
                        }
                    }

                    preset_data = subghz_setting_get_preset_data(app->setting, preset_index);
                    preset_data_size =
                        subghz_setting_get_preset_data_size(app->setting, preset_index);
                }

                if(!preset_data || preset_data_size == 0U) {
                    FURI_LOG_E(TAG, "Failed to get preset data!");
                    break;
                }

                FURI_LOG_I(TAG, "Sub-decode using preset %s", preset_name_short);

                protopirate_preset_init(
                    app, preset_name_short, ctx->frequency, preset_data, preset_data_size);

                if(!protopirate_apply_protocol_registry_for_context(
                       app, preset_name_short, ctx->frequency, preset_data, preset_data_size, NULL) ||
                   !app->txrx->receiver) {
                    FURI_LOG_E(TAG, "Failed to rebuild receiver for preset %s", preset_name_short);
                    break;
                }

                subghz_receiver_reset(app->txrx->receiver);
                subghz_receiver_set_rx_callback(
                    app->txrx->receiver, protopirate_sub_decode_receiver_callback, app);

                setup_ok = true;
            } while(false);

            if(fff_data_file) flipper_format_free(fff_data_file);

            if(storage) furi_record_close(RECORD_STORAGE);

            furi_string_free(temp_str);

            if(!setup_ok) {
                furi_string_set(ctx->result, "Failed to read file metadata");
                furi_string_set(ctx->error_info, "Metadata read failed");
                ctx->state = DecodeStateShowFailure;
                notification_message(app->notifications, &sequence_error);
                break;
            }

            FURI_LOG_I(
                TAG,
                "StartingWorker: Allocating raw reader - Free heap: %zu",
                memmgr_get_free_heap());

            ctx->raw_reader = raw_file_reader_alloc();
            if(!ctx->raw_reader) {
                FURI_LOG_E(TAG, "Failed to allocate raw reader");
                furi_string_set(ctx->result, "Memory allocation failed");
                furi_string_set(ctx->error_info, "Out of memory");
                ctx->state = DecodeStateShowFailure;
                notification_message(app->notifications, &sequence_error);
                break;
            }

            {
                Storage* size_storage = furi_record_open(RECORD_STORAGE);
                FileInfo file_info = {0};
                bool stat_ok = size_storage && (storage_common_stat(
                                                    size_storage,
                                                    furi_string_get_cstr(ctx->file_path),
                                                    &file_info) == FSE_OK);
                uint64_t file_size = stat_ok ? file_info.size : 0;
                if(size_storage) {
                    furi_record_close(RECORD_STORAGE);
                }

                if(stat_ok && file_size > SUB_DECODE_MAX_FILE_SIZE) {
                    FURI_LOG_W(TAG, "RAW file too large: %llu bytes", file_size);
                    raw_file_reader_free(ctx->raw_reader);
                    ctx->raw_reader = NULL;
                    furi_string_printf(
                        ctx->result,
                        "File too large to decode\n\n"
                        "%lu KB\n"
                        "(limit %lu KB)",
                        (uint32_t)(file_size / 1024U),
                        (uint32_t)(SUB_DECODE_MAX_FILE_SIZE / 1024U));
                    furi_string_set(ctx->error_info, "File too large");
                    ctx->state = DecodeStateShowFailure;
                    notification_message(app->notifications, &sequence_error);
                    break;
                }
            }

            FURI_LOG_I(
                TAG,
                "StartingWorker: Opening RAW stream - Free heap: %zu",
                memmgr_get_free_heap());

            if(!raw_file_reader_open(ctx->raw_reader, furi_string_get_cstr(ctx->file_path))) {
                FURI_LOG_E(TAG, "Failed to open raw file");
                raw_file_reader_free(ctx->raw_reader);
                ctx->raw_reader = NULL;
                furi_string_set(ctx->result, "Failed to open RAW file");
                furi_string_set(ctx->error_info, "File open failed");
                ctx->state = DecodeStateShowFailure;
                notification_message(app->notifications, &sequence_error);
                break;
            }

            ctx->state = DecodeStateDecodingRaw;
            ctx->decode_elapsed_us = 0;
            protopirate_scene_sub_decode_update_receiver_progress(app, ctx);
            protopirate_scene_sub_decode_update_receiver_statusbar(app, ctx);
            FURI_LOG_I(
                TAG,
                "StartingWorker: Ready to decode RAW stream - Free heap: %zu",
                memmgr_get_free_heap());
            break;
        }

        case DecodeStateDecodingRaw: {
            if(!ctx->raw_reader) {
                FURI_LOG_E(TAG, "DecodingRaw: No raw reader");
                ctx->state = DecodeStateShowFailure;
                break;
            }

            bool level = false;
            uint32_t duration = 0;
            uint32_t samples_processed = 0;

            while(samples_processed < SAMPLES_TO_READ_PER_TICK) {
                if(!raw_file_reader_get_next(ctx->raw_reader, &level, &duration)) {
                    const bool at_eof = raw_file_reader_is_finished(ctx->raw_reader);
                    FURI_LOG_I(
                        TAG,
                        "DecodingRaw: Read stopped, eof=%d, signals=%u",
                        at_eof,
                        ctx->signal_count);

                    raw_file_reader_free(ctx->raw_reader);
                    ctx->raw_reader = NULL;

                    subghz_receiver_set_rx_callback(app->txrx->receiver, NULL, NULL);

                    if(!at_eof) {
                        furi_string_set(ctx->result, "Failed while reading RAW file");
                        furi_string_set(ctx->error_info, "Read interrupted");
                        ctx->state = DecodeStateShowFailure;
                        notification_message(app->notifications, &sequence_error);
                        break;
                    }

                    uint16_t history_count = protopirate_history_get_item(ctx->history);
                    protopirate_view_receiver_set_sub_decode_progress(
                        app->protopirate_receiver, 100);

                    if(history_count > 0) {
                        ctx->state = DecodeStateShowHistory;
                        ctx->selected_history_index = 0;
                        ctx->showing_signal_info = false;
                        notification_message(app->notifications, &sequence_success);
                    } else {
                        furi_string_printf(
                            ctx->result,
                            "RAW Signal\n\n"
                            "Freq: %lu.%02lu MHz\n\n"
                            "No ProtoPirate protocol\n"
                            "detected in signal.",
                            ctx->frequency / 1000000,
                            (ctx->frequency % 1000000) / 10000);
                        furi_string_set(ctx->error_info, "No protocol match");
                        ctx->failure_kind = SubDecodeFailureNoMatch;
                        ctx->state = DecodeStateShowFailure;
                        notification_message(app->notifications, &sequence_error);
                    }
                    break;
                }
                ctx->decode_elapsed_us += duration;
                subghz_receiver_decode(app->txrx->receiver, level, duration);
                samples_processed++;
            }

            protopirate_scene_sub_decode_update_receiver_progress(app, ctx);
            protopirate_scene_sub_decode_update_receiver_statusbar(app, ctx);
            furi_thread_yield();
            break;
        }

        case DecodeStateShowFailure: {
            protopirate_scene_sub_decode_draw_failure(app, ctx);
            view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewWidget);
            ctx->state = DecodeStateDone;
            break;
        }

        case DecodeStateShowHistory: {
            // Show history list using receiver view (same as receive mode)
            uint16_t history_count = protopirate_history_get_item(ctx->history);
            if(history_count > 0) {
                protopirate_view_receiver_sync_menu_from_history(
                    app->protopirate_receiver, ctx->history);

                // Set initial selection
                protopirate_view_receiver_set_idx_menu(
                    app->protopirate_receiver, ctx->selected_history_index);

                // Set up callback
                protopirate_view_receiver_set_callback(
                    app->protopirate_receiver,
                    protopirate_scene_sub_decode_receiver_callback,
                    app);

                protopirate_view_receiver_set_sub_decode_progress(app->protopirate_receiver, 100);
                protopirate_scene_sub_decode_update_receiver_statusbar(app, ctx);

                // Switch to receiver view
                view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewReceiver);
                ctx->state = DecodeStateDone;
                ctx->showing_signal_info = false;
            }
            break;
        }

        case DecodeStateShowSignalInfo: {
            // Show signal info in widget (same layout as receiver_info)
            widget_reset(app->widget);

            uint16_t history_count = protopirate_history_get_item(ctx->history);
            if(ctx->selected_history_index < history_count) {
                FuriString* text = furi_string_alloc();

                // Get menu text (first line) for header
                /*protopirate_history_get_text_item_menu(
                    ctx->history, text, ctx->selected_history_index);
                widget_add_string_element(
                    app->widget,
                    64,
                    0,
                    AlignCenter,
                    AlignTop,
                    FontPrimary,
                    furi_string_get_cstr(text));*/

                // Get full text for body
                furi_string_reset(text);
                protopirate_history_get_text_item_detail(
                    ctx->history, ctx->selected_history_index, text, app->txrx->environment);
                widget_add_text_scroll_element(
                    app->widget, 0, 0, 128, 50, furi_string_get_cstr(text));

                // Add save button
                widget_add_button_element(
                    app->widget,
                    GuiButtonTypeRight,
                    "Save",
                    protopirate_scene_sub_decode_widget_callback,
                    app);

                ctx->signal_info_left_is_emulate = false;
#ifdef ENABLE_EMULATE_FEATURE
                bool left_button_used = false;
#endif
                app->emulate_disabled_for_loaded = true;

                // Store reference to history item's flipper format for saving
                FlipperFormat* ff =
                    protopirate_history_get_raw_data(ctx->history, ctx->selected_history_index);
                if(ff) {
                    FuriString* proto_str = furi_string_alloc();
                    flipper_format_rewind(ff);
                    bool have_proto = flipper_format_read_string(ff, FF_PROTOCOL, proto_str);
                    bool is_psa = have_proto && furi_string_cmp_str(proto_str, "PSA") == 0;

                    if(have_proto) {
                        const char* protocol_name = furi_string_get_cstr(proto_str);
                        app->emulate_disabled_for_loaded =
                            !protopirate_protocol_catalog_can_tx(protocol_name);
                    } else {
                        app->emulate_disabled_for_loaded = true;
                    }
                    furi_string_free(proto_str);
                    if(is_psa) {
                        app->txrx->idx_menu_chosen = ctx->selected_history_index;
                        bool needs_bf = false;
                        if(protopirate_psa_bf_plugin_ensure_loaded(app) && app->psa_bf_plugin) {
                            needs_bf = app->psa_bf_plugin->needs_bruteforce(
                                app, ProtoPiratePsaBfContextSubDecode);
                        }
                        if(needs_bf) {
                            widget_add_button_element(
                                app->widget,
                                GuiButtonTypeLeft,
                                "Brute force",
                                protopirate_scene_sub_decode_widget_callback,
                                app);
#ifdef ENABLE_EMULATE_FEATURE
                            left_button_used = true;
#endif
                        }
                    }
                }

#ifdef ENABLE_EMULATE_FEATURE
                if(!left_button_used && app->emulate_feature_enabled &&
                   !app->emulate_disabled_for_loaded) {
                    widget_add_button_element(
                        app->widget,
                        GuiButtonTypeLeft,
                        "Emulate",
                        protopirate_scene_sub_decode_widget_callback,
                        app);
                    ctx->signal_info_left_is_emulate = true;
                    left_button_used = true;
                }
#endif

                furi_string_free(text);
            }

            view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewWidget);
            ctx->state = DecodeStateDone;
            ctx->showing_signal_info = true;
            break;
        }

        default:
            break;
        }

    } else if(event.type == SceneManagerEventTypeBack) {
        app->txrx->idx_menu_chosen = ctx->selected_history_index;
        if(protopirate_scene_sub_decode_cancel_active_decode(app, ctx)) {
            consumed = true;
            return consumed;
        }
        if(app->psa_bf_plugin &&
           app->psa_bf_plugin->on_scene_event(app, ProtoPiratePsaBfContextSubDecode, event)) {
            consumed = true;
            return consumed;
        }
        // Handle back button navigation
        if(ctx->showing_signal_info) {
            // In signal info - go back to history
            ctx->showing_signal_info = false;
            //ctx->selected_history_index = 0;
            ctx->state = DecodeStateShowHistory;
            view_dispatcher_send_custom_event(
                app->view_dispatcher, ProtoPirateCustomEventSubDecodeUpdate);
            consumed = true;
        }
        // If in history view, back is handled by ViewReceiverBack event
    }

    return consumed;
}

void protopirate_scene_sub_decode_on_exit(void* context) {
    ProtoPirateApp* app = context;

    if(app && app->txrx && app->txrx->receiver) {
        subghz_receiver_reset(app->txrx->receiver);
        subghz_receiver_set_rx_callback(app->txrx->receiver, NULL, NULL);
    }

    protopirate_psa_bf_context_release(app);

    bool owns_history = false;
    if(g_decode_ctx) {
        owns_history = g_decode_ctx->owns_history;

        if(g_decode_ctx->raw_reader) {
            raw_file_reader_free(g_decode_ctx->raw_reader);
            g_decode_ctx->raw_reader = NULL;
        }

        close_file_handles(g_decode_ctx);

        if(g_decode_ctx->previous_preset_saved && app && app->txrx && app->txrx->preset) {
            protopirate_preset_init(
                app,
                g_decode_ctx->previous_preset_name,
                g_decode_ctx->previous_frequency,
                g_decode_ctx->previous_preset_data,
                g_decode_ctx->previous_preset_data_size);
        }

        protopirate_sub_decode_clear_custom_preset(g_decode_ctx);

        furi_string_free(g_decode_ctx->file_path);
        furi_string_free(g_decode_ctx->protocol_name);
        furi_string_free(g_decode_ctx->result);
        furi_string_free(g_decode_ctx->error_info);
        free(g_decode_ctx);
        g_decode_ctx = NULL;
    }

    if(owns_history && app && app->txrx && app->txrx->history) {
        protopirate_history_reset(app->txrx->history);

        FURI_LOG_D(TAG, "Freeing history %p", app->txrx->history);
        protopirate_history_free(app->txrx->history);
        app->txrx->history = NULL;
    }

    if(app && app->widget) {
        widget_reset(app->widget);
    }

    if(app && app->protopirate_receiver) {
        protopirate_view_receiver_reset_menu(app->protopirate_receiver);
    }
}

static void sub_decode_plugin_set_host_api(const ProtoPirateToolSceneHostApi* host_api) {
    g_tool_scene_host_api = host_api;
}

static const ProtoPirateToolScenePlugin protopirate_sub_decode_plugin = {
    .plugin_name = "ProtoPirate Sub Decode",
    .kind = ProtoPirateToolScenePluginKindSubDecode,
    .set_host_api = sub_decode_plugin_set_host_api,
    .on_enter = protopirate_scene_sub_decode_on_enter,
    .on_event = protopirate_scene_sub_decode_on_event,
    .on_exit = protopirate_scene_sub_decode_on_exit,
    .release = NULL,
};

static const FlipperAppPluginDescriptor protopirate_sub_decode_plugin_descriptor = {
    .appid = PROTOPIRATE_TOOL_SCENE_PLUGIN_APP_ID,
    .ep_api_version = PROTOPIRATE_TOOL_SCENE_PLUGIN_API_VERSION,
    .entry_point = &protopirate_sub_decode_plugin,
};

const FlipperAppPluginDescriptor* protopirate_sub_decode_plugin_ep(void) {
    return &protopirate_sub_decode_plugin_descriptor;
}

#endif // PROTOPIRATE_SUB_DECODE_PLUGIN_BUILD
#endif
