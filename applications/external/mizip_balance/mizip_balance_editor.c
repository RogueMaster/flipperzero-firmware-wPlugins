#include <mizip_balance_editor_i.h>

// This function is called when there are custom events to process.
bool mizip_balance_editor_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    MiZipBalanceEditorApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

// This function is called when the user has pressed the Back key.
bool mizip_balance_editor_app_back_event_callback(void* context) {
    furi_assert(context);
    MiZipBalanceEditorApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

bool mizip_balance_editor_write_new_balance(void* context) {
    MiZipBalanceEditorApp* app = context;

    //Updating previous balance
    app->mf_classic_data->block[app->previous_credit_pointer].data[2] =
        (app->current_balance >> 8) & 0xFF; // High byte
    app->mf_classic_data->block[app->previous_credit_pointer].data[1] = app->current_balance &
                                                                        0xFF; // Low byte
    app->mf_classic_data->block[app->previous_credit_pointer].data[3] =
        app->mf_classic_data->block[app->previous_credit_pointer].data[1] ^
        app->mf_classic_data->block[app->previous_credit_pointer].data[2]; //Checksum

    //Updating current balance
    app->mf_classic_data->block[app->credit_pointer].data[2] = (app->new_balance >> 8) &
                                                               0xFF; // High byte
    app->mf_classic_data->block[app->credit_pointer].data[1] = app->new_balance & 0xFF; // Low byte
    app->mf_classic_data->block[app->credit_pointer].data[3] =
        app->mf_classic_data->block[app->credit_pointer].data[1] ^
        app->mf_classic_data->block[app->credit_pointer].data[2]; //Checksum

    bool write_success = false;

    if(app->currentDataSource == DataSourceFile) {
        write_success = mizip_balance_editor_write_new_balance_to_file(context);
    } else if(app->currentDataSource == DataSourceNfc) {
        write_success = mizip_balance_editor_write_new_balance_to_tag(context);
    }

    return write_success;
}

bool mizip_balance_editor_write_new_balance_to_file(void* context) {
    MiZipBalanceEditorApp* app = context;
    nfc_device_set_data(app->nfc_device, NfcProtocolMfClassic, app->mf_classic_data);
    bool write_success =
        nfc_device_save(app->nfc_device, furi_string_get_cstr(app->shadowFilePath));
    if(write_success) {
        FURI_LOG_D(
            "MiZipBalanceEditor",
            "Data successfully writen to %s",
            furi_string_get_cstr(app->shadowFilePath));
    } else {
        FURI_LOG_D(
            "MiZipBalanceEditor",
            "Failed to write to %s",
            furi_string_get_cstr(app->shadowFilePath));
    }
    return write_success;
}

bool mizip_balance_editor_write_new_balance_to_tag(void* context) {
    MiZipBalanceEditorApp* app = context;
    bool write_success = false;

    // Generates keys for writing
    uint8_t keyA[MIZIP_SECTOR_COUNT][MIZIP_KEY_LENGTH];
    uint8_t keyB[MIZIP_SECTOR_COUNT][MIZIP_KEY_LENGTH];

    // Set Key B sector 0 (standard)
    const uint8_t keyB_0[] = MIZIP_KEYB_0_BYTES;
    memcpy(keyB[0], keyB_0, MIZIP_KEY_LENGTH);

    // Generate the other keys from the UID
    mizip_generate_key(app->uid, keyA, keyB);

    // Configure keys for writing
    MfClassicDeviceKeys keys = {};
    for(size_t i = 0; i < 5; i++) {
        bit_lib_num_to_bytes_be(
            bit_lib_bytes_to_num_be(keyB[i], MIZIP_KEY_LENGTH),
            sizeof(MfClassicKey),
            keys.key_b[i].data);
        FURI_BIT_SET(keys.key_b_mask, i);
    }

    // Write the modified blocks on tag
    MfClassicKey key_b_prev, key_b_curr;

    // Determine which sector for each block
    uint8_t sector_prev =
        app->previous_credit_pointer / 4; // Block 8 = sector 2, block 9 = sector 2
    uint8_t sector_curr = app->credit_pointer / 4;

    memcpy(key_b_prev.data, keyB[sector_prev], MIZIP_KEY_LENGTH);
    memcpy(key_b_curr.data, keyB[sector_curr], MIZIP_KEY_LENGTH);

    MfClassicError error_prev = mf_classic_poller_sync_write_block(
        app->nfc,
        app->previous_credit_pointer,
        &key_b_prev,
        MfClassicKeyTypeB,
        &app->mf_classic_data->block[app->previous_credit_pointer]);

    MfClassicError error_curr = mf_classic_poller_sync_write_block(
        app->nfc,
        app->credit_pointer,
        &key_b_curr,
        MfClassicKeyTypeB,
        &app->mf_classic_data->block[app->credit_pointer]);

    write_success = (error_prev == MfClassicErrorNone && error_curr == MfClassicErrorNone);

    FURI_LOG_D(
        "MiZipBalanceEditor", "Physical write result: %s", write_success ? "SUCCESS" : "FAILED");

    return write_success;
}

// Application constructor function.
static MiZipBalanceEditorApp* mizip_balance_editor_app_alloc() {
    MiZipBalanceEditorApp* app = malloc(sizeof(MiZipBalanceEditorApp));
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->storage = furi_record_open(RECORD_STORAGE);

    // Create the ViewDispatcher and SceneManager instance
    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&mizip_balance_editor_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, mizip_balance_editor_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, mizip_balance_editor_app_back_event_callback);

    //Create the SubMenu for main menu
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MiZipBalanceEditorViewIdMainMenu, submenu_get_view(app->submenu));

    //Initialize Dialogs for file browser
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    //Initialize Popup for NFC scanner and Write Success
    app->popup = popup_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MiZipBalanceEditorViewIdScanner, popup_get_view(app->popup));
    view_dispatcher_add_view(
        app->view_dispatcher, MiZipBalanceEditorViewIdReader, popup_get_view(app->popup));
    view_dispatcher_add_view(
        app->view_dispatcher, MiZipBalanceEditorViewIdWriteSuccess, popup_get_view(app->popup));
    app->nfc = nfc_alloc();
    app->nfc_device = nfc_device_alloc();

    //Create the DialogEx for editing view
    app->dialog_ex = dialog_ex_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        MiZipBalanceEditorViewIdShowBalance,
        dialog_ex_get_view(app->dialog_ex));

    //Create the NumberInput for custom value balance
    app->number_input = number_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        MiZipBalanceEditorViewIdNumberInput,
        number_input_get_view(app->number_input));

    //Create the TextBox for about scene
    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MiZipBalanceEditorViewIdAbout, text_box_get_view(app->text_box));

    //Initiate data for MfClassic data store
    app->mf_classic_data = mf_classic_alloc();
    app->filePath = furi_string_alloc();
    app->shadowFilePath = furi_string_alloc();
    app->is_valid_mizip_data = false;

    app->credit_pointer = 0x09;
    app->previous_credit_pointer = 0x08;
    app->current_balance = 0;
    app->previous_balance = 0;

    app->last_selected_submenu_index = SubmenuIndexDirectToTag;

    return app;
}

// Application destructor function.
static void mizip_balance_editor_app_free(MiZipBalanceEditorApp* app) {
    // All views must be un-registered (removed) from a ViewDispatcher instance
    // before deleting it. Failure to do so will result in a crash.
    view_dispatcher_remove_view(app->view_dispatcher, MiZipBalanceEditorViewIdMainMenu);
    submenu_free(app->submenu);

    view_dispatcher_remove_view(app->view_dispatcher, MiZipBalanceEditorViewIdScanner);
    view_dispatcher_remove_view(app->view_dispatcher, MiZipBalanceEditorViewIdReader);
    view_dispatcher_remove_view(app->view_dispatcher, MiZipBalanceEditorViewIdWriteSuccess);
    popup_free(app->popup);

    view_dispatcher_remove_view(app->view_dispatcher, MiZipBalanceEditorViewIdShowBalance);
    dialog_ex_free(app->dialog_ex);

    view_dispatcher_remove_view(app->view_dispatcher, MiZipBalanceEditorViewIdNumberInput);
    number_input_free(app->number_input);

    view_dispatcher_remove_view(app->view_dispatcher, MiZipBalanceEditorViewIdAbout);
    text_box_free(app->text_box);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_GUI);
    app->gui = NULL;

    furi_record_close(RECORD_NOTIFICATION);
    app->notifications = NULL;

    furi_record_close(RECORD_STORAGE);
    app->storage = NULL;

    furi_record_close(RECORD_DIALOGS);
    app->dialogs = NULL;

    nfc_free(app->nfc);
    nfc_device_free(app->nfc_device);
    mf_classic_free(app->mf_classic_data);
    furi_string_free(app->filePath);
    furi_string_free(app->shadowFilePath);

    free(app);
}

int32_t mizip_balance_editor_app(void* p) {
    UNUSED(p);

    MiZipBalanceEditorApp* app = mizip_balance_editor_app_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, MiZipBalanceEditorSceneMainMenu);
    view_dispatcher_run(app->view_dispatcher);
    mizip_balance_editor_app_free(app);

    return 0;
}
