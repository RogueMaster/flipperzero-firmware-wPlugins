#include "../marauder_gui_app_i.h"

/* Built at runtime from wifi_spam.c's type table plus one extra static entry, into a `static`
   (not stack-local) array so the pointer marauder_gui_menu_set_items() is given stays valid for
   as long as this menu stays on screen - a local array here would dangle after on_enter returns
   since the menu view redraws later, independent of this function's stack frame. */
#define WIFI_SPAM_MENU_MAX_ITEMS 8
static MarauderMenuItem marauder_wifi_spam_menu_items[WIFI_SPAM_MENU_MAX_ITEMS];

void marauder_gui_scene_wifi_spam_menu_on_enter(void* context) {
    MarauderGuiApp* app = context;

    size_t count = marauder_gui_wifi_spam_type_count();
    if(count > WIFI_SPAM_MENU_MAX_ITEMS - 1) count = WIFI_SPAM_MENU_MAX_ITEMS - 1;

    for(size_t i = 0; i < count; i++) {
        marauder_wifi_spam_menu_items[i].label_tr = marauder_gui_wifi_spam_type_label(i);
        marauder_wifi_spam_menu_items[i].label_en = marauder_gui_wifi_spam_type_label_en(i);
        marauder_wifi_spam_menu_items[i].description_tr = marauder_gui_wifi_spam_type_description(i);
        marauder_wifi_spam_menu_items[i].description_en =
            marauder_gui_wifi_spam_type_description_en(i);
    }
    marauder_wifi_spam_menu_items[count].label_tr = "Sayi Belirle (N Sahte AG)";
    marauder_wifi_spam_menu_items[count].label_en = "Set Count (N Fake AP)";
    marauder_wifi_spam_menu_items[count].description_tr =
        "Girdigin sayida (1-50) rastgele isimli, SABIT sayida sahte ag olusturup yayinlar.";
    marauder_wifi_spam_menu_items[count].description_en =
        "Creates and broadcasts a FIXED number (1-50) of randomly-named fake networks.";

    marauder_gui_menu_set_items(app, marauder_wifi_spam_menu_items, count + 1, "WiFi Spam");
}

bool marauder_gui_scene_wifi_spam_menu_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        size_t table_count = marauder_gui_wifi_spam_type_count();
        if(event.event < table_count) {
            app->wifi_spam_type_index = event.event;
            app->wifi_spam_use_count = false;
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiSpam);
            consumed = true;
        } else if(event.event == table_count) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiSpamCount);
            consumed = true;
        }
    }

    return consumed;
}

void marauder_gui_scene_wifi_spam_menu_on_exit(void* context) {
    MarauderGuiApp* app = context;
    app->tick_handler = NULL;
}
