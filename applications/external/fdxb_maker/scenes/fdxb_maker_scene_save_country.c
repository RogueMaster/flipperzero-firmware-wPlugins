#include "../fdxb_maker.h"

void fdxb_maker_scene_save_country_input_number_callback(void* context, uint64_t number) {
    FdxbMaker* app = context;
    FdxbExpanded* data = app->data;

    data->country_code = number;

    if(number > 999) {
        view_dispatcher_send_custom_event(app->view_dispatcher, FdxbMakerEventNonstandardValue);
    } else {
        view_dispatcher_send_custom_event(app->view_dispatcher, FdxbMakerEventNext);
    }
}

void fdxb_maker_scene_save_country_on_enter(void* context) {
    FdxbMaker* app = context;
    FdxbExpanded* data = app->data;
    BigNumberInput* big_number_input = app->big_number_input;

    app->needs_restore = true;

    big_number_input_set_header_text(big_number_input, "Enter the country code");

    big_number_input_set_result_callback(
        big_number_input,
        fdxb_maker_scene_save_country_input_number_callback,
        app,
        data->country_code,
        0,
        1023); // 2^10-1

    view_dispatcher_switch_to_view(app->view_dispatcher, FdxbMakerViewBigNumberInput);
}

bool fdxb_maker_scene_save_country_on_event(void* context, SceneManagerEvent event) {
    FdxbMaker* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == FdxbMakerEventNext) {
            consumed = true;

            scene_manager_next_scene(app->scene_manager, FdxbMakerSceneSaveNational);
        } else if(event.event == FdxbMakerEventNonstandardValue) {
            consumed = true;
            scene_manager_next_scene(app->scene_manager, FdxbMakerSceneConfirmNonstandardCountry);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        app->needs_restore = false;
    }

    return consumed;
}

void fdxb_maker_scene_save_country_on_exit(void* context) {
    UNUSED(context);
}
