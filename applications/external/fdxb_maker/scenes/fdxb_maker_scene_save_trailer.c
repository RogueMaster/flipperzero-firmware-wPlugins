#include "../fdxb_maker.h"

void fdxb_maker_scene_save_trailer_byte_input_callback(void* context) {
    FdxbMaker* app = context;
    FdxbExpanded* data = app->data;

    data->trailer = app->byte_store[0] << 16 | app->byte_store[1] << 8 | app->byte_store[2];

    view_dispatcher_send_custom_event(app->view_dispatcher, FdxbMakerEventNext);
}

void fdxb_maker_scene_save_trailer_on_enter(void* context) {
    FdxbMaker* app = context;
    FdxbExpanded* data = app->data;
    ByteInput* byte_input = app->byte_input;

    app->byte_store[0] = data->trailer >> 16;
    app->byte_store[1] = data->trailer >> 8;
    app->byte_store[2] = data->trailer;

    byte_input_set_header_text(byte_input, "Enter the application data");

    byte_input_set_result_callback(
        byte_input,
        fdxb_maker_scene_save_trailer_byte_input_callback,
        NULL,
        app,
        (uint8_t*)&app->byte_store,
        3);

    view_dispatcher_switch_to_view(app->view_dispatcher, FdxbMakerViewByteInput);
}

bool fdxb_maker_scene_save_trailer_on_event(void* context, SceneManagerEvent event) {
    FdxbMaker* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == FdxbMakerEventNext) {
            consumed = true;
            fdxb_maker_save_key_and_switch_scenes(app);
        }
    }

    return consumed;
}

void fdxb_maker_scene_save_trailer_on_exit(void* context) {
    UNUSED(context);
}
