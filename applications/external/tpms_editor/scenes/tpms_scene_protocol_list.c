#include "../tpms_app_i.h"

typedef struct {
    const char* name;
    const char* mode;
} TPMSProtocolListItem;

static const TPMSProtocolListItem protocol_items[] = {
    {"01 Renault", "FSK RX/TX"},
    {"02 Pacific C210/Toyota", "FSK RX/TX"},
    {"03 Schrader", "OOK RX/TX"},
    {"04 Schrader EG53MA4", "OOK RX/TX"},
    {"05 VDO/PSA/FCA", "FSK RX/TX"},
    {"06 Ford", "FSK RX/TX"},
    {"07 BMW Gen2/3", "FSK RX/TX EXP"},
    {"08 Elantra/Honda", "FSK RX/TX EXP"},
    {"09 Hyundai VDO", "FSK RX/TX EXP"},
    {"10 Truck Solar", "FSK RX/TX EXP"},
    {"11 Renault 0435R", "FSK RX/TX EXP"},
    {"12 Honda TRW", "FSK RX/TX EXP"},
    {"13 Porsche", "FSK RX/TX EXP"},
    {"14 Kia", "FSK RX/TX EXP"},
};

void tpms_scene_protocol_list_on_enter(void* context) {
    TPMSApp* app = context;
    for(size_t i = 0U; i < COUNT_OF(protocol_items); i++) {
        VariableItem* item = variable_item_list_add(
            app->variable_item_list, protocol_items[i].name, 1U, NULL, app);
        variable_item_set_current_value_text(item, protocol_items[i].mode);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewVariableItemList);
}

bool tpms_scene_protocol_list_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void tpms_scene_protocol_list_on_exit(void* context) {
    TPMSApp* app = context;
    variable_item_list_set_selected_item(app->variable_item_list, 0U);
    variable_item_list_reset(app->variable_item_list);
}
