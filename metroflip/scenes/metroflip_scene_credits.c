#include "../metroflip_i.h"
#include <dolphin/dolphin.h>
#include "../api/metroflip/metroflip_api.h"

#define TAG "Metroflip:Scene:Credits"

void metroflip_scene_credits_on_enter(void* context) {
    Metroflip* app = context;
    Widget* widget = app->widget;

    dolphin_deed(DolphinDeedNfcReadSuccess);
    furi_string_reset(app->text_box_store);

    FuriString* str = furi_string_alloc();

    furi_string_printf(str, "\e#Credits:\n\n");
    furi_string_cat_printf(str, "Created by luu176\n");
    furi_string_cat_printf(str, "Inspired by Metrodroid\n\n");
    furi_string_cat_printf(str, "Special Thanks:\nWillyJL\n\n");
    furi_string_cat_printf(str, "Info Slaves:\nequipter, TheDingo8MyBaby\nry4000, WillyJL\n\n");
    furi_string_cat_printf(str, "\e#Parser Credits:\n\n");
    furi_string_cat_printf(str, "Bip! Parser:\nrbasoalto, gornekich\n\n");
    furi_string_cat_printf(str, "CharlieCard Parser:\nzacharyweiss\n\n");
    furi_string_cat_printf(str, "Clipper Parser:\nke6jjj\n\n");
    furi_string_cat_printf(str, "ITSO Parser:\ngsp8181, hedger, gornekich\n\n");
    furi_string_cat_printf(str, "Intertic Parser:\nluu176");
    furi_string_cat_printf(str, "Metromoney Parser:\nLeptopt1los\n\n");
    furi_string_cat_printf(str, "myki Parser:\ngornekich\n\n");
    furi_string_cat_printf(str, "Navigo Parser:\nluu176, DocSystem \n\n");
    furi_string_cat_printf(str, "Opal Parser:\ngornekich\n\n");
    furi_string_cat_printf(str, "Opus Parser:\nDocSystem\n\n");
    furi_string_cat_printf(str, "Rav-Kav Parser:\nluu176\n\n");
    furi_string_cat_printf(str, "RENFE Parser:\nBocamoCM\n\n");
    furi_string_cat_printf(str, "Japan Transit IC Parser:\nzinongli\n\n");
    furi_string_cat_printf(str, "Troika Parser:\ngornekich\n\n");
    furi_string_cat_printf(str, "TRT Parser:\nluu176, zinongli");
    furi_string_cat_printf(str, "Octopus Parser:\nzinongli\n\n");
    furi_string_cat_printf(str, "nol Parser:\nzinongli\n\n");

    widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(str));

    widget_add_button_element(
        widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);

    furi_string_free(str);
    view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
}

bool metroflip_scene_credits_on_event(void* context, SceneManagerEvent event) {
    Metroflip* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            consumed = scene_manager_previous_scene(app->scene_manager);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MetroflipSceneStart);
        consumed = true;
    }
    return consumed;
}

void metroflip_scene_credits_on_exit(void* context) {
    Metroflip* app = context;
    widget_reset(app->widget);
    UNUSED(context);
}
