#include "../marauder_gui_app_i.h"

/* Built at runtime from bt_spam.c's type table into a `static` (not stack-local) array so the
   pointer given to marauder_gui_menu_set_items() stays valid for as long as this menu is on
   screen - see wifi_spam_menu.c for why a local array here would be unsafe. */
#define BT_SPAM_MENU_MAX_ITEMS 8
static MarauderMenuItem marauder_bt_spam_menu_items[BT_SPAM_MENU_MAX_ITEMS];

void marauder_gui_scene_bt_spam_menu_on_enter(void* context) {
    MarauderGuiApp* app = context;

    size_t count = marauder_gui_bt_spam_type_count();
    if(count > BT_SPAM_MENU_MAX_ITEMS) count = BT_SPAM_MENU_MAX_ITEMS;

    for(size_t i = 0; i < count; i++) {
        marauder_bt_spam_menu_items[i].label_tr = marauder_gui_bt_spam_type_label(i);
        marauder_bt_spam_menu_items[i].label_en = marauder_gui_bt_spam_type_label_en(i);
        marauder_bt_spam_menu_items[i].description_tr = marauder_gui_bt_spam_type_description(i);
        marauder_bt_spam_menu_items[i].description_en =
            marauder_gui_bt_spam_type_description_en(i);
    }

    marauder_gui_menu_set_items(app, marauder_bt_spam_menu_items, count, "Bluetooth Spam");
}

bool marauder_gui_scene_bt_spam_menu_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event < marauder_gui_bt_spam_type_count()) {
            app->bt_spam_type_index = event.event;
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneBtSpam);
            consumed = true;
        }
    }

    return consumed;
}

void marauder_gui_scene_bt_spam_menu_on_exit(void* context) {
    MarauderGuiApp* app = context;
    app->tick_handler = NULL;
}
