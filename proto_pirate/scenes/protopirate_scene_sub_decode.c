// scenes/protopirate_scene_sub_decode.c
#include "../protopirate_app_i.h"
#ifdef ENABLE_SUB_DECODE_SCENE
#include "../helpers/protopirate_storage.h"
#include "../helpers/radio_device_loader.h"
#include "../helpers/raw_file_reader.h"
#include "../protopirate_history.h"
#include "../helpers/protopirate_psa_bf_host.h"
#include "core/core_defines.h"
#include "core/record.h"
#include "storage/storage.h"
#include <dialogs/dialogs.h>
#include <math.h>
#include <lib/subghz/types.h>

#include "proto_pirate_icons.h"

#define TAG "ProtoPirateSubDecode"

#define SUBGHZ_APP_FOLDER        EXT_PATH("subghz")
#define SAMPLES_TO_READ_PER_TICK 128
#define SUCCESS_DISPLAY_TICKS    18
#define FAILURE_DISPLAY_TICKS    18

// Decode state machine
typedef enum {
    DecodeStateIdle,
    DecodeStateOpenFile,
    DecodeStateReadHeader,
    DecodeStateStartingWorker,
    DecodeStateDecodingRaw,
    DecodeStateShowHistory,
    DecodeStateShowSignalInfo,
    DecodeStateShowSuccess,
    DecodeStateShowFailure,
    DecodeStateDone,
} DecodeState;

// Context for the whole decode operation
typedef struct {
    DecodeState state;
    uint16_t animation_frame;
    uint8_t result_display_counter;

    FuriString* file_path;
    FuriString* protocol_name;
    FuriString* result;
    FuriString* error_info;
    uint32_t frequency;

    Storage* storage;
    FlipperFormat* ff;

    bool decode_success;
    SubGhzProtocolDecoderBase* decoded_decoder;
    FuriString* decoded_string;

    FlipperFormat* save_data;
    bool can_save;

    uint8_t worker_startup_delay;

    ProtoPirateHistory* history;
    uint16_t match_count;
    uint16_t selected_history_index;
    bool showing_signal_info;

    RawFileReader* raw_reader;
} SubDecodeContext;

static SubDecodeContext* g_decode_ctx = NULL;

static void protopirate_scene_sub_decode_update_receiver_statusbar(
    ProtoPirateApp* app,
    ProtoPirateHistory* history) {
    char frequency_str[16] = {0};
    char modulation_str[8] = {0};
    char history_stat_str[16] = {0};

    protopirate_get_frequency_modulation_str(
        app, frequency_str, sizeof(frequency_str), modulation_str, sizeof(modulation_str));
    protopirate_history_format_status_text(history, history_stat_str, sizeof(history_stat_str));

    bool is_external =
        app->txrx->radio_device ? radio_device_loader_is_external(app->txrx->radio_device) : false;
    protopirate_view_receiver_add_data_statusbar(
        app->protopirate_receiver, frequency_str, modulation_str, history_stat_str, is_external);
}

void protopirate_subdecode_psa_bf_complete_refresh(void* app) {
    ProtoPirateApp* a = (ProtoPirateApp*)app;
    SubDecodeContext* ctx = g_decode_ctx;
    if(!a || !ctx) return;
    ctx->state = DecodeStateShowSignalInfo;
    view_dispatcher_send_custom_event(
        a->view_dispatcher, ProtoPirateCustomEventSubDecodeUpdate);
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
    furi_check(context);
    ProtoPirateApp* app = context;
    SubDecodeContext* ctx = g_decode_ctx;

    if(!ctx || ctx->state != DecodeStateDecodingRaw) {
        return;
    }

    FURI_LOG_I(TAG, "=== SIGNAL DECODED FROM FILE ===");

    if(protopirate_history_add_to_history(ctx->history, decoder_base, app->txrx->preset)) {
        ctx->match_count++;
        FURI_LOG_I(TAG, "Added signal %u to history", ctx->match_count);

        view_dispatcher_send_custom_event(
            app->view_dispatcher, ProtoPirateCustomEventSubDecodeUpdate);
    }

    subghz_receiver_reset(receiver);
}

// Draw the decoding animation
static void protopirate_decode_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    SubDecodeContext* ctx = g_decode_ctx;
    if(!ctx) return;

    canvas_clear(canvas);

    if(ctx->state == DecodeStateIdle || ctx->state == DecodeStateDone) {
        return;
    }

    canvas_set_color(canvas, ColorBlack);
    uint16_t frame = ctx->animation_frame;

    // Check for success/failure display states
    if(ctx->state == DecodeStateShowSuccess) {
        // Success screen
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignTop, "DECODED!");

        // Checkmark animation
        int check_progress = ctx->result_display_counter * 3;
        int cx = 64, cy = 32;
        int size = 12;

        // First stroke of check (going down-right from left)
        int stroke1_max = size;
        int stroke1_len = (check_progress > stroke1_max) ? stroke1_max : check_progress;
        for(int i = 0; i <= stroke1_len; i++) {
            canvas_draw_dot(canvas, cx - size + i, cy - size / 2 + i);
            canvas_draw_dot(canvas, cx - size + i, cy - size / 2 + i + 1);
            canvas_draw_dot(canvas, cx - size + i + 1, cy - size / 2 + i);
        }

        // Second stroke of check (going up-right)
        if(check_progress > stroke1_max) {
            int stroke2_max = size * 2;
            int stroke2_len = check_progress - stroke1_max;
            if(stroke2_len > stroke2_max) stroke2_len = stroke2_max;
            for(int i = 0; i <= stroke2_len; i++) {
                canvas_draw_dot(canvas, cx + i, cy + size / 2 - i);
                canvas_draw_dot(canvas, cx + i, cy + size / 2 - i - 1);
                canvas_draw_dot(canvas, cx + i + 1, cy + size / 2 - i);
            }
        }

        // Radiating dots
        for(int r = 0; r < 3; r++) {
            int radius = ((frame * 2 + r * 12) % 35) + 8;
            if(radius < 30) {
                for(int angle = 0; angle < 12; angle++) {
                    float a = (float)angle * 3.14159f * 2.0f / 12.0f;
                    int x = cx + (int)(radius * cosf(a));
                    int y = cy + (int)(radius * sinf(a));
                    if(x >= 0 && x < 128 && y >= 0 && y < 64) {
                        canvas_draw_dot(canvas, x, y);
                    }
                }
            }
        }

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignTop, "Signal matched!");
        return;
    }

    if(ctx->state == DecodeStateShowFailure) {
        // Failure screen
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignTop, "NO MATCH");

        // X animation
        int x_progress = ctx->result_display_counter * 3;
        int cx = 64, cy = 32;
        int size = 10;
        int stroke_len = size * 2 + 1; // Full diagonal length

        // First stroke: top-left to bottom-right
        int stroke1_len = (x_progress > stroke_len) ? stroke_len : x_progress;
        for(int i = 0; i < stroke1_len; i++) {
            int x = cx - size + i;
            int y = cy - size + i;
            canvas_draw_dot(canvas, x, y);
            canvas_draw_dot(canvas, x + 1, y);
            canvas_draw_dot(canvas, x, y + 1);
        }

        // Second stroke: top-right to bottom-left
        if(x_progress > stroke_len) {
            int stroke2_progress = x_progress - stroke_len;
            int stroke2_len = (stroke2_progress > stroke_len) ? stroke_len : stroke2_progress;
            for(int i = 0; i < stroke2_len; i++) {
                int x = cx + size - i;
                int y = cy - size + i;
                canvas_draw_dot(canvas, x, y);
                canvas_draw_dot(canvas, x - 1, y);
                canvas_draw_dot(canvas, x, y + 1);
            }
        }

        // Static noise effect around the edges
        for(int i = 0; i < 30; i++) {
            int x = ((frame * 7 + i * 17) * 31) % 128;
            int y = ((frame * 13 + i * 23) * 17) % 64;
            canvas_draw_dot(canvas, x, y);
        }

        canvas_set_font(canvas, FontSecondary);
        // Show error info if we have it
        if(furi_string_size(ctx->error_info) > 0) {
            canvas_draw_str_aligned(
                canvas, 64, 54, AlignCenter, AlignTop, furi_string_get_cstr(ctx->error_info));
        } else {
            canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignTop, "Unknown protocol");
        }
        return;
    }

    // Normal decoding animation

    // Title with occasional glitch
    canvas_set_font(canvas, FontPrimary);
    int glitch = (frame % 47 == 0) ? 1 : 0;
    canvas_draw_str_aligned(canvas, 64 + glitch, 0, AlignCenter, AlignTop, "Decoding");

    // Waveform visualization - original style with sinf
    int wave_y = 22;
    int wave_height = 14;

    for(int x = 0; x < 128; x++) {
        float phase = (float)(x + frame * 4) * 0.12f;
        float phase2 = (float)(x - frame * 2) * 0.08f;
        int y_offset = (int)(sinf(phase) * wave_height / 2 + sinf(phase2) * wave_height / 4);

        // Add some noise variation
        if((x * 7 + frame) % 13 == 0) {
            y_offset += ((frame * x) % 5) - 2;
        }

        canvas_draw_dot(canvas, x, wave_y + y_offset);

        // Thicker line
        if((x + frame) % 3 != 0) {
            canvas_draw_dot(canvas, x, wave_y + y_offset + 1);
        }
    }

    // Scanning beam effect
    int scan_x = (frame * 5) % 148 - 10;
    for(int dx = 0; dx < 8; dx++) {
        int sx = scan_x + dx;
        if(sx >= 0 && sx < 128) {
            int intensity = 8 - dx;
            for(int y = wave_y - wave_height / 2 - 1; y <= wave_y + wave_height / 2 + 1; y++) {
                if(dx < intensity / 2) {
                    canvas_draw_dot(canvas, sx, y);
                }
            }
        }
    }

    // Progress bar frame
    int progress_y = 38;
    canvas_draw_rframe(canvas, 8, progress_y, 112, 10, 2);

    // Calculate progress
    int progress = 0;
    if(ctx->state == DecodeStateStartingWorker) {
        progress = 20 + (frame % 10);
    } else if(ctx->state == DecodeStateDecodingRaw) {
        // Show animated progress while decoding - gradually increase
        progress = 30 + (frame % 50);
    } else if(ctx->state == DecodeStateOpenFile || ctx->state == DecodeStateReadHeader) {
        progress = 5 + (frame % 10);
    }
    if(progress > 100) progress = 100;

    // Animated progress fill with diagonal stripes
    int fill_width = (progress * 108) / 100;
    for(int x = 0; x < fill_width; x++) {
        for(int y = 0; y < 6; y++) {
            if(((x - (int)frame + y) & 3) < 2) {
                canvas_draw_dot(canvas, 10 + x, progress_y + 2 + y);
            }
        }
    }

    // Status text
    canvas_set_font(canvas, FontSecondary);
    const char* status_text = "Starting...";

    switch(ctx->state) {
    case DecodeStateOpenFile:
        status_text = "Opening file...";
        break;
    case DecodeStateReadHeader:
        status_text = "Reading header...";
        break;
    case DecodeStateStartingWorker:
        status_text = "Counting timings...";
        break;
    case DecodeStateDecodingRaw: {
        static char match_text[32];
        if(ctx->match_count > 0) {
            snprintf(
                match_text,
                sizeof(match_text),
                "%u  match%s",
                ctx->match_count,
                ctx->match_count > 1 ? "es" : "");
            status_text = match_text;
        } else {
            status_text = "Decoding signal...";
        }
        break;
    }
    default:
        break;
    }
    canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignTop, status_text);

    // Binary rain effect on sides
    canvas_set_font(canvas, FontKeyboard);
    for(int i = 0; i < 5; i++) {
        int y_left = ((frame * 2 + i * 13) % 70) - 5;
        int y_right = ((frame * 2 + i * 17 + 35) % 70) - 5;
        char bit_l = '0' + ((frame + i) & 1);
        char bit_r = '0' + ((frame + i + 1) & 1);
        char str_l[2] = {bit_l, 0};
        char str_r[2] = {bit_r, 0};

        if(y_left >= 0 && y_left < 64) canvas_draw_str(canvas, 1, y_left, str_l);
        if(y_right >= 0 && y_right < 64) canvas_draw_str(canvas, 123, y_right, str_r);
    }

    // Corner spinners (slower)
    const char* spin = "|/-\\";
    char spinner[2] = {spin[(frame / 4) & 3], 0};
    canvas_draw_str(canvas, 1, 62, spinner);
    canvas_draw_str(canvas, 123, 62, spinner);
}

static bool protopirate_decode_input_callback(InputEvent* event, void* context) {
    UNUSED(context);

    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        if(g_decode_ctx && g_decode_ctx->state != DecodeStateIdle &&
           g_decode_ctx->state != DecodeStateDone) {
            if(g_decode_ctx->raw_reader) {
                raw_file_reader_free(g_decode_ctx->raw_reader);
                g_decode_ctx->raw_reader = NULL;
            }

            furi_string_set(g_decode_ctx->error_info, "Cancelled");
            g_decode_ctx->state = DecodeStateShowFailure;
            g_decode_ctx->result_display_counter = 0;
            furi_string_set(g_decode_ctx->result, "Cancelled by user");
        }
        return true;
    }

    return false;
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

// Receiver view callback for history navigation
static void
    protopirate_scene_sub_decode_receiver_callback(ProtoPirateCustomEvent event, void* context) {
    ProtoPirateApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

// Widget callback for buttons (used in signal info view)
static void protopirate_scene_sub_decode_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    ProtoPirateApp* app = context;

    if(type == InputTypeShort || type == InputTypeLong) {
        if(result == GuiButtonTypeRight) {
            // Save button in signal info view
            view_dispatcher_send_custom_event(
                app->view_dispatcher, ProtoPirateCustomEventSubDecodeSave);
        } else if(result == GuiButtonTypeLeft) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, ProtoPirateCustomEventSubDecodeBruteforceStart);
        }
    }
}

void protopirate_scene_sub_decode_on_enter(void* context) {
    ProtoPirateApp* app = context;

    if(!protopirate_ensure_receiver_view(app) || !protopirate_ensure_widget(app) ||
       !protopirate_ensure_view_about(app)) {
        notification_message(app->notifications, &sequence_error);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    if(!app->radio_initialized && !protopirate_radio_init(app)) {
        FURI_LOG_E(TAG, "Failed to initialize radio for sub decode scene");
        notification_message(app->notifications, &sequence_error);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    protopirate_rx_stack_resume_after_tx(app);
    if(!app->txrx->receiver) {
        FURI_LOG_E(TAG, "Failed to allocate receiver for sub decode scene");
        notification_message(app->notifications, &sequence_error);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    FURI_LOG_I(TAG, "Sub decode scene enter - Free heap: %zu", memmgr_get_free_heap());

    g_decode_ctx = malloc(sizeof(SubDecodeContext));
    if(!g_decode_ctx) {
        FURI_LOG_E(TAG, "Failed to allocate decode context");
        scene_manager_previous_scene(app->scene_manager);
        return;
    }
    memset(g_decode_ctx, 0, sizeof(SubDecodeContext));

    FURI_LOG_I(TAG, "After decode context alloc - Free heap: %zu", memmgr_get_free_heap());

    // Allocate history
    if(!app->txrx->history) {
        app->txrx->history = protopirate_history_alloc();
        if(!app->txrx->history) {
            FURI_LOG_E(TAG, "Failed to allocate history!");
            free(g_decode_ctx);
            g_decode_ctx = NULL;
            return;
        }
    }

    g_decode_ctx->file_path = furi_string_alloc();
    g_decode_ctx->protocol_name = furi_string_alloc();
    g_decode_ctx->result = furi_string_alloc();
    g_decode_ctx->error_info = furi_string_alloc();
    g_decode_ctx->decoded_string = furi_string_alloc();
    g_decode_ctx->state = DecodeStateIdle;
    g_decode_ctx->can_save = false;
    g_decode_ctx->save_data = NULL;
    g_decode_ctx->worker_startup_delay = 0;
    g_decode_ctx->history = app->txrx->history;
    //protopirate_history_reset(g_decode_ctx->history);
    g_decode_ctx->match_count = 0;
    g_decode_ctx->selected_history_index = 0;
    g_decode_ctx->raw_reader = NULL;

    protopirate_view_receiver_set_sub_decode_mode(app->protopirate_receiver, true);

    FURI_LOG_I(TAG, "After context setup - Free heap: %zu", memmgr_get_free_heap());

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, ".sub", &I_subghz_10px);
    browser_options.base_path = SUBGHZ_APP_FOLDER;
    browser_options.hide_ext = false;

    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
    furi_string_set(g_decode_ctx->file_path, SUBGHZ_APP_FOLDER);

    if(dialog_file_browser_show(
           dialogs, g_decode_ctx->file_path, g_decode_ctx->file_path, &browser_options)) {
        FURI_LOG_I(TAG, "Selected file: %s", furi_string_get_cstr(g_decode_ctx->file_path));
        g_decode_ctx->state = DecodeStateOpenFile;

        view_set_draw_callback(app->view_about, protopirate_decode_draw_callback);
        view_set_input_callback(app->view_about, protopirate_decode_input_callback);
        view_set_context(app->view_about, app);

        view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewAbout);
    } else {
        scene_manager_previous_scene(app->scene_manager);
    }

    furi_record_close(RECORD_DIALOGS);
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
                // History is updated in callback, just refresh animation
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

                    protopirate_scene_sub_decode_update_receiver_statusbar(app, ctx->history);
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
                flipper_format_rewind(ff);
                if(!flipper_format_read_string(ff, FF_PROTOCOL, protocol)) {
                    furi_string_set_str(protocol, "Unknown");
                }

                // Clean protocol name for filename
                furi_string_replace_all(protocol, "/", "_");
                furi_string_replace_all(protocol, " ", "_");

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
        } else if(event.event == ProtoPirateCustomEventSubDecodeBruteforceStart) {
            app->txrx->idx_menu_chosen = ctx->selected_history_index;
            if(protopirate_psa_bf_plugin_ensure_loaded(app) && app->psa_bf_plugin &&
               app->psa_bf_plugin->on_scene_event(
                   app, ProtoPiratePsaBfContextSubDecode, event)) {
                if(app->psa_bf_plugin->is_running(app)) {
                    view_dispatcher_switch_to_view(
                        app->view_dispatcher, ProtoPirateViewWidget);
                }
            }
            consumed = true;
            return consumed;

        } else if(event.event == ProtoPirateCustomEventPsaBruteforceComplete) {
            app->txrx->idx_menu_chosen = ctx->selected_history_index;
            if(protopirate_psa_bf_plugin_ensure_loaded(app) && app->psa_bf_plugin) {
                app->psa_bf_plugin->on_scene_event(
                    app, ProtoPiratePsaBfContextSubDecode, event);
            }
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
            // User pressed back from history - reset and go to main menu
            protopirate_history_reset(ctx->history);
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, ProtoPirateSceneStart);
            consumed = true;
        }
    }

    if(event.type == SceneManagerEventTypeTick) {
        consumed = true;
        ctx->animation_frame++;

        app->txrx->idx_menu_chosen = ctx->selected_history_index;
        if(protopirate_psa_bf_plugin_ensure_loaded(app) && app->psa_bf_plugin &&
           app->psa_bf_plugin->is_running(app) &&
           app->psa_bf_plugin->on_scene_event(
               app, ProtoPiratePsaBfContextSubDecode, event)) {
            return consumed;
        }

        FURI_LOG_D(TAG, "Tick: state=%d, frame=%u", ctx->state, ctx->animation_frame);

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
                ctx->result_display_counter = 0;
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

                success = true;
            } while(false);

            furi_string_free(temp_str);
            FURI_LOG_D(TAG, "ReadHeader: Freed temp_str");

            if(!success) {
                FURI_LOG_E(TAG, "ReadHeader: Failed, closing handles");
                close_file_handles(ctx);
                ctx->state = DecodeStateShowFailure;
                ctx->result_display_counter = 0;
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
                ctx->state = DecodeStateShowFailure;
                ctx->result_display_counter = 0;
            }

            FURI_LOG_I(TAG, "ReadHeader: Complete, next state: %d", ctx->state);
            break;
        }

        case DecodeStateStartingWorker: {
            FURI_LOG_I(
                TAG,
                "StartingWorker: Entry - delay=%d, Free heap: %zu",
                ctx->worker_startup_delay,
                memmgr_get_free_heap());

            if(ctx->worker_startup_delay < 3) {
                ctx->worker_startup_delay++;
                FURI_LOG_D(TAG, "StartingWorker: Delay tick %d", ctx->worker_startup_delay);
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
                const char* preset_name_short = preset_name_long;

                if(!strcmp(preset_name_long, "FuriHalSubGhzPresetOok270Async")) {
                    preset_name_short = "AM270";
                } else if(!strcmp(preset_name_long, "FuriHalSubGhzPresetOok650Async")) {
                    preset_name_short = "AM650";
                } else if(!strcmp(preset_name_long, "FuriHalSubGhzPreset2FSKDev238Async")) {
                    preset_name_short = "FM238";
                } else if(!strcmp(preset_name_long, "FuriHalSubGhzPreset2FSKDev12KAsync")) {
                    preset_name_short = "FM12K";
                } else if(!strcmp(preset_name_long, "FuriHalSubGhzPreset2FSKDev476Async")) {
                    preset_name_short = "FM476";
                } else if(!strcmp(preset_name_long, "FuriHalSubGhzPresetCustom")) {
                    preset_name_short = "CUSTOM";
                }

                size_t preset_index = subghz_setting_get_preset_count(app->setting);
                for(size_t i = 0; i < subghz_setting_get_preset_count(app->setting); i++) {
                    if(!strcmp(
                           subghz_setting_get_preset_name(app->setting, i), preset_name_short)) {
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

                uint8_t* preset_data = subghz_setting_get_preset_data(app->setting, preset_index);
                size_t preset_data_size =
                    subghz_setting_get_preset_data_size(app->setting, preset_index);

                if(preset_data == NULL) {
                    FURI_LOG_E(TAG, "Failed to get preset data!");
                    break;
                }

                protopirate_preset_init(
                    app, preset_name_short, ctx->frequency, preset_data, preset_data_size);

                if(!protopirate_refresh_protocol_registry(app, true) || !app->txrx->receiver) {
                    FURI_LOG_E(TAG, "Failed to rebuild receiver for preset %s", preset_name_short);
                    break;
                }

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
                ctx->result_display_counter = 0;
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
                ctx->result_display_counter = 0;
                notification_message(app->notifications, &sequence_error);
                break;
            }

            FURI_LOG_I(
                TAG, "StartingWorker: Opening raw file - Free heap: %zu", memmgr_get_free_heap());

            if(!raw_file_reader_open(ctx->raw_reader, furi_string_get_cstr(ctx->file_path))) {
                FURI_LOG_E(TAG, "Failed to open raw file");
                raw_file_reader_free(ctx->raw_reader);
                ctx->raw_reader = NULL;
                furi_string_set(ctx->result, "Failed to open RAW file");
                furi_string_set(ctx->error_info, "File open failed");
                ctx->state = DecodeStateShowFailure;
                ctx->result_display_counter = 0;
                notification_message(app->notifications, &sequence_error);
                break;
            }

            ctx->state = DecodeStateDecodingRaw;
            FURI_LOG_I(
                TAG, "StartingWorker: Ready to decode - Free heap: %zu", memmgr_get_free_heap());
            break;
        }

        case DecodeStateDecodingRaw: {
            if(!ctx->raw_reader) {
                FURI_LOG_E(TAG, "DecodingRaw: No raw reader");
                ctx->state = DecodeStateShowFailure;
                ctx->result_display_counter = 0;
                break;
            }

            bool level = false;
            uint32_t duration = 0;
            uint32_t samples_processed = 0;

            while(samples_processed < SAMPLES_TO_READ_PER_TICK) {
                if(!raw_file_reader_get_next(ctx->raw_reader, &level, &duration)) {
                    FURI_LOG_I(TAG, "DecodingRaw: File finished, matches=%u", ctx->match_count);

                    raw_file_reader_free(ctx->raw_reader);
                    ctx->raw_reader = NULL;

                    subghz_receiver_set_rx_callback(app->txrx->receiver, NULL, NULL);

                    uint16_t history_count = protopirate_history_get_item(ctx->history);

                    if(history_count > 0) {
                        ctx->state = DecodeStateShowSuccess;
                        ctx->selected_history_index = 0;
                        ctx->showing_signal_info = false;
                        ctx->result_display_counter = 0;
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
                        ctx->state = DecodeStateShowFailure;
                        ctx->result_display_counter = 0;
                        notification_message(app->notifications, &sequence_error);
                    }
                    break;
                }
                furi_thread_yield();
                subghz_receiver_decode(app->txrx->receiver, level, duration);
                samples_processed++;
            }
            break;
        }

        case DecodeStateShowSuccess: {
            ctx->result_display_counter++;
            if(ctx->result_display_counter >= SUCCESS_DISPLAY_TICKS) {
                // Check if we have history items (from RAW decoding) - show history list instead of widget
                uint16_t history_count = protopirate_history_get_item(ctx->history);
                if(history_count > 0) {
                    // Transition to showing history list
                    ctx->state = DecodeStateShowHistory;
                } else {
                    // No history items, show widget with result (for protocol decoding)
                    widget_reset(app->widget);
                    widget_add_text_scroll_element(
                        app->widget, 0, 0, 128, 50, furi_string_get_cstr(ctx->result));

                    // Add save button if we can save
                    if(ctx->can_save) {
                        widget_add_button_element(
                            app->widget,
                            GuiButtonTypeRight,
                            "Save",
                            protopirate_scene_sub_decode_widget_callback,
                            app);
                    }

                    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewWidget);
                    ctx->state = DecodeStateDone;
                }
            }
            break;
        }

        case DecodeStateShowFailure: {
            ctx->result_display_counter++;
            if(ctx->result_display_counter >= FAILURE_DISPLAY_TICKS) {
                widget_reset(app->widget);
                widget_add_text_scroll_element(
                    app->widget, 0, 0, 128, 64, furi_string_get_cstr(ctx->result));
                view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewWidget);
                ctx->state = DecodeStateDone;
            }
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

                protopirate_scene_sub_decode_update_receiver_statusbar(app, ctx->history);

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

                // Store reference to history item's flipper format for saving
                FlipperFormat* ff =
                    protopirate_history_get_raw_data(ctx->history, ctx->selected_history_index);
                if(ff) {
                    ctx->can_save = true;

                    FuriString* proto_str = furi_string_alloc();
                    flipper_format_rewind(ff);
                    bool is_psa = flipper_format_read_string(ff, FF_PROTOCOL, proto_str) &&
                                  furi_string_cmp_str(proto_str, "PSA") == 0;
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
                        }
                    }
                }

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

        // Force view update to show animation - only when actually showing animation
        if(ctx->state != DecodeStateDone && ctx->state != DecodeStateShowHistory &&
           ctx->state != DecodeStateShowSignalInfo) {
            view_commit_model(app->view_about, true);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        app->txrx->idx_menu_chosen = ctx->selected_history_index;
        if(protopirate_psa_bf_plugin_ensure_loaded(app) && app->psa_bf_plugin &&
           app->psa_bf_plugin->on_scene_event(
               app, ProtoPiratePsaBfContextSubDecode, event)) {
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

    subghz_receiver_reset(app->txrx->receiver);

    subghz_receiver_set_rx_callback(app->txrx->receiver, NULL, NULL);

    protopirate_psa_bf_context_release(app);

    if(g_decode_ctx) {
        if(g_decode_ctx->raw_reader) {
            raw_file_reader_free(g_decode_ctx->raw_reader);
            g_decode_ctx->raw_reader = NULL;
        }

        close_file_handles(g_decode_ctx);

        if(g_decode_ctx->save_data) {
            flipper_format_free(g_decode_ctx->save_data);
        }

        furi_string_free(g_decode_ctx->file_path);
        furi_string_free(g_decode_ctx->protocol_name);
        furi_string_free(g_decode_ctx->result);
        furi_string_free(g_decode_ctx->error_info);
        furi_string_free(g_decode_ctx->decoded_string);
        free(g_decode_ctx);
        g_decode_ctx = NULL;
    }

    if(app->txrx->history) {
        protopirate_history_reset(app->txrx->history);

        FURI_LOG_D(TAG, "Freeing history %p", app->txrx->history);
        protopirate_history_free(app->txrx->history);
        app->txrx->history = NULL;
    }

    view_set_draw_callback(app->view_about, NULL);
    view_set_input_callback(app->view_about, NULL);
    widget_reset(app->widget);

    protopirate_view_receiver_reset_menu(app->protopirate_receiver);
}
#endif
