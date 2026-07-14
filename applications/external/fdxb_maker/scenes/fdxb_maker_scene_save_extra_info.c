#include "../fdxb_maker.h"

typedef enum {
    FdxbMakerSaveExtraInfoAnimalBit = 0,
    FdxbMakerSaveExtraInfoRetaggingCount,
    FdxbMakerSaveExtraInfoSetUserInfo,
    FdxbMakerSaveExtraInfoVisualDigit,
    FdxbMakerSaveExtraInfoRUDIBit,
    FdxbMakerSaveExtraInfoTrailerMode,
    FdxbMakerSaveExtraInfoNext
} FdxbMakerSaveExtraInfoEntry;

const char* const fdxb_maker_bool_text[2] = {
    "No",
    "Yes",
};

#define FDXB_3BIT_COUNT 8
static const char* const fdxb_maker_3bit_text[FDXB_3BIT_COUNT] =
    {"0", "1", "2", "3", "4", "5", "6", "7"};

#define FDXB_TRAILER_MODE_COUNT 3
const char* const fdxb_maker_trailer_text[3] = {"Off", "Custom", "Temp"};

static void
    fdxb_maker_scene_save_extra_info_var_list_enter_callback(void* context, uint32_t index) {
    FdxbMaker* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void fdxb_maker_scene_save_extra_info_animal_bit_changed(VariableItem* item) {
    FdxbMaker* app = variable_item_get_context(item);
    FdxbExpanded* data = app->data;
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, fdxb_maker_bool_text[index]);
    data->animal = index;
}

static void fdxb_maker_scene_save_extra_info_retagging_changed(VariableItem* item) {
    FdxbMaker* app = variable_item_get_context(item);
    FdxbExpanded* data = app->data;
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, fdxb_maker_3bit_text[index]);
    data->retagging_count = index;
}

static void fdxb_maker_scene_save_extra_info_visual_digit_changed(VariableItem* item) {
    FdxbMaker* app = variable_item_get_context(item);
    FdxbExpanded* data = app->data;
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, fdxb_maker_3bit_text[index]);
    data->visual_start_digit = index;
}

static void fdxb_maker_scene_save_extra_info_rudi_bit_changed(VariableItem* item) {
    FdxbMaker* app = variable_item_get_context(item);
    FdxbExpanded* data = app->data;
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, fdxb_maker_bool_text[index]);
    data->RUDI_bit = index;
}

static void fdxb_maker_scene_save_extra_info_trailer_mode_changed(VariableItem* item) {
    FdxbMaker* app = variable_item_get_context(item);
    FdxbExpanded* data = app->data;
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, fdxb_maker_trailer_text[index]);
    data->trailer_mode = index;
}

void fdxb_maker_scene_save_extra_info_on_enter(void* context) {
    FdxbMaker* app = context;
    FdxbExpanded* data = app->data;
    VariableItemList* variable_item_list = app->variable_item_list;

    VariableItem* item;
    uint8_t value_index;

    item = variable_item_list_add(
        variable_item_list,
        "Animal bit",
        2,
        fdxb_maker_scene_save_extra_info_animal_bit_changed,
        app);

    value_index = data->animal;
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, fdxb_maker_bool_text[value_index]);

    item = variable_item_list_add(
        variable_item_list,
        "Retagging",
        FDXB_3BIT_COUNT,
        fdxb_maker_scene_save_extra_info_retagging_changed,
        app);

    value_index = data->retagging_count;
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, fdxb_maker_3bit_text[value_index]);

    variable_item_list_add(variable_item_list, "Set user info", 1, NULL, NULL);

    item = variable_item_list_add(
        variable_item_list,
        "Visual start digit",
        FDXB_3BIT_COUNT,
        fdxb_maker_scene_save_extra_info_visual_digit_changed,
        app);

    value_index = data->visual_start_digit;
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, fdxb_maker_3bit_text[value_index]);

    item = variable_item_list_add(
        variable_item_list, "RUDI bit", 2, fdxb_maker_scene_save_extra_info_rudi_bit_changed, app);

    value_index = data->RUDI_bit;
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, fdxb_maker_bool_text[value_index]);

    item = variable_item_list_add(
        variable_item_list,
        "Data block",
        FDXB_TRAILER_MODE_COUNT,
        fdxb_maker_scene_save_extra_info_trailer_mode_changed,
        app);

    value_index = data->trailer_mode;
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, fdxb_maker_trailer_text[value_index]);

    variable_item_list_add(variable_item_list, "Next", 1, NULL, NULL);

    variable_item_list_set_enter_callback(
        variable_item_list, fdxb_maker_scene_save_extra_info_var_list_enter_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, FdxbMakerViewVarItemList);
}

bool fdxb_maker_scene_save_extra_info_on_event(void* context, SceneManagerEvent event) {
    FdxbMaker* app = context;
    FdxbExpanded* data = app->data;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == FdxbMakerSaveExtraInfoSetUserInfo) {
            scene_manager_next_scene(app->scene_manager, FdxbMakerSceneSaveUserInfo);
        } else if(event.event == FdxbMakerSaveExtraInfoNext) {
            if(data->trailer_mode == FdxbTrailerModeAppData) {
                scene_manager_next_scene(app->scene_manager, FdxbMakerSceneSaveTrailer);
            } else if(data->trailer_mode == FdxbTrailerModeTemperature) {
                scene_manager_next_scene(app->scene_manager, FdxbMakerSceneSaveTemperature);
            } else {
                if(data->trailer) {
                    scene_manager_next_scene(app->scene_manager, FdxbMakerSceneAskWipeTrailer);
                } else {
                    fdxb_maker_save_key_and_switch_scenes(app);
                }
            }
        }
        consumed = true;
    }

    return consumed;
}

void fdxb_maker_scene_save_extra_info_on_exit(void* context) {
    FdxbMaker* app = context;
    variable_item_list_reset(app->variable_item_list);
}
