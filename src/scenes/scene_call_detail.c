#include "scene_call_detail.h"

#include "../api_caller.h"
#include "../api/call_runner.h"
#include "../utils/logger.h"

typedef enum {
    CallDetailEventRun = 0,
    CallDetailEventEdit = 1,
} CallDetailEvent;

typedef enum {
    CallDetailStateMenu,
    CallDetailStateResult,
} CallDetailState;

static void api_caller_scene_call_detail_item_callback(void* context, uint32_t index) {
    furi_assert(context);
    AppContext* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

/** The call selected in the list scene (call_edit_index is reused as selection). */
static const CallEntry* api_caller_scene_call_detail_get_call(AppContext* app) {
    if(app->call_edit_index >= app->call_history_count) {
        return NULL;
    }
    return &app->call_history[app->call_edit_index];
}

static void api_caller_scene_call_detail_render_menu(AppContext* app) {
    const CallEntry* call = api_caller_scene_call_detail_get_call(app);
    furi_assert(call);

    FuriString* header =
        furi_string_alloc_printf("%s %s", call_method_names[call->method], call->url);

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, furi_string_get_cstr(header));
    submenu_add_item(
        app->submenu,
        "Esegui",
        CallDetailEventRun,
        api_caller_scene_call_detail_item_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Modifica",
        CallDetailEventEdit,
        api_caller_scene_call_detail_item_callback,
        app);

    furi_string_free(header);

    view_dispatcher_switch_to_view(app->view_dispatcher, ApiCallerViewWifiScan);
}

/** Start the request and show the progress screen. */
static void api_caller_scene_call_detail_run(AppContext* app) {
    const CallEntry* call = api_caller_scene_call_detail_get_call(app);
    furi_assert(call);

    FuriString* log =
        furi_string_alloc_printf("SEND %s %s", call_method_names[call->method], call->url);
    logger_log(furi_string_get_cstr(log));
    furi_string_free(log);

    text_box_reset(app->text_box);
    text_box_set_text(app->text_box, "Invio richiesta...\nPremi BACK per tornare al menu.");
    app->call_progress_last_second = 0;

    if(!call_runner_start(app, call)) {
        // Immediate failure (no board or UART error)
        FuriString* text = furi_string_alloc_printf("Invio fallito.\n%s", app->call_error);
        text_box_set_text(app->text_box, furi_string_get_cstr(text));
        furi_string_free(text);

        log = furi_string_alloc_printf("ERROR %s", app->call_error);
        logger_log(furi_string_get_cstr(log));
        furi_string_free(log);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, ApiCallerViewTextBox);
}

/** Show the final result in the TextBox and append it to the debug log. */
static void api_caller_scene_call_detail_render_result(AppContext* app) {
    const CallEntry* call = api_caller_scene_call_detail_get_call(app);
    furi_assert(call);

    text_box_reset(app->text_box);

    FuriString* text = furi_string_alloc();
    FuriString* log = NULL;

    if(app->call_error[0] != '\0') {
        furi_string_printf(text, "Errore\n%s", app->call_error);
        log = furi_string_alloc_printf("ERROR %s", app->call_error);
    } else {
        const char* response = app->call_response[0] != '\0' ? app->call_response :
                                                               "(risposta vuota)";
        furi_string_printf(
            text,
            "%s %s\nStatus: %d\n\nRisposta:\n%s",
            call_method_names[call->method],
            call->url,
            app->call_status_code,
            response);
        log = furi_string_alloc_printf(
            "RESULT %s %s -> %d (%u bytes)",
            call_method_names[call->method],
            call->url,
            app->call_status_code,
            (unsigned int)app->call_response_len);
    }

    text_box_set_text(app->text_box, furi_string_get_cstr(text));
    logger_log(furi_string_get_cstr(log));

    furi_string_free(log);
    furi_string_free(text);

    view_dispatcher_switch_to_view(app->view_dispatcher, ApiCallerViewTextBox);
}

/** Update the elapsed-seconds feedback while the request is in flight. */
static void api_caller_scene_call_detail_update_progress(AppContext* app) {
    uint32_t seconds = (furi_get_tick() - app->call_start_tick) / 1000;
    if(seconds == app->call_progress_last_second) {
        return;
    }
    app->call_progress_last_second = seconds;

    FuriString* text = furi_string_alloc_printf(
        "Invio richiesta... %lus\nPremi BACK per tornare al menu.", (unsigned long)seconds);
    text_box_set_text(app->text_box, furi_string_get_cstr(text));
    furi_string_free(text);
}

void api_caller_scene_call_detail_on_enter(void* context) {
    furi_assert(context);
    AppContext* app = context;

    if(api_caller_scene_call_detail_get_call(app) == NULL) {
        // The selection disappeared (e.g. deleted while editing): go back
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    api_caller_scene_call_detail_render_menu(app);
}

bool api_caller_scene_call_detail_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    AppContext* app = context;
    bool consumed = false;

    uint32_t state = scene_manager_get_scene_state(app->scene_manager, ApiCallerSceneCallDetail);

    if(event.type == SceneManagerEventTypeBack) {
        if(state == CallDetailStateResult) {
            // Back from the response screen: return to the detail menu
            scene_manager_set_scene_state(
                app->scene_manager, ApiCallerSceneCallDetail, CallDetailStateMenu);
            api_caller_scene_call_detail_render_menu(app);
            consumed = true;
        }
        return consumed;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case CallDetailEventRun:
            scene_manager_set_scene_state(
                app->scene_manager, ApiCallerSceneCallDetail, CallDetailStateResult);
            api_caller_scene_call_detail_run(app);
            consumed = true;
            break;
        case CallDetailEventEdit: {
            const CallEntry* call = api_caller_scene_call_detail_get_call(app);
            if(call != NULL) {
                // Pre-fill the form; call_edit_index already holds the selection
                app->call_form = *call;
                scene_manager_set_scene_state(app->scene_manager, ApiCallerSceneCallAdd, 0);
                scene_manager_next_scene(app->scene_manager, ApiCallerSceneCallAdd);
                consumed = true;
            }
            break;
        }
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        if(state == CallDetailStateResult && app->call_in_progress) {
            if(call_runner_poll(app)) {
                api_caller_scene_call_detail_render_result(app);
            } else {
                api_caller_scene_call_detail_update_progress(app);
            }
            consumed = true;
        }
    }

    return consumed;
}

void api_caller_scene_call_detail_on_exit(void* context) {
    furi_assert(context);
    AppContext* app = context;
    submenu_reset(app->submenu);
    text_box_reset(app->text_box);
}
