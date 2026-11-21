#include "../../rf_studio_i.h"

#include <flipper_format/flipper_format.h>

/*
 * Capture confirmation (Addendum 001 Section 5). Every value shown comes from the capture
 * result. A received transmission is not assumed to be a reusable command.
 */

static void rfs_result_button_callback(GuiButtonType result, InputType type, void* context) {
    RfStudioApp* app = context;
    if(type != InputTypeShort) return;
    if(result == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, RfsEventResultRetry);
    } else if(result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, RfsEventResultSave);
    }
}

static const char* rfs_result_title(const RfsCaptureResult* result) {
    switch(result->kind) {
    case RfsKindStatic:
        return "Signal Captured";
    case RfsKindDynamic:
        return "Rolling Code Found";
    case RfsKindRaw:
    case RfsKindBinRaw:
        return "Raw Recording";
    default:
        return "Unknown Signal";
    }
}

/* Find another button on the open remote whose signal has the same protocol and key. A match
 * usually means the wrong button was captured or the code was misread (Section 8.5). */
static bool
    rfs_result_find_duplicate(RfStudioApp* app, const RfsCaptureResult* result, FuriString* out) {
    if(app->capture_target == RfsCaptureForLibrary || !result->decoded) return false;
    FuriString* key = furi_string_alloc();
    FuriString* other = furi_string_alloc();
    FuriString* path = furi_string_alloc();
    FlipperFormat* ff = flipper_format_file_alloc(app->storage);
    bool found = false;
    if(rfs_capture_result_key(app->capture, key)) {
        for(size_t i = 0; i < app->remote.count && !found; i++) {
            if(app->capture_target == RfsCaptureForRerecord && (int32_t)i == app->button_index)
                continue;
            const RfsButton* button = &app->remote.buttons[i];
            RfsSignalMeta* meta = rfs_signal_find(&app->signals, button->signal_id);
            if(!meta || strcmp(meta->protocol, result->protocol) != 0) continue;
            rfs_signal_path(meta->id, path);
            if(flipper_format_file_open_existing(ff, furi_string_get_cstr(path)) &&
               flipper_format_read_string(ff, "Key", other) && furi_string_equal(key, other)) {
                furi_string_set(out, button->name);
                found = true;
            }
            flipper_format_file_close(ff);
        }
    }
    flipper_format_free(ff);
    furi_string_free(path);
    furi_string_free(other);
    furi_string_free(key);
    return found;
}

void rfs_scene_capture_result_on_enter(void* context) {
    RfStudioApp* app = context;
    const RfsCaptureResult* result = rfs_capture_result(app->capture);
    Widget* widget = app->widget;
    widget_reset(widget);

    char mhz[16];
    rfs_format_mhz(result->frequency, mhz, sizeof(mhz));
    RfsReplay replay = rfs_protocol_replay(result->kind, result->protocol, false);
    FuriString* text = furi_string_alloc();
    furi_string_printf(
        text,
        "\e#%s\n"
        "Freq: %s MHz %s\n"
        "Type: %s\n"
        "Protocol: %s\n"
        "Signal: %s\n",
        rfs_result_title(result),
        mhz,
        result->preset,
        rfs_kind_name(result->kind),
        result->protocol,
        rfs_replay_name(replay));
    if(result->decoded && !result->confirmed) {
        furi_string_cat_str(
            text,
            "\nDecoded from one frame only.\nTest it after saving, or Retry\nand hold the button longer.\n");
    }
    FuriString* duplicate = furi_string_alloc();
    if(rfs_result_find_duplicate(app, result, duplicate)) {
        furi_string_cat_printf(
            text,
            "\nWARNING: same code as\n\"%s\". Retry if you\npressed a different button.\n",
            furi_string_get_cstr(duplicate));
    }
    furi_string_free(duplicate);
    if(!furi_string_empty(result->text)) {
        furi_string_cat_printf(text, "\n%s", furi_string_get_cstr(result->text));
    }
    widget_add_text_scroll_element(widget, 0, 0, 128, 52, furi_string_get_cstr(text));
    furi_string_free(text);

    widget_add_button_element(widget, GuiButtonTypeLeft, "Retry", rfs_result_button_callback, app);
    widget_add_button_element(widget, GuiButtonTypeRight, "Save", rfs_result_button_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, RfsViewWidget);
}

static void rfs_result_default_name(RfStudioApp* app, const RfsCaptureResult* result) {
    char mhz[16];
    switch(app->capture_target) {
    case RfsCaptureForNewButton:
        snprintf(
            app->text_buffer,
            sizeof(app->text_buffer),
            "Button %u",
            (unsigned)(app->remote.count + 1));
        break;
    case RfsCaptureForLibrary: {
        rfs_format_mhz(result->frequency, mhz, sizeof(mhz));
        FuriString* name = furi_string_alloc_printf("%s %s", result->protocol, mhz);
        strlcpy(app->text_buffer, furi_string_get_cstr(name), sizeof(app->text_buffer));
        furi_string_free(name);
    } break;
    default:
        break;
    }
}

bool rfs_scene_capture_result_on_event(void* context, SceneManagerEvent event) {
    RfStudioApp* app = context;
    if(event.type == SceneManagerEventTypeBack) {
        /* Back is Cancel: the recording is discarded. */
        rfs_app_exit_capture(app);
        return true;
    }
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == RfsEventResultRetry) {
        rfs_capture_discard(app->capture);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    if(event.event == RfsEventResultSave) {
        const RfsCaptureResult* result = rfs_capture_result(app->capture);
        if(app->capture_target == RfsCaptureForRerecord) {
            /* The button keeps its name; only its signal changes. */
            if(app->button_index >= 0 && (size_t)app->button_index < app->remote.count) {
                char name[RFS_NAME_LEN + 1];
                strlcpy(name, app->remote.buttons[app->button_index].name, sizeof(name));
                rfs_app_finish_button_capture(app, name);
            }
            return true;
        }
        rfs_result_default_name(app, result);
        app->text_purpose = app->capture_target == RfsCaptureForLibrary ? RfsTextSignalNew :
                                                                          RfsTextButtonNew;
        scene_manager_next_scene(app->scene_manager, RfsSceneTextInput);
        return true;
    }
    return false;
}

void rfs_scene_capture_result_on_exit(void* context) {
    RfStudioApp* app = context;
    widget_reset(app->widget);
}
