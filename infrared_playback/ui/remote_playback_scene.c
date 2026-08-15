
#include "ui.h"

#include <furi.h>
#include <gui/elements.h>
#include <infrared_error_code.h>
#include <infrared_signal.h>
#include <notification/notification_messages.h>

#define LOG_TAG "infrared_playback_remote_playback"

#define IR_PAYLOAD_START        "\nname:"
#define IR_PAYLOAD_START_LENGTH strlen(IR_PAYLOAD_START)

typedef struct {
    UI* ui;
} RemotePlaybackSceneViewModel;

// Definition comes from infrared_signal.c
struct InfraredSignal {
    bool is_raw;
    union {
        InfraredMessage message;
        InfraredRawSignal raw;
    } payload;
};

static void remote_playback_scene_draw_callback(Canvas* canvas, void* model) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_check(canvas);
    furi_check(model);
    
    RemotePlaybackSceneViewModel* view_model = model;
    UI* ui = view_model->ui;
    
    if(ui->remote_playback_scene->is_loading) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 0, 0, AlignLeft, AlignTop, "Loading");
        return;
    }
    
    if(ui->remote_playback_scene->has_error) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 0, 0, AlignLeft, AlignTop, "ERROR");
        
        canvas_set_font(canvas, FontSecondary);
        elements_multiline_text_aligned(canvas, 0, 12, AlignLeft, AlignTop, furi_string_get_cstr(ui->remote_playback_scene->error_string));
        return;
    }
    
    const char* full_path = furi_string_get_cstr(ui->remote_select_scene->file_path);
    const char* filename = strrchr(full_path, '/');
    
    if(filename) {
        filename++; // Skip the '/'
    } else {
        filename = full_path;
    }
    
    canvas_set_font(canvas, FontPrimary);
    elements_multiline_text_aligned(canvas, 0, 0, AlignLeft, AlignTop, filename);
    
    canvas_draw_triangle(canvas, 32, 48, 16, 10, CanvasDirectionBottomToTop);
    canvas_draw_triangle(canvas, 32, 80, 16, 10, CanvasDirectionTopToBottom);
    
    canvas_set_font(canvas, FontSecondary);
    char index[5];
    index[4] = '\0';
    snprintf(index, 12, "%d", ui->remote_playback_scene->current_ir_payload + 1);
    canvas_draw_str_aligned(canvas, 27, 62, AlignRight, AlignBottom, index);
    canvas_draw_str_aligned(canvas, 32, 62, AlignCenter, AlignBottom, "/");
    snprintf(index, 12, "%d", ui->remote_playback_scene->num_ir_payloads);
    canvas_draw_str_aligned(canvas, 37, 62, AlignLeft, AlignBottom, index);
    
    // TODO Better error handling
    FlipperFormat* ff = flipper_format_buffered_file_alloc(ui->storage);
    FuriString* name = furi_string_alloc();
    
    if(!flipper_format_buffered_file_open_existing(ff, furi_string_get_cstr(ui->remote_select_scene->file_path))) {
        FURI_LOG_E(LOG_TAG, "Failed to open %s.", furi_string_get_cstr(ui->remote_select_scene->file_path));
        goto draw_end;
    }
    
    if(!flipper_format_seek(ff, ui->remote_playback_scene->remote_offsets[ui->remote_playback_scene->current_ir_payload], FlipperFormatOffsetFromStart)) {
        FURI_LOG_E(LOG_TAG, "Failed to load signal '%d' from file '%s'", ui->remote_playback_scene->current_ir_payload, furi_string_get_cstr(ui->remote_select_scene->file_path));
        goto draw_end;
    }
    
    InfraredErrorCode error = infrared_signal_read_name(ff, name);
    if(INFRARED_ERROR_PRESENT(error)) {
        FURI_LOG_E(LOG_TAG, "Failed to load signal '%d' from file '%s'", ui->remote_playback_scene->current_ir_payload, furi_string_get_cstr(ui->remote_select_scene->file_path));
        goto draw_end;
    }
    
    canvas_draw_str_aligned(canvas, 32, 66, AlignCenter, AlignTop, furi_string_get_cstr(name));
    
    draw_end:
    furi_string_free(name);
    flipper_format_free(ff);
}

static void clean_up_signal(InfraredSignal* signal){
    if(!signal->is_raw) {
        // Only cleans up raw signals right now
        return;
    }
    
    const InfraredRawSignal* raw_signal = &signal->payload.raw;
    
    for(size_t i = 0; i < raw_signal->timings_size; i++){
        // Ensure all timings are below 1 second
        if(raw_signal->timings[i] > 1000000) {
            FURI_LOG_W(LOG_TAG, "Found a timing of %lu. Truncating to 1 second.", raw_signal->timings[i]);
            raw_signal->timings[i] = 1000000;
        }
    }
}

static bool remote_playback_scene_input_callback(InputEvent* event, void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_check(event);
    furi_check(context);
    
    UI* ui = context;
    bool processed = false;
    
    if(event->type == InputTypePress || event->type == InputTypeRepeat) {
        if(event->key == InputKeyUp) {
            ui->remote_playback_scene->current_ir_payload--;
            
            if(ui->remote_playback_scene->current_ir_payload > ui->remote_playback_scene->num_ir_payloads) {
                ui->remote_playback_scene->current_ir_payload = ui->remote_playback_scene->num_ir_payloads - 1;
            }
            
            processed = true;
        } else if(event->key == InputKeyDown) {
            ui->remote_playback_scene->current_ir_payload++;
            ui->remote_playback_scene->current_ir_payload = ui->remote_playback_scene->current_ir_payload % ui->remote_playback_scene->num_ir_payloads;
            processed = true;
        } else if(event->key == InputKeyOk) {
            // TODO Better error handling
            FlipperFormat* ff = flipper_format_buffered_file_alloc(ui->storage);
            InfraredSignal* signal = infrared_signal_alloc();
            
            if(!flipper_format_buffered_file_open_existing(ff, furi_string_get_cstr(ui->remote_select_scene->file_path))) {
                FURI_LOG_E(LOG_TAG, "Failed to open %s.", furi_string_get_cstr(ui->remote_select_scene->file_path));
                goto input_ok_error_handler;
            }
            
            if(!flipper_format_seek(ff, ui->remote_playback_scene->remote_offsets[ui->remote_playback_scene->current_ir_payload], FlipperFormatOffsetFromStart)) {
                FURI_LOG_E(LOG_TAG, "Failed to load signal '%d' from file '%s'", ui->remote_playback_scene->current_ir_payload, furi_string_get_cstr(ui->remote_select_scene->file_path));
                goto input_ok_error_handler;
            }
            
            InfraredErrorCode error = infrared_signal_read_body(signal, ff);
            if(INFRARED_ERROR_PRESENT(error)) {
                FURI_LOG_E(LOG_TAG, "Failed to load signal '%d' from file '%s'", ui->remote_playback_scene->current_ir_payload, furi_string_get_cstr(ui->remote_select_scene->file_path));
                goto input_ok_error_handler;
            }
            
            clean_up_signal(signal);
            
            notification_message(ui->notifications, &sequence_blink_start_cyan);
            infrared_signal_transmit(signal);
            notification_message(ui->notifications, &sequence_blink_stop);
            
            input_ok_error_handler:
            infrared_signal_free(signal);
            flipper_format_free(ff);
            processed = true;
        } else if(event->key == InputKeyBack) {
            scene_manager_previous_scene(ui->scene_manager);
            processed = true;
        }
        
        view_commit_model(ui->remote_playback_scene->view, true);
    }
    
    return processed;
}

void remote_playback_scene_on_enter(void* context){
    FURI_LOG_T(LOG_TAG, __func__);
    furi_check(context);
    
    UI* ui = context;
    ui->remote_playback_scene->is_loading = true;
    view_dispatcher_switch_to_view(ui->view_dispatcher, View_RemotePlaybackDisplay);
    
    ui->remote_playback_scene->has_error = false;
    
    ui->remote_playback_scene->current_ir_payload = 0;
    ui->remote_playback_scene->num_ir_payloads = 0;
    
    FlipperFormat* ff = flipper_format_buffered_file_alloc(ui->storage);
    InfraredSignal* signal = infrared_signal_alloc();
    
    if(!flipper_format_buffered_file_open_existing(ff, furi_string_get_cstr(ui->remote_select_scene->file_path))) {
        FURI_LOG_E(LOG_TAG, "Failed to open %s.", furi_string_get_cstr(ui->remote_select_scene->file_path));
        furi_string_set_str(ui->remote_playback_scene->error_string, "Failed to open\nfile");
        ui->remote_playback_scene->has_error = true;
        goto load_end;
    }
    
    InfraredErrorCode error = InfraredErrorCodeNone;
    
    while(true) {
        size_t loc = flipper_format_tell(ff);
        error = infrared_signal_read_body(signal, ff);
        
        if(error == InfraredErrorCodeSignalUnableToReadType) {
            // Most likely the end of file
            break;
        }
        
        if(INFRARED_ERROR_PRESENT(error)){
            FURI_LOG_D(LOG_TAG, "Flipper format error: %08X", error);
            continue;
        }
        
        if(ui->remote_playback_scene->num_ir_payloads == MAX_NUM_REMOTES) {
            FURI_LOG_E(LOG_TAG, "Too many IR payloads");
            furi_string_set_str(ui->remote_playback_scene->error_string, "Too many IR payloads");
            ui->remote_playback_scene->has_error = true;
            break;
        }
        
        ui->remote_playback_scene->remote_offsets[ui->remote_playback_scene->num_ir_payloads] = loc;
        ui->remote_playback_scene->num_ir_payloads++;
    }
    
    if(ui->remote_playback_scene->num_ir_payloads == 0) {
        FURI_LOG_E(LOG_TAG, "No IR payloads");
        furi_string_set_str(ui->remote_playback_scene->error_string, "No IR payloads");
        ui->remote_playback_scene->has_error = true;
        goto load_end;
    }
    
    load_end:
    infrared_signal_free(signal);
    flipper_format_free(ff);
    
    ui->remote_playback_scene->is_loading = false;
    view_commit_model(ui->remote_playback_scene->view, true);
}

bool remote_playback_scene_on_event(void* context, SceneManagerEvent event){
    FURI_LOG_T(LOG_TAG, __func__);
    furi_check(context);
    
    if (event.type == SceneManagerEventTypeCustom) {
        return true;
    }
    
    return false;
}

void remote_playback_scene_on_exit(void* context){
    FURI_LOG_T(LOG_TAG, __func__);
    furi_check(context);
}

RemotePlaybackScene* remote_playback_scene_alloc(UI* ui) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_check(ui);
    
    RemotePlaybackScene* scene = malloc(sizeof(RemotePlaybackScene));
    
    scene->view = view_alloc();
    view_set_orientation(scene->view, ViewOrientationVertical);
    view_set_input_callback(scene->view, remote_playback_scene_input_callback);
    view_set_context(scene->view, ui); // Context for the input callback
    
    view_set_draw_callback(scene->view, remote_playback_scene_draw_callback);
    view_allocate_model(scene->view, ViewModelTypeLockFree, sizeof(RemotePlaybackSceneViewModel));
    ((RemotePlaybackSceneViewModel*)view_get_model(scene->view))->ui = ui;
    
    scene->error_string = furi_string_alloc();
    
    return scene;
}

void remote_playback_scene_free(RemotePlaybackScene* scene) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_check(scene);
    
    view_free_model(scene->view);
    view_free(scene->view);
    furi_string_free(scene->error_string);
    
    free(scene);
}
