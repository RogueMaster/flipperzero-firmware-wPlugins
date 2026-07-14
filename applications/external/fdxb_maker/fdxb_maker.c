#include "fdxb_maker.h"

bool fdxb_maker_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    FdxbMaker* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool fdxb_maker_back_event_callback(void* context) {
    furi_assert(context);
    FdxbMaker* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static FdxbMaker* fdxb_maker_alloc() {
    FdxbMaker* app = malloc(sizeof(FdxbMaker));

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&fdxb_maker_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, fdxb_maker_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, fdxb_maker_back_event_callback);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FdxbMakerViewSubmenu, submenu_get_view(app->submenu));

    app->popup = popup_alloc();
    view_dispatcher_add_view(app->view_dispatcher, FdxbMakerViewPopup, popup_get_view(app->popup));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FdxbMakerViewTextInput, text_input_get_view(app->text_input));

    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        FdxbMakerViewVarItemList,
        variable_item_list_get_view(app->variable_item_list));

    app->byte_input = byte_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FdxbMakerViewByteInput, byte_input_get_view(app->byte_input));

    app->number_input = number_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FdxbMakerViewNumberInput, number_input_get_view(app->number_input));

    app->big_number_input = big_number_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        FdxbMakerViewBigNumberInput,
        big_number_input_get_view(app->big_number_input));

    app->fdxb_temperature_input = fdxb_temperature_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        FdxbMakerViewFdxbTemperatureInput,
        fdxb_temperature_input_get_view(app->fdxb_temperature_input));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FdxbMakerViewWidget, widget_get_view(app->widget));

    app->file_name = furi_string_alloc();
    app->file_path = furi_string_alloc_set(LFRFID_APP_FOLDER);

    fdxb_maker_reset_data(app);

    app->needs_restore = false;

    return app;
}

static void fdxb_maker_free(FdxbMaker* app) {
    furi_assert(app);

    furi_string_free(app->file_name);
    furi_string_free(app->file_path);

    free(app->data);
    free(app->bytes);
    protocol_dict_free(app->dict);
    protocol_dict_free(app->tmp_dict);

    view_dispatcher_remove_view(app->view_dispatcher, FdxbMakerViewSubmenu);
    submenu_free(app->submenu);

    view_dispatcher_remove_view(app->view_dispatcher, FdxbMakerViewPopup);
    popup_free(app->popup);

    view_dispatcher_remove_view(app->view_dispatcher, FdxbMakerViewTextInput);
    text_input_free(app->text_input);

    view_dispatcher_remove_view(app->view_dispatcher, FdxbMakerViewVarItemList);
    variable_item_list_free(app->variable_item_list);

    view_dispatcher_remove_view(app->view_dispatcher, FdxbMakerViewByteInput);
    byte_input_free(app->byte_input);

    view_dispatcher_remove_view(app->view_dispatcher, FdxbMakerViewNumberInput);
    number_input_free(app->number_input);

    view_dispatcher_remove_view(app->view_dispatcher, FdxbMakerViewBigNumberInput);
    big_number_input_free(app->big_number_input);

    view_dispatcher_remove_view(app->view_dispatcher, FdxbMakerViewFdxbTemperatureInput);
    fdxb_temperature_input_free(app->fdxb_temperature_input);

    view_dispatcher_remove_view(app->view_dispatcher, FdxbMakerViewWidget);
    widget_free(app->widget);

    view_dispatcher_free(app->view_dispatcher);

    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_DIALOGS);

    free(app);
}

int32_t fdxb_maker(void* p) {
    UNUSED(p);
    FdxbMaker* app = fdxb_maker_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    scene_manager_next_scene(app->scene_manager, FdxbMakerSceneStart);

    view_dispatcher_run(app->view_dispatcher);

    fdxb_maker_free(app);

    return 0;
}

void fdxb_maker_construct_data(FdxbMaker* app) {
    FdxbExpanded* data = app->data;
    uint8_t* bytes = app->bytes;

    // 0-37: National
    uint64_t national = data->national_code << (64 - 38);
    bit_lib_reverse_bits((uint8_t*)&national, 0, 64);
    bit_lib_set_bits(bytes, 0, national >> (38 - 8), 8);
    bit_lib_set_bits(bytes, 8, national >> (38 - 16), 8);
    bit_lib_set_bits(bytes, 16, national >> (38 - 24), 8);
    bit_lib_set_bits(bytes, 24, national >> (38 - 32), 8);
    bit_lib_set_bits(bytes, 32, national, 6);

    // 38-47: Country
    uint16_t country_rev = bit_lib_reverse_16_fast(data->country_code << (16 - 10));
    bit_lib_set_bits(bytes, 38, country_rev >> (10 - 8), 8);
    bit_lib_set_bits(bytes, 46, country_rev, 2);

    // 48: Data Block presence
    bool data_block = data->trailer_mode != FdxbTrailerModeOff;
    bit_lib_set_bit(bytes, 48, data_block);

    // 49: RUDI bit (advanced transponder)
    bit_lib_set_bit(bytes, 49, data->RUDI_bit);

    // 50-52: Visual number starting digit
    uint8_t visual_digit_rev = bit_lib_reverse_8_fast(data->visual_start_digit << (8 - 3));
    bit_lib_set_bits(bytes, 50, visual_digit_rev, 3);

    // 53-54: Reserved for future use (0)
    uint8_t rfu_rev = bit_lib_reverse_8_fast(data->RFU << (8 - 2));
    bit_lib_set_bits(bytes, 53, rfu_rev, 2);

    // 55-59: User info
    uint8_t user_rev = bit_lib_reverse_8_fast(data->user_info << (8 - 5));
    bit_lib_set_bits(bytes, 55, user_rev, 5);

    // 60-62: Retagging
    uint8_t retag_rev = bit_lib_reverse_8_fast(data->retagging_count << (8 - 3));
    bit_lib_set_bits(bytes, 60, retag_rev, 3);

    // 63: Animal
    bit_lib_set_bit(bytes, 63, data->animal);

    // 64-87: Trailer (application data)
    if(data->trailer_mode == FdxbTrailerModeTemperature) {
        // https://forum.dangerousthings.com/t/6799/3
        uint8_t temperature = (uint8_t)round((data->temperature - 74) / 0.2);
        uint8_t temp_rev = bit_lib_reverse_8_fast(temperature);

        bool parity = bit_lib_test_parity_32(temperature, BitLibParityOdd);

        bit_lib_set_bits(bytes, 64, temp_rev, 8);
        bit_lib_set_bit(bytes, 72, parity);
        bit_lib_set_bits(bytes, 73, 0, 7);
        bit_lib_set_bits(bytes, 80, 0, 8);
    } else {
        uint32_t trailer_rev = data->trailer << (32 - 24);
        bit_lib_reverse_bits((uint8_t*)&trailer_rev, 0, 32);

        bit_lib_set_bits(bytes, 64, trailer_rev >> 16, 8);
        bit_lib_set_bits(bytes, 72, trailer_rev >> 8, 8);
        bit_lib_set_bits(bytes, 80, trailer_rev, 8);
    }

    protocol_dict_set_data(app->dict, LFRFIDProtocolFDXB, bytes, FDXB_DECODED_DATA_SIZE);
}

void fdxb_maker_deconstruct_data(FdxbMaker* app) {
    FdxbExpanded* data = app->data;
    uint8_t* bytes = app->bytes;

    protocol_dict_get_data(app->dict, LFRFIDProtocolFDXB, bytes, FDXB_DECODED_DATA_SIZE);

    // National
    uint64_t national_code = bit_lib_get_bits_32(bytes, 0, 32);
    national_code = national_code << 32;
    national_code |= (uint64_t)bit_lib_get_bits_32(bytes, 32, 6) << (32 - 6);
    bit_lib_reverse_bits((uint8_t*)&national_code, 0, 64);

    data->national_code = national_code;

    // Country
    uint16_t country_code = bit_lib_reverse_16_fast(bit_lib_get_bits_16(bytes, 38, 10) << 6);
    data->country_code = country_code;

    // Data block
    bool block_status = bit_lib_get_bit(bytes, 48);

    bool calc_parity = bit_lib_test_parity_32(bit_lib_get_bits(bytes, 64, 8), BitLibParityOdd);
    bool parity = bit_lib_get_bit(bytes, 72);

    uint16_t extra_trailer = bit_lib_get_bits_16(bytes, 73, 15);

    bool temperature_present = (calc_parity == parity) && !extra_trailer;

    if(block_status) {
        if(temperature_present) {
            data->trailer_mode = FdxbTrailerModeTemperature;
        } else {
            data->trailer_mode = FdxbTrailerModeAppData;
        }
    } else {
        data->trailer_mode = FdxbTrailerModeOff;
    }

    // RUDI
    data->RUDI_bit = bit_lib_get_bit(bytes, 49);

    // Visual tag digit
    data->visual_start_digit = bit_lib_reverse_8_fast(bit_lib_get_bits(bytes, 50, 3) << (8 - 3));

    // RFU
    data->RFU = bit_lib_reverse_8_fast(bit_lib_get_bits(bytes, 53, 2) << (8 - 2));

    // User info
    data->user_info = bit_lib_reverse_8_fast(bit_lib_get_bits(bytes, 55, 5) << (8 - 5));

    // Retagging
    data->retagging_count = bit_lib_reverse_8_fast(bit_lib_get_bits(bytes, 60, 3) << (8 - 3));

    // Animal
    data->animal = bit_lib_get_bit(bytes, 63);

    // Trailer
    uint32_t trailer = bit_lib_get_bits_32(bytes, 64, 24) << 8;
    bit_lib_reverse_bits((uint8_t*)&trailer, 0, 32);

    if(temperature_present) {
        uint8_t temp_byte = bit_lib_reverse_8_fast(bit_lib_get_bits(bytes, 64, 8));
        data->temperature = temp_byte * 0.2 + 74;
    }

    data->trailer = trailer;
}

void fdxb_maker_reset_data(FdxbMaker* app) {
    app->file_name = furi_string_alloc();
    app->file_path = furi_string_alloc_set(LFRFID_APP_FOLDER);

    app->data = calloc(1, sizeof(FdxbExpanded));
    app->data->animal = true;

    app->bytes = malloc(FDXB_DECODED_DATA_SIZE);
    app->dict = protocol_dict_alloc(lfrfid_protocols, LFRFIDProtocolMax);
    app->tmp_dict = protocol_dict_alloc(lfrfid_protocols, LFRFIDProtocolMax);
}

bool fdxb_maker_save_key(FdxbMaker* app) {
    furi_assert(app);
    bool result = false;

    fdxb_maker_construct_data(app);

    fdxb_maker_make_app_folder(app);

    if(furi_string_end_with(app->file_path, LFRFID_APP_FILENAME_EXTENSION)) {
        size_t filename_start = furi_string_search_rchar(app->file_path, '/');
        furi_string_left(app->file_path, filename_start);
    }

    furi_string_cat_printf(
        app->file_path,
        "/%s%s",
        furi_string_get_cstr(app->file_name),
        LFRFID_APP_FILENAME_EXTENSION);

    result = fdxb_maker_save_key_data(app, app->file_path);

    return result;
}

bool fdxb_maker_load_key_from_file_select(FdxbMaker* app) {
    furi_assert(app);

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(
        &browser_options, LFRFID_APP_FILENAME_EXTENSION, &I_125_10px);
    browser_options.base_path = LFRFID_APP_FOLDER;

    // Input events and views are managed by file_browser
    bool result =
        dialog_file_browser_show(app->dialogs, app->file_path, app->file_path, &browser_options);

    if(result) {
        result = fdxb_maker_load_key_data(app, app->file_path, true);
    }

    return result;
}

bool fdxb_maker_delete_key(FdxbMaker* app) {
    furi_assert(app);

    return storage_simply_remove(app->storage, furi_string_get_cstr(app->file_path));
}

bool fdxb_maker_load_key_data(FdxbMaker* app, FuriString* path, bool show_dialog) {
    bool success = false;
    ProtocolId protocol_id;

    protocol_id = lfrfid_dict_file_load(app->tmp_dict, furi_string_get_cstr(path));
    if(protocol_id == PROTOCOL_NO) {
        if(show_dialog) dialog_message_show_storage_error(app->dialogs, "Cannot load\nkey file");
    } else if(protocol_id != LFRFIDProtocolFDXB) {
        if(show_dialog)
            dialog_message_show_storage_error(app->dialogs, "File is not in\nFDX-B format");
    } else {
        path_extract_filename(path, app->file_name, true);
        if(lfrfid_dict_file_load(app->dict, furi_string_get_cstr(path)) == LFRFIDProtocolFDXB) {
            success = true;
            fdxb_maker_deconstruct_data(app);
        }
    }

    return success;
}

bool fdxb_maker_save_key_data(FdxbMaker* app, FuriString* path) {
    bool result = lfrfid_dict_file_save(app->dict, LFRFIDProtocolFDXB, furi_string_get_cstr(path));

    if(!result) {
        dialog_message_show_storage_error(app->dialogs, "Cannot save\nkey file");
    }

    return result;
}

bool fdxb_maker_save_key_and_switch_scenes(FdxbMaker* app) {
    fdxb_maker_construct_data(app);

    if(scene_manager_get_scene_state(app->scene_manager, FdxbMakerSceneStart) ==
       FdxbMakerMenuIndexNew) {
        scene_manager_next_scene(app->scene_manager, FdxbMakerSceneSaveName);
        return true;
    } else {
        if(!furi_string_empty(app->file_name)) {
            fdxb_maker_delete_key(app);
        }

        if(fdxb_maker_save_key(app)) {
            scene_manager_next_scene(app->scene_manager, FdxbMakerSceneSaveSuccess);
            return true;
        } else {
            return scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, FdxbMakerSceneSelectKey);
        }
    }
}

void fdxb_maker_make_app_folder(FdxbMaker* app) {
    furi_assert(app);

    if(!storage_simply_mkdir(app->storage, LFRFID_APP_FOLDER)) {
        dialog_message_show_storage_error(app->dialogs, "Cannot create\nRFID app folder");
    }
}

void fdxb_maker_text_store_set(FdxbMaker* app, const char* text, ...) {
    furi_assert(app);
    va_list args;
    va_start(args, text);

    vsnprintf(app->text_store, FDXB_MAKER_TEXT_STORE_SIZE, text, args);

    va_end(args);
}

void fdxb_maker_text_store_clear(FdxbMaker* app) {
    furi_assert(app);
    memset(app->text_store, 0, sizeof(app->text_store));
}

void fdxb_maker_widget_callback(GuiButtonType result, InputType type, void* context) {
    FdxbMaker* app = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, result);
    }
}

void fdxb_maker_popup_timeout_callback(void* context) {
    FdxbMaker* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FdxbMakerEventPopupClosed);
}

void fdxb_maker_text_input_callback(void* context) {
    FdxbMaker* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FdxbMakerEventNext);
}
