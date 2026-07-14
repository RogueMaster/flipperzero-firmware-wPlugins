#include "../fdxb_maker.h"

void fdxb_maker_scene_save_national_input_number_callback(void* context, uint64_t number) {
    FdxbMaker* app = context;
    FdxbExpanded* data = app->data;

    data->national_code = number;

    view_dispatcher_send_custom_event(app->view_dispatcher, FdxbMakerEventNext);
}

void fdxb_maker_scene_save_national_on_enter(void* context) {
    FdxbMaker* app = context;
    FdxbExpanded* data = app->data;
    BigNumberInput* big_number_input = app->big_number_input;

    big_number_input_set_header_text(big_number_input, "Enter the national code");

    big_number_input_set_result_callback(
        big_number_input,
        fdxb_maker_scene_save_national_input_number_callback,
        app,
        data->national_code,
        0,
        274877906943); // 2^38-1

    view_dispatcher_switch_to_view(app->view_dispatcher, FdxbMakerViewBigNumberInput);
}

bool fdxb_maker_scene_save_national_on_event(void* context, SceneManagerEvent event) {
    FdxbMaker* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == FdxbMakerEventNext) {
            consumed = true;
            scene_manager_next_scene(app->scene_manager, FdxbMakerSceneSaveExtraInfo);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = true;
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, FdxbMakerSceneSaveCountry);
    }

    return consumed;
}

void fdxb_maker_scene_save_national_on_exit(void* context) {
    UNUSED(context);
}
