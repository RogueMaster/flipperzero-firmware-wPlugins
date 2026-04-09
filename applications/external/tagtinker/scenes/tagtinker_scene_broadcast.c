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

static void broadcast_enter_callback(void* context, uint32_t index) {
    UNUSED(index);
    TagTinkerApp* app = context;
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
            if(durations[i] == app->duration) { dur_idx = i; break; }
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
        if(repeat_values[i] == app->repeats) { rep_idx = i; break; }
    }
    variable_item_set_current_value_index(item, rep_idx);
    variable_item_set_current_value_text(item, repeat_labels[rep_idx]);

    /* Continuous repeat toggle */
    item = variable_item_list_add(vil, "Repeat", 2, spam_changed, app);
    variable_item_set_current_value_index(item, app->tx_spam ? 1 : 0);
    variable_item_set_current_value_text(item, forever_labels[app->tx_spam ? 1 : 0]);

    /* OK press on any item → trigger transmit */
    variable_item_list_set_enter_callback(vil, broadcast_enter_callback, app);

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
