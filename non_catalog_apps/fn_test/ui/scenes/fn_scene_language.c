#include "../../fn_test_app_i.h"
#include "fn_scene.h"

#include "../../fn_settings.h"

/** Выбор языка: только custom event, без прямого управления сценами. */
static void fn_scene_language_callback(void* context, FnLang lang, bool confirmed) {
    furi_assert(context);
    FNApp* app = context;

    app->pending_lang = lang;
    view_dispatcher_send_custom_event(
        app->view_dispatcher,
        confirmed ? FNAppCustomEventLanguageSelected : FNAppCustomEventLanguageCancelled);
}

void fn_scene_language_on_enter(void* context) {
    furi_assert(context);
    FNApp* app = context;

    fn_view_language_set(app->view_language, app->lang, app->lang);
    fn_view_language_set_callback(app->view_language, fn_scene_language_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, FNAppViewLanguage);
}

bool fn_scene_language_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    FNApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == FNAppCustomEventLanguageSelected) {
            app->lang = app->pending_lang;
            app->lang_selected = true;
            fn_settings_save_lang(app->lang);
            fn_view_menu_set_language(app->view_menu, app->lang);
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, FNAppSceneStart);
            return true;
        }
        if(event.event == FNAppCustomEventLanguageCancelled) {
            /* Первый запуск: без выбора остаётся язык по умолчанию (English). */
            if(!app->lang_selected) {
                app->lang = FN_LANG_DEFAULT;
                app->lang_selected = true;
                fn_settings_save_lang(app->lang);
                fn_view_menu_set_language(app->view_menu, app->lang);
            }
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, FNAppSceneStart);
            return true;
        }
    }

    if(event.type == SceneManagerEventTypeBack) {
        view_dispatcher_send_custom_event(app->view_dispatcher, FNAppCustomEventLanguageCancelled);
        return true;
    }

    return false;
}

void fn_scene_language_on_exit(void* context) {
    furi_assert(context);
    UNUSED(context);
}
