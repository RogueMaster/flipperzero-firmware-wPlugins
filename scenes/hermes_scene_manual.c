#include "../hermes_i.h"

/* For when you already know the answer, or want to sit on a rate Hermes ruled
 * out. Everything here is a deliberate override of the detector. */

typedef enum {
    ManualItemBaud,
    ManualItemFraming,
    ManualItemOpen,
} ManualItem;

static uint8_t manual_baud_index = 11; // 115200, the sensible place to start
static uint8_t manual_framing_index = HermesFraming8N1;

static void hermes_scene_manual_baud_changed(VariableItem* item) {
    HermesApp* app = variable_item_get_context(item);
    manual_baud_index = variable_item_get_current_value_index(item);

    char text[12];
    snprintf(text, sizeof(text), "%lu", hermes_baud_table[manual_baud_index].baud);
    variable_item_set_current_value_text(item, text);
    UNUSED(app);
}

static void hermes_scene_manual_framing_changed(VariableItem* item) {
    manual_framing_index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, hermes_framing_name(manual_framing_index));
}

static void hermes_scene_manual_enter_callback(void* context, uint32_t index) {
    HermesApp* app = context;
    if(index == ManualItemOpen) {
        view_dispatcher_send_custom_event(app->view_dispatcher, HermesCustomEventResultPicked);
    }
}

void hermes_scene_manual_on_enter(void* context) {
    HermesApp* app = context;
    VariableItemList* list = app->var_item_list;
    VariableItem* item;
    char text[12];

    variable_item_list_reset(list);

    item = variable_item_list_add(
        list, "Baud", HERMES_BAUD_COUNT, hermes_scene_manual_baud_changed, app);
    variable_item_set_current_value_index(item, manual_baud_index);
    snprintf(text, sizeof(text), "%lu", hermes_baud_table[manual_baud_index].baud);
    variable_item_set_current_value_text(item, text);

    item = variable_item_list_add(
        list, "Framing", HermesFramingCount, hermes_scene_manual_framing_changed, app);
    variable_item_set_current_value_index(item, manual_framing_index);
    variable_item_set_current_value_text(item, hermes_framing_name(manual_framing_index));

    variable_item_list_add(list, "Open console", 0, NULL, app);

    variable_item_list_set_enter_callback(list, hermes_scene_manual_enter_callback, app);
    variable_item_list_set_selected_item(list, ManualItemOpen);

    view_dispatcher_switch_to_view(app->view_dispatcher, HermesViewSettings);
}

bool hermes_scene_manual_on_event(void* context, SceneManagerEvent event) {
    HermesApp* app = context;

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == HermesCustomEventResultPicked) {
        app->link.baud = hermes_baud_table[manual_baud_index].baud;
        app->link.framing = manual_framing_index;
        app->link.verified = false;
        scene_manager_next_scene(app->scene_manager, HermesSceneConsole);
        return true;
    }

    return false;
}

void hermes_scene_manual_on_exit(void* context) {
    HermesApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
