/**
 * Broadcast page scene
 *
 * Sends either a page-select command or the diagnostic page command
 * to all DM ESLs in range.
 * No barcode needed - uses PLID 0 (broadcast).
 * Settings: page (0-7), duration (2-120s), forever toggle.
 */

#include "../tagtinker_app.h"

enum {
    BroadcastItemPage,
    BroadcastItemDuration,
    BroadcastItemForever,
    BroadcastItemRepeats,
    BroadcastItemSpam,
};

static const char* page_labels[] = {"0", "1", "2", "3", "4", "5", "6", "7"};
static const char* forever_labels[] = {"Off", "On"};
static const char* repeat_labels[] = {"50", "100", "200", "400", "800"};
static const uint16_t repeat_values[] = {50, 100, 200, 400, 800};
#define REPEAT_COUNT 5

static void page_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, page_labels[idx]);
    app->page = idx;
}

static void duration_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    uint16_t durations[] = {2, 5, 10, 15, 30, 60, 120};
    const char* dur_labels[] = {"2s", "5s", "10s", "15s", "30s", "60s", "120s"};
    variable_item_set_current_value_text(item, dur_labels[idx]);
    app->duration = durations[idx];
}

static void forever_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, forever_labels[idx]);
    app->forever = (idx == 1);
}

static void spam_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, forever_labels[idx]);
    app->tx_spam = (idx == 1);
}

static void repeats_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, repeat_labels[idx]);
    app->repeats = repeat_values[idx];
}

/* Row index of the ">> Transmit <<" item. Set when the list is built
 * because the row count differs between flip-page (Page/Duration/Forever
 * + Repeats/Repeat = 5 rows above Transmit) and diagnostic (just
 * Repeats/Repeat = 2 rows above Transmit). */
static uint8_t s_transmit_row_index = 0;

static void broadcast_enter_callback(void* context, uint32_t index) {
    TagTinkerApp* app = context;
    /* VariableItemList fires this on OK for ANY row. We only want OK on
     * the explicit ">> Transmit <<" row to start the broadcast - pressing
     * OK on the Page / Duration / Forever / Repeats rows used to also
     * start a transmit, which made it impossible to actually open those
     * settings without sending stuff to every tag in the room. */
    if(index != s_transmit_row_index) return;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void tagtinker_scene_broadcast_on_enter(void* context) {
    TagTinkerApp* app = context;
    VariableItemList* vil = app->var_item_list;

    variable_item_list_reset(vil);

    VariableItem* item;

    /* Build list conditionally */
    if(app->broadcast_type == TagTinkerBroadcastFlipPage) {
        /* Page selector 0-7 */
        item = variable_item_list_add(vil, "Page", 8, page_changed, app);
        variable_item_set_current_value_index(item, app->page);
        variable_item_set_current_value_text(item, page_labels[app->page]);

        /* Duration */
        item = variable_item_list_add(vil, "Duration", 7, duration_changed, app);
        /* Find closest index */
        uint16_t durations[] = {2, 5, 10, 15, 30, 60, 120};
        uint8_t dur_idx = 3; /* default 15s */
        for(uint8_t i = 0; i < 7; i++) {
            if(durations[i] == app->duration) {
                dur_idx = i;
                break;
            }
        }
        const char* dur_labels[] = {"2s", "5s", "10s", "15s", "30s", "60s", "120s"};
        variable_item_set_current_value_index(item, dur_idx);
        variable_item_set_current_value_text(item, dur_labels[dur_idx]);

        /* Forever toggle */
        item = variable_item_list_add(vil, "Forever", 2, forever_changed, app);
        variable_item_set_current_value_index(item, app->forever ? 1 : 0);
        variable_item_set_current_value_text(item, forever_labels[app->forever ? 1 : 0]);
    }

    /* Repeats (for both) */
    item = variable_item_list_add(vil, "Repeats", REPEAT_COUNT, repeats_changed, app);
    uint8_t rep_idx = 2; /* default 200 */
    for(uint8_t i = 0; i < REPEAT_COUNT; i++) {
        if(repeat_values[i] == app->repeats) {
            rep_idx = i;
            break;
        }
    }
    variable_item_set_current_value_index(item, rep_idx);
    variable_item_set_current_value_text(item, repeat_labels[rep_idx]);

    /* Continuous repeat toggle */
    item = variable_item_list_add(vil, "Repeat", 2, spam_changed, app);
    variable_item_set_current_value_index(item, app->tx_spam ? 1 : 0);
    variable_item_set_current_value_text(item, forever_labels[app->tx_spam ? 1 : 0]);

    /* The Transmit row is whatever index comes after everything we've
     * added so far. Diagnostic mode skips Page/Duration/Forever, hence
     * the index isn't a constant. */
    s_transmit_row_index = (app->broadcast_type == TagTinkerBroadcastFlipPage) ? 5 : 2;
    variable_item_list_add(vil, ">> Transmit <<", 0, NULL, app);

    /* The list calls back on OK for ANY row; broadcast_enter_callback
     * filters by row index so only the Transmit row actually starts
     * the broadcast.
     *
     * Cursor is intentionally LEFT on row 0 (the first setting) and
     * NOT on Transmit. Reason: the OK key-press that picked us out of
     * the previous Submenu generates a key-release input event that
     * arrives just after this scene's VIL becomes active. If the
     * cursor were on the Transmit row at that moment the stale OK
     * would auto-fire the enter callback and the radio would start
     * blasting before the user touched anything. Starting on row 0
     * (which has a change_callback) means stale OKs are harmless. */
    variable_item_list_set_enter_callback(vil, broadcast_enter_callback, app);
    variable_item_list_set_selected_item(vil, 0);

    view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewVarItemList);
}

bool tagtinker_scene_broadcast_on_event(void* context, SceneManagerEvent event) {
    TagTinkerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(app->broadcast_type == TagTinkerBroadcastFlipPage) {
            /* Build page broadcast frame and transmit */
            app->frame_len = tagtinker_build_broadcast_page_frame(
                app->frame_buf, app->page, app->forever, app->duration);
        } else {
            /* Build debug broadcast frame */
            app->frame_len = tagtinker_build_broadcast_debug_frame(app->frame_buf);
        }

        /* Set up single-frame TX */
        app->frame_seq_count = 0; /* Use single frame mode */

        scene_manager_next_scene(app->scene_manager, TagTinkerSceneTransmit);
        consumed = true;
    }
    return consumed;
}

void tagtinker_scene_broadcast_on_exit(void* context) {
    TagTinkerApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
