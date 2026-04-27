/*
 * WiFi Run
 * ========
 *
 * 1. Renders a VariableItemList of the selected plugin's parameters.
 *    - Enum  : cycle through options.
 *    - Bool  : Off/On toggle.
 *    - Int   : numeric range.
 *    - String: tap to open text_input, write back into wifi_param_values.
 * 2. Adds a "Generate" item at the bottom that:
 *    - Picks a target (defaults to the currently-selected target;
 *      if none, asks the user via the popup).
 *    - Sends RUN_PLUGIN to the ESP.
 *    - Switches to a Popup view that streams progress updates.
 *    - On RESULT_END, writes a BMP and chains to the existing transmit
 *      scene via tagtinker_prepare_bmp_tx().
 *    - On ERROR, shows the message in the popup.
 */
#include "../tagtinker_app.h"
#include "../wifi/tagtinker_wifi.h"
#include "../wifi/tagtinker_wifi_bmp.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define EVT_GENERATE     0xD0u
#define EVT_PARAM_STRING 0xD1u
#define EVT_TEXT_DONE    0xD2u
#define EVT_PROGRESS     0xD3u
#define EVT_ERROR        0xD4u
#define EVT_RESULT_DONE  0xD5u

/* Per-scene state held in the app to avoid statics. */
static TagTinkerWifiBmpWriter s_bmp_writer;
static int8_t s_string_param_being_edited = -1;

static TagTinkerWifiPlugin* current_plugin(TagTinkerApp* app) {
    if(app->wifi_selected_plugin < 0) return NULL;
    TagTinkerWifiPlugin* arr = (TagTinkerWifiPlugin*)app->wifi_plugins;
    return &arr[app->wifi_selected_plugin];
}

/* ---- Variable-item callbacks --------------------------------------------*/

/* Because VariableItem doesn't directly expose row index in its callback,
 * we encode the param index in the high byte of the variable item's
 * `current_value_index` when a callback fires - we re-pack it elsewhere.
 * Simpler: we maintain a parallel array of the items we created, in order,
 * and use variable_item_set_current_value_text to update the displayed text.
 * The scenes module provides no easier way; this approach is minimal. */
static VariableItem* s_param_items[6];

static void item_changed_enum(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    TagTinkerWifiPlugin* p = current_plugin(app);
    if(!p) return;
    /* Locate the param index for this item by matching pointer in s_param_items. */
    for(uint8_t i = 0; i < p->param_count; i++) {
        if(s_param_items[i] != item) continue;
        const TtWifiParam* sp = &p->params[i];
        uint8_t idx = variable_item_get_current_value_index(item);
        if(idx >= sp->option_count) idx = 0;
        const char* opt = sp->options[idx];
        variable_item_set_current_value_text(item, opt);
        strncpy(app->wifi_param_values[i], opt, sizeof(app->wifi_param_values[i]) - 1);
        app->wifi_param_values[i][sizeof(app->wifi_param_values[i]) - 1] = 0;
        break;
    }
}

static void item_changed_bool(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    TagTinkerWifiPlugin* p = current_plugin(app);
    if(!p) return;
    for(uint8_t i = 0; i < p->param_count; i++) {
        if(s_param_items[i] != item) continue;
        uint8_t idx = variable_item_get_current_value_index(item);
        variable_item_set_current_value_text(item, idx ? "On" : "Off");
        strcpy(app->wifi_param_values[i], idx ? "1" : "0");
        break;
    }
}

static void item_changed_int(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    TagTinkerWifiPlugin* p = current_plugin(app);
    if(!p) return;
    for(uint8_t i = 0; i < p->param_count; i++) {
        if(s_param_items[i] != item) continue;
        const TtWifiParam* sp = &p->params[i];
        int32_t v = sp->int_min + variable_item_get_current_value_index(item);
        char buf[16];
        snprintf(buf, sizeof(buf), "%ld", (long)v);
        variable_item_set_current_value_text(item, buf);
        strncpy(app->wifi_param_values[i], buf, sizeof(app->wifi_param_values[i]) - 1);
        break;
    }
}

static void item_enter_cb(void* ctx, uint32_t index) {
    TagTinkerApp* app = ctx;
    TagTinkerWifiPlugin* p = current_plugin(app);
    if(!p) return;

    /* The very last item is "Generate"; any string-param item opens the
     * text_input view. */
    if(index < p->param_count) {
        const TtWifiParam* sp = &p->params[index];
        if(sp->type != TT_PARAM_STRING) return;
        s_string_param_being_edited = (int8_t)index;
        view_dispatcher_send_custom_event(app->view_dispatcher, EVT_PARAM_STRING);
    } else {
        view_dispatcher_send_custom_event(app->view_dispatcher, EVT_GENERATE);
    }
}

/* ---- Build the param list ---------------------------------------------- */

static void seed_param_value(TagTinkerApp* app, const TtWifiParam* sp, uint8_t i) {
    /* If we already have a value (e.g. text_input edit), keep it. The
     * scene's on_enter wipes the slots fresh per plugin to prevent the
     * shared array leaking values across plugin selections. */
    if(app->wifi_param_values[i][0] != 0) return;
    strncpy(app->wifi_param_values[i], sp->default_value, sizeof(app->wifi_param_values[i]) - 1);
    app->wifi_param_values[i][sizeof(app->wifi_param_values[i]) - 1] = 0;
}

static void build_param_list(TagTinkerApp* app) {
    VariableItemList* list = app->var_item_list;
    variable_item_list_reset(list);
    memset(s_param_items, 0, sizeof(s_param_items));

    TagTinkerWifiPlugin* p = current_plugin(app);
    if(!p) return;

    for(uint8_t i = 0; i < p->param_count; i++) {
        const TtWifiParam* sp = &p->params[i];
        seed_param_value(app, sp, i);
        VariableItem* it = NULL;

        if(sp->type == TT_PARAM_ENUM) {
            it = variable_item_list_add(list, sp->label, sp->option_count, item_changed_enum, app);
            /* Default-select the option matching the seeded value. */
            uint8_t sel = 0;
            for(uint8_t j = 0; j < sp->option_count; j++) {
                if(strcmp(app->wifi_param_values[i], sp->options[j]) == 0) {
                    sel = j;
                    break;
                }
            }
            variable_item_set_current_value_index(it, sel);
            variable_item_set_current_value_text(it, sp->options[sel]);
        } else if(sp->type == TT_PARAM_BOOL) {
            it = variable_item_list_add(list, sp->label, 2, item_changed_bool, app);
            uint8_t sel = (app->wifi_param_values[i][0] == '1') ? 1 : 0;
            variable_item_set_current_value_index(it, sel);
            variable_item_set_current_value_text(it, sel ? "On" : "Off");
        } else if(sp->type == TT_PARAM_INT) {
            int32_t range = sp->int_max - sp->int_min + 1;
            if(range <= 0 || range > 100) range = 1;
            it = variable_item_list_add(list, sp->label, (uint8_t)range, item_changed_int, app);
            int32_t cur = atoi(app->wifi_param_values[i]);
            if(cur < sp->int_min) cur = sp->int_min;
            uint8_t idx = (uint8_t)(cur - sp->int_min);
            variable_item_set_current_value_index(it, idx);
            char buf[16];
            snprintf(buf, sizeof(buf), "%ld", (long)cur);
            variable_item_set_current_value_text(it, buf);
        } else {
            /* String: clickable, opens text_input. */
            it = variable_item_list_add(list, sp->label, 1, NULL, app);
            variable_item_set_current_value_text(
                it, app->wifi_param_values[i][0] ? app->wifi_param_values[i] : "(set)");
        }
        s_param_items[i] = it;
    }
    variable_item_list_add(list, ">> Generate <<", 0, NULL, app);
    variable_item_list_set_enter_callback(list, item_enter_cb, app);
}

/* ---- Text input for STRING params -------------------------------------- */
static char s_text_buf[64];

static void text_done_cb(void* ctx) {
    TagTinkerApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, EVT_TEXT_DONE);
}

static void open_text_input_for_param(TagTinkerApp* app, uint8_t i) {
    text_input_reset(app->text_input);
    TagTinkerWifiPlugin* p = current_plugin(app);
    text_input_set_header_text(app->text_input, p->params[i].label);
    strncpy(s_text_buf, app->wifi_param_values[i], sizeof(s_text_buf) - 1);
    s_text_buf[sizeof(s_text_buf) - 1] = 0;
    text_input_set_result_callback(
        app->text_input, text_done_cb, app, s_text_buf, sizeof(s_text_buf), false);
    view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewTextInput);
}

/* ---- Run + result handling --------------------------------------------- */

/* The wifi_plugins scene installed its own callback; we hot-swap it on
 * scene enter and restore on exit. */
static TtWifiEventCb s_prev_cb;
static void* s_prev_user;

static void run_event_cb(const TtWifiEvent* e, void* user) {
    TagTinkerApp* app = user;
    switch(e->type) {
    case TtWifiEvtProgress:
        app->wifi_progress_pct = (uint8_t)e->u0;
        strncpy(
            app->wifi_progress_msg, e->str0 ? e->str0 : "", sizeof(app->wifi_progress_msg) - 1);
        view_dispatcher_send_custom_event(app->view_dispatcher, EVT_PROGRESS);
        break;
    case TtWifiEvtResultBegin: {
        uint16_t w = (uint16_t)(e->u0 & 0xFFFFu);
        uint16_t h = (uint16_t)(e->u0 >> 16);
        uint8_t pl = (uint8_t)(e->u1 ? e->u1 : 1);
        /* Pick a palette accent that matches the destination tag's colour
         * so the BMP file embeds the right BGR for previewers. The IR TX
         * path itself only cares about plane bits + the target profile. */
        uint8_t ar = 0xE0, ag = 0x10, ab = 0x10; /* default red */
        if(app->selected_target >= 0 && app->selected_target < app->target_count) {
            const TagTinkerTarget* t = &app->targets[app->selected_target];
            if(t->profile.color == TagTinkerTagColorYellow) {
                ar = 0xF0;
                ag = 0xC0;
                ab = 0x10;
            }
        }
        if(!tagtinker_wifi_bmp_open(&s_bmp_writer, w, h, pl, ar, ag, ab)) {
            strncpy(app->wifi_last_error, "BMP open failed", sizeof(app->wifi_last_error) - 1);
            view_dispatcher_send_custom_event(app->view_dispatcher, EVT_ERROR);
        }
        break;
    }
    case TtWifiEvtResultChunk:
        tagtinker_wifi_bmp_chunk(&s_bmp_writer, e->data, e->data_len);
        break;
    case TtWifiEvtResultEnd:
        if(!tagtinker_wifi_bmp_close(&s_bmp_writer)) {
            strncpy(app->wifi_last_error, "BMP write failed", sizeof(app->wifi_last_error) - 1);
            view_dispatcher_send_custom_event(app->view_dispatcher, EVT_ERROR);
        } else {
            view_dispatcher_send_custom_event(app->view_dispatcher, EVT_RESULT_DONE);
        }
        break;
    case TtWifiEvtError:
        strncpy(
            app->wifi_last_error,
            e->str0 ? e->str0 : "Unknown error",
            sizeof(app->wifi_last_error) - 1);
        view_dispatcher_send_custom_event(app->view_dispatcher, EVT_ERROR);
        break;
    case TtWifiEvtLinkLost:
        strncpy(app->wifi_last_error, "Dev board went silent", sizeof(app->wifi_last_error) - 1);
        view_dispatcher_send_custom_event(app->view_dispatcher, EVT_ERROR);
        break;
    default:
        /* Hello/status/plugin events still useful: forward to the previous
         * callback so the plugin-list scene can refresh on return. */
        if(s_prev_cb) s_prev_cb(e, s_prev_user);
        break;
    }
}

static void show_progress_popup(TagTinkerApp* app) {
    popup_reset(app->popup);
    popup_set_header(app->popup, "WiFi Plugin", 64, 6, AlignCenter, AlignTop);
    char body[120];
    snprintf(body, sizeof(body), "%u%%\n%s", app->wifi_progress_pct, app->wifi_progress_msg);
    popup_set_text(app->popup, body, 64, 32, AlignCenter, AlignCenter);
    view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewPopup);
}

static void show_error_popup(TagTinkerApp* app) {
    popup_reset(app->popup);
    popup_set_header(app->popup, "Error", 64, 6, AlignCenter, AlignTop);
    popup_set_text(app->popup, app->wifi_last_error, 64, 32, AlignCenter, AlignCenter);
    view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewPopup);
}

static void start_run(TagTinkerApp* app) {
    TagTinkerWifiPlugin* p = current_plugin(app);
    if(!p) return;
    /* Need a target to pick the canvas size. Default to selected_target;
     * else fallback to a reasonable sane size. */
    uint16_t tw = app->esl_width ? app->esl_width : 296;
    uint16_t th = app->esl_height ? app->esl_height : 128;
    /* Honour the tag's accent capability: red/yellow profiles get the
     * accent plane, mono profiles stay mono. The BMP writer + the IR TX
     * pipeline already understand 2-plane BMPs (same convention as the
     * web image prep tool), so plugins can use the accent freely. */
    uint8_t accent = TT_ACCENT_NONE;
    if(app->selected_target >= 0 && app->selected_target < app->target_count) {
        const TagTinkerTarget* t = &app->targets[app->selected_target];
        if(tagtinker_target_supports_accent(t)) {
            accent = (t->profile.color == TagTinkerTagColorYellow) ? TT_ACCENT_YELLOW :
                                                                     TT_ACCENT_RED;
        }
    }

    TtWifiKV kv[6];
    uint8_t n = 0;
    for(uint8_t i = 0; i < p->param_count && i < 6; i++) {
        kv[n].key = p->params[i].key;
        kv[n].value = app->wifi_param_values[i];
        n++;
    }

    app->wifi_progress_pct = 0;
    snprintf(app->wifi_progress_msg, sizeof(app->wifi_progress_msg), "Starting...");
    app->wifi_last_error[0] = 0;
    app->wifi_run_in_flight = true;

    show_progress_popup(app);

    tagtinker_wifi_run_plugin((TagTinkerWifi*)app->wifi, p->index, tw, th, accent, kv, n);
}

/* ---- Scene entry / event ---------------------------------------------- */

void tagtinker_scene_wifi_run_on_enter(void* ctx) {
    TagTinkerApp* app = ctx;
    s_string_param_being_edited = -1;
    /* Reset the shared param-value array so the new plugin starts fresh
     * with its own defaults (otherwise e.g. Crypto's "BTC" leaks into
     * Weather's "Location" slot). */
    memset(app->wifi_param_values, 0, sizeof(app->wifi_param_values));

    /* Hot-swap the WiFi callback so progress/result frames land here.
     * The previous callback (the plugins scene's) is restored on exit. */
    if(app->wifi) {
        tagtinker_wifi_set_callback(
            (TagTinkerWifi*)app->wifi, run_event_cb, app, &s_prev_cb, &s_prev_user);
    }

    build_param_list(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewVarItemList);
}

bool tagtinker_scene_wifi_run_on_event(void* ctx, SceneManagerEvent event) {
    TagTinkerApp* app = ctx;
    if(event.type != SceneManagerEventTypeCustom) return false;

    switch(event.event) {
    case EVT_PARAM_STRING:
        if(s_string_param_being_edited >= 0)
            open_text_input_for_param(app, (uint8_t)s_string_param_being_edited);
        return true;
    case EVT_TEXT_DONE: {
        if(s_string_param_being_edited >= 0) {
            uint8_t i = (uint8_t)s_string_param_being_edited;
            strncpy(app->wifi_param_values[i], s_text_buf, sizeof(app->wifi_param_values[i]) - 1);
            app->wifi_param_values[i][sizeof(app->wifi_param_values[i]) - 1] = 0;
        }
        s_string_param_being_edited = -1;
        build_param_list(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewVarItemList);
        return true;
    }
    case EVT_GENERATE:
        start_run(app);
        return true;
    case EVT_PROGRESS:
        if(app->wifi_run_in_flight) show_progress_popup(app);
        return true;
    case EVT_ERROR:
        app->wifi_run_in_flight = false;
        tagtinker_wifi_bmp_abort(&s_bmp_writer);
        show_error_popup(app);
        return true;
    case EVT_RESULT_DONE: {
        app->wifi_run_in_flight = false;
        /* Hand the BMP to the existing TX path. */
        if(app->selected_target < 0 || app->selected_target >= app->target_count) {
            strncpy(
                app->wifi_last_error,
                "No saved tags - scan one in Targeted Payloads first",
                sizeof(app->wifi_last_error) - 1);
            show_error_popup(app);
            return true;
        }
        const TagTinkerTarget* t = &app->targets[app->selected_target];
        tagtinker_prepare_bmp_tx(
            app, t->plid, TAGTINKER_WIFI_TMP_BMP, app->esl_width, app->esl_height, app->img_page);
        scene_manager_next_scene(app->scene_manager, TagTinkerSceneTransmit);
        return true;
    }
    }
    return false;
}

void tagtinker_scene_wifi_run_on_exit(void* ctx) {
    TagTinkerApp* app = ctx;
    /* Restore the plugins-scene callback. */
    if(app->wifi && s_prev_cb) {
        tagtinker_wifi_set_callback((TagTinkerWifi*)app->wifi, s_prev_cb, s_prev_user, NULL, NULL);
        s_prev_cb = NULL;
        s_prev_user = NULL;
    }
    /* Release the ~10 KB pixel buffer if a transfer was abandoned mid-flight
     * (e.g. the user backs out of the popup before RESULT_END). Without this
     * the buffer leaks on every run and the IR transmit scene that follows
     * has noticeably less heap to malloc its plane buffers - the OOM crashes
     * we were seeing. abort() is a no-op if the writer is already closed. */
    tagtinker_wifi_bmp_abort(&s_bmp_writer);
    variable_item_list_reset(app->var_item_list);
    popup_reset(app->popup);
    text_input_reset(app->text_input);
}
