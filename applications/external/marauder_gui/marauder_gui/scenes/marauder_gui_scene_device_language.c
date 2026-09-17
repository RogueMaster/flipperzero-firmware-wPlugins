#include "../marauder_gui_app_i.h"

enum {
    DeviceLanguageIndexTurkish,
    DeviceLanguageIndexEnglish,
};

/* The two option names are proper nouns for the language itself, not text to translate - "Turkce"
   reads the same regardless of which language is currently active. Only the description changes
   language, like everywhere else. */
static const MarauderMenuItem marauder_device_language_items[] = {
    {"Turkce", "Turkce", "Tum menuler, basliklar ve durum yazilari Turkce olur.", "All menus, titles and status text will be in Turkish."},
    {"English", "English", "Tum menuler, basliklar ve durum yazilari Ingilizce olur.", "All menus, titles and status text will be in English."},
};

void marauder_gui_scene_device_language_on_enter(void* context) {
    MarauderGuiApp* app = context;
    marauder_gui_menu_set_items(
        app,
        marauder_device_language_items,
        sizeof(marauder_device_language_items) / sizeof(marauder_device_language_items[0]),
        marauder_gui_text(app, "Dil", "Language"));
    app->menu_selected = (app->language == MarauderLanguageEnglish) ? DeviceLanguageIndexEnglish :
                                                                       DeviceLanguageIndexTurkish;
}

bool marauder_gui_scene_device_language_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DeviceLanguageIndexTurkish) {
            app->language = MarauderLanguageTurkish;
            marauder_gui_save_language(app);
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        } else if(event.event == DeviceLanguageIndexEnglish) {
            app->language = MarauderLanguageEnglish;
            marauder_gui_save_language(app);
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        }
    }

    return consumed;
}

void marauder_gui_scene_device_language_on_exit(void* context) {
    MarauderGuiApp* app = context;
    app->tick_handler = NULL;
}
