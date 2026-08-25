#include "scene_call_add.h"

#include "../api_caller.h"
#include "../utils/call_history.h"

typedef enum {
    // Form item indices, used as custom events
    CallAddEventUrl = 0,
    CallAddEventQuery = 3,
    CallAddEventHeaders = 4,
    CallAddEventBody = 5,
    CallAddEventSave = 6,
    CallAddEventDelete = 7,
    CallAddEventInputDone,
} CallAddEvent;

typedef enum {
    CallAddStateForm,
    CallAddStateUrl,
    CallAddStateQuery,
    CallAddStateHeaders,
    CallAddStateBody,
} CallAddState;

static bool api_caller_scene_call_add_is_edit(AppContext* app) {
    return app->call_edit_index != CALL_EDIT_INDEX_NONE;
}

/** Short display value for a VariableItemList row (long values get clipped). */
static void
    api_caller_scene_call_add_display_value(char* out, size_t out_size, const char* value) {
    furi_assert(out);
    if(strlen(value) > 24) {
        snprintf(out, out_size, "%.21s...", value);
    } else {
        snprintf(out, out_size, "%s", value);
    }
}

static void api_caller_scene_call_add_item_callback(void* context, uint32_t index) {
    furi_assert(context);
    AppContext* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void api_caller_scene_call_add_protocol_change(VariableItem* item) {
    AppContext* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->call_form.protocol = (CallProtocol)index;
    variable_item_set_current_value_text(item, call_protocol_names[index]);
}

static void api_caller_scene_call_add_method_change(VariableItem* item) {
    AppContext* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->call_form.method = (CallMethod)index;
    variable_item_set_current_value_text(item, call_method_names[index]);
}

static void api_caller_scene_call_add_input_callback(void* context) {
    furi_assert(context);
    AppContext* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, CallAddEventInputDone);
}

/** Render the form (new or edit mode) and switch to the form view. */
static void api_caller_scene_call_add_render_form(AppContext* app) {
    char display[CALL_URL_MAX];

    variable_item_list_reset(app->var_item_list_call);

    VariableItem* item = variable_item_list_add(app->var_item_list_call, "URL", 0, NULL, app);
    api_caller_scene_call_add_display_value(display, sizeof(display), app->call_form.url);
    variable_item_set_current_value_text(item, display);

    item = variable_item_list_add(
        app->var_item_list_call,
        "Tipo",
        CallProtocolCount,
        api_caller_scene_call_add_protocol_change,
        app);
    variable_item_set_current_value_index(item, app->call_form.protocol);
    variable_item_set_current_value_text(item, call_protocol_names[app->call_form.protocol]);

    item = variable_item_list_add(
        app->var_item_list_call,
        "Metodo",
        CallMethodCount,
        api_caller_scene_call_add_method_change,
        app);
    variable_item_set_current_value_index(item, app->call_form.method);
    variable_item_set_current_value_text(item, call_method_names[app->call_form.method]);

    item = variable_item_list_add(app->var_item_list_call, "Query", 0, NULL, app);
    api_caller_scene_call_add_display_value(display, sizeof(display), app->call_form.query);
    variable_item_set_current_value_text(item, display);

    item = variable_item_list_add(app->var_item_list_call, "Headers", 0, NULL, app);
    api_caller_scene_call_add_display_value(display, sizeof(display), app->call_form.headers);
    variable_item_set_current_value_text(item, display);

    item = variable_item_list_add(app->var_item_list_call, "Body", 0, NULL, app);
    api_caller_scene_call_add_display_value(display, sizeof(display), app->call_form.body);
    variable_item_set_current_value_text(item, display);

    variable_item_list_add(app->var_item_list_call, "Salva", 0, NULL, app);

    if(api_caller_scene_call_add_is_edit(app)) {
        variable_item_list_add(app->var_item_list_call, "Elimina", 0, NULL, app);
    }

    variable_item_list_set_enter_callback(
        app->var_item_list_call, api_caller_scene_call_add_item_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, ApiCallerViewCallMenu);
}

/** Open the TextInput view for the given form field (writes into call_form). */
static void api_caller_scene_call_add_start_input(AppContext* app, CallAddState state) {
    const char* header;
    char* buffer;
    size_t buffer_size;

    switch(state) {
    case CallAddStateUrl:
        header = "URL";
        buffer = app->call_form.url;
        buffer_size = sizeof(app->call_form.url);
        break;
    case CallAddStateQuery:
        header = "Query parameters";
        buffer = app->call_form.query;
        buffer_size = sizeof(app->call_form.query);
        break;
    case CallAddStateHeaders:
        header = "Headers (JSON)";
        buffer = app->call_form.headers;
        buffer_size = sizeof(app->call_form.headers);
        break;
    case CallAddStateBody:
        header = "Body (JSON)";
        buffer = app->call_form.body;
        buffer_size = sizeof(app->call_form.body);
        break;
    default:
        furi_crash("Call add: bad text field state");
    }

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, header);
    text_input_set_result_callback(
        app->text_input, api_caller_scene_call_add_input_callback, app, buffer, buffer_size, true);

    view_dispatcher_switch_to_view(app->view_dispatcher, ApiCallerViewTextInput);
}

/** Make sure the URL has a scheme, using the selected protocol when missing. */
static void api_caller_scene_call_add_normalize_url(AppContext* app) {
    if(strstr(app->call_form.url, "://") != NULL) {
        return;
    }
    const char* scheme = app->call_form.protocol == CallProtocolHttps ? "https://" : "http://";
    size_t scheme_len = strlen(scheme);
    size_t url_len = strlen(app->call_form.url);
    if(scheme_len + url_len >= sizeof(app->call_form.url)) {
        // No room for the scheme: keep the URL as-is
        return;
    }
    memmove(app->call_form.url + scheme_len, app->call_form.url, url_len + 1);
    memcpy(app->call_form.url, scheme, scheme_len);
}

static void api_caller_scene_call_add_do_save(AppContext* app) {
    if(strlen(app->call_form.url) == 0) {
        // Force the URL input: a call cannot be saved without a URL
        scene_manager_set_scene_state(app->scene_manager, ApiCallerSceneCallAdd, CallAddStateUrl);
        api_caller_scene_call_add_start_input(app, CallAddStateUrl);
        return;
    }

    api_caller_scene_call_add_normalize_url(app);

    bool ok;
    if(api_caller_scene_call_add_is_edit(app)) {
        ok = call_history_update(app, app->call_edit_index);
    } else {
        ok = call_history_add(app);
    }

    if(ok) {
        scene_manager_previous_scene(app->scene_manager);
    } else {
        FURI_LOG_W("ApiCaller", "Failed to save the call");
    }
}

static void api_caller_scene_call_add_do_delete(AppContext* app) {
    if(api_caller_scene_call_add_is_edit(app)) {
        call_history_remove(app, app->call_edit_index);
    }
    // Jump to the list (the previous scene may be the detail with a dead index)
    scene_manager_search_and_switch_to_another_scene(app->scene_manager, ApiCallerSceneCallList);
}

void api_caller_scene_call_add_on_enter(void* context) {
    furi_assert(context);
    AppContext* app = context;

    if(!api_caller_scene_call_add_is_edit(app)) {
        // New call: start with a clean form
        memset(&app->call_form, 0, sizeof(app->call_form));
        app->call_form.protocol = CallProtocolHttp;
        app->call_form.method = CallMethodGet;
    }

    api_caller_scene_call_add_render_form(app);
}

bool api_caller_scene_call_add_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    AppContext* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        uint32_t state = scene_manager_get_scene_state(app->scene_manager, ApiCallerSceneCallAdd);
        if(state != CallAddStateForm) {
            // Back from a text field: return to the form
            scene_manager_set_scene_state(
                app->scene_manager, ApiCallerSceneCallAdd, CallAddStateForm);
            api_caller_scene_call_add_render_form(app);
            consumed = true;
        }
        return consumed;
    }

    if(event.type != SceneManagerEventTypeCustom) {
        return false;
    }

    uint32_t state = scene_manager_get_scene_state(app->scene_manager, ApiCallerSceneCallAdd);

    if(state != CallAddStateForm) {
        if(event.event == CallAddEventInputDone) {
            // The TextInput wrote directly into the call_form buffer
            scene_manager_set_scene_state(
                app->scene_manager, ApiCallerSceneCallAdd, CallAddStateForm);
            api_caller_scene_call_add_render_form(app);
            consumed = true;
        }
        return consumed;
    }

    switch(event.event) {
    case CallAddEventUrl:
        scene_manager_set_scene_state(app->scene_manager, ApiCallerSceneCallAdd, CallAddStateUrl);
        api_caller_scene_call_add_start_input(app, CallAddStateUrl);
        consumed = true;
        break;
    case CallAddEventQuery:
        scene_manager_set_scene_state(
            app->scene_manager, ApiCallerSceneCallAdd, CallAddStateQuery);
        api_caller_scene_call_add_start_input(app, CallAddStateQuery);
        consumed = true;
        break;
    case CallAddEventHeaders:
        scene_manager_set_scene_state(
            app->scene_manager, ApiCallerSceneCallAdd, CallAddStateHeaders);
        api_caller_scene_call_add_start_input(app, CallAddStateHeaders);
        consumed = true;
        break;
    case CallAddEventBody:
        scene_manager_set_scene_state(app->scene_manager, ApiCallerSceneCallAdd, CallAddStateBody);
        api_caller_scene_call_add_start_input(app, CallAddStateBody);
        consumed = true;
        break;
    case CallAddEventSave:
        api_caller_scene_call_add_do_save(app);
        consumed = true;
        break;
    case CallAddEventDelete:
        api_caller_scene_call_add_do_delete(app);
        consumed = true;
        break;
    default:
        break;
    }

    return consumed;
}

void api_caller_scene_call_add_on_exit(void* context) {
    furi_assert(context);
    AppContext* app = context;
    variable_item_list_reset(app->var_item_list_call);
    text_input_reset(app->text_input);
}
