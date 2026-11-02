#include "../../fn_test_app_i.h"
#include "fn_scene.h"

/**
 * Версию задаёт `fap_version` в `application.fam`: ufbt передаёт её
 * компилятору как строковый макрос `FAP_VERSION`. Запасное значение нужно
 * только для сборки вне ufbt (clangd без актуального `compile_commands.json`).
 */
#ifndef FAP_VERSION
#define FAP_VERSION "0.5"
#endif

void fn_scene_about_on_enter(void* context) {
    furi_assert(context);
    FNApp* app = context;

    fn_view_text_clear(app->view_text);
    fn_view_text_add_section(app->view_text, FN_STR_ABOUT_TITLE);
    fn_view_text_add_line(app->view_text, FnTextStyleNormal, "%s", FN_STR_ABOUT_APP);
    fn_view_text_add_line(
        app->view_text, FnTextStyleNormal, FN_STR_ABOUT_VERSION_FMT, FAP_VERSION);
    fn_view_text_add_line(app->view_text, FnTextStyleNormal, "%s", FN_STR_ABOUT_AUTHOR);
    fn_view_text_add_line(app->view_text, FnTextStyleNormal, "%s", FN_STR_ABOUT_URL_1);
    fn_view_text_add_line(app->view_text, FnTextStyleNormal, "%s", FN_STR_ABOUT_URL_2);
    fn_view_text_add_section(app->view_text, NULL);
    fn_view_text_add_line(app->view_text, FnTextStyleNormal, "%s", FN_STR_ABOUT_DESCRIPTION_1);
    fn_view_text_add_line(app->view_text, FnTextStyleNormal, "%s", FN_STR_ABOUT_DESCRIPTION_2);
    fn_view_text_add_line(app->view_text, FnTextStyleNormal, "%s", FN_STR_ABOUT_DESCRIPTION_3);
    fn_view_text_add_line(app->view_text, FnTextStyleNormal, "%s", FN_STR_ABOUT_DESCRIPTION_4);
    fn_view_text_commit(app->view_text);

    view_dispatcher_switch_to_view(app->view_dispatcher, FNAppViewText);
}

bool fn_scene_about_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    FNApp* app = context;

    /* Back возвращает в меню с любой позиции прокрутки (FR-018). */
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, FNAppSceneStart);
        return true;
    }
    return false;
}

void fn_scene_about_on_exit(void* context) {
    furi_assert(context);
    UNUSED(context);
}
