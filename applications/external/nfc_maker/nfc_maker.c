#include "nfc_maker.h"

const CardDef cards[CardMAX] = {
    // MfUltralight
    [CardNtag203] =
        {
            .name = "NTAG 203 (144B)",
            .size = 0x12 * NTAG_DATA_AREA_UNIT_SIZE,
            .protocol = NfcProtocolMfUltralight,
            .generator = NfcDataGeneratorTypeNTAG203,
        },
    [CardNtag213] =
        {
            .name = "NTAG 213 (144B)",
            .size = 0x12 * NTAG_DATA_AREA_UNIT_SIZE,
            .protocol = NfcProtocolMfUltralight,
            .generator = NfcDataGeneratorTypeNTAG213,
        },
    [CardNtag215] =
        {
            .name = "NTAG 215 (496B)",
            .size = 0x3E * NTAG_DATA_AREA_UNIT_SIZE,
            .protocol = NfcProtocolMfUltralight,
            .generator = NfcDataGeneratorTypeNTAG215,
        },
    [CardNtag216] =
        {
            .name = "NTAG 216 (872B)",
            .size = 0x6D * NTAG_DATA_AREA_UNIT_SIZE,
            .protocol = NfcProtocolMfUltralight,
            .generator = NfcDataGeneratorTypeNTAG216,
        },
    [CardNtagI2C1K] =
        {
            .name = "NTAG I2C 1K (872B)",
            .size = 0x6D * NTAG_DATA_AREA_UNIT_SIZE,
            .protocol = NfcProtocolMfUltralight,
            .generator = NfcDataGeneratorTypeNTAGI2C1k,
        },
    [CardNtagI2C2K] =
        {
            .name = "NTAG I2C 2K (1872B)",
            .size = 0xEA * NTAG_DATA_AREA_UNIT_SIZE,
            .protocol = NfcProtocolMfUltralight,
            .generator = NfcDataGeneratorTypeNTAGI2C2k,
        },

    // Type4Tag (no TLV, so +3 for < 255b message or +5 for larger messages)
    [CardNtag413DNA] =
        {
            .name = "NTAG 413 DNA (126B)",
            .size = 126 + 3,
            .protocol = NfcProtocolType4Tag,
        },
    [CardNtag424DNA] =
        {
            .name = "NTAG 424 DNA (254B)",
            .size = 254 + 3,
            .protocol = NfcProtocolType4Tag,
        },
    [CardMfDesfire] =
        {
            .name = "MIFARE DESFire (2046B)",
            .size = TYPE_4_TAG_MF_DESFIRE_NDEF_SIZE + 5,
            .protocol = NfcProtocolType4Tag,
        },
    [CardType4Generic] =
        {
            .name = "Generic Type 4 Tag",
            .size = -1,
            .protocol = NfcProtocolType4Tag,
        },

    // MfClassic (size excludes sector trailers and MAD1/2 sectors)
    [CardMfClassicMini] =
        {
            .name = "MIFARE Classic Mini 0.3K",
            .size = (5 - 1) * (4 - 1) * MF_CLASSIC_BLOCK_SIZE,
            .protocol = NfcProtocolMfClassic,
            .generator = NfcDataGeneratorTypeMfClassicMini,
        },
    [CardMfClassic1K4b] =
        {
            .name = "MIFARE Classic 1K UID4",
            .size = (16 - 1) * (4 - 1) * MF_CLASSIC_BLOCK_SIZE,
            .protocol = NfcProtocolMfClassic,
            .generator = NfcDataGeneratorTypeMfClassic1k_4b,
        },
    [CardMfClassic1K7b] =
        {
            .name = "MIFARE Classic 1K UID7",
            .size = (16 - 1) * (4 - 1) * MF_CLASSIC_BLOCK_SIZE,
            .protocol = NfcProtocolMfClassic,
            .generator = NfcDataGeneratorTypeMfClassic1k_7b,
        },
    [CardMfClassic4K4b] =
        {
            .name = "MIFARE Classic 4K UID4",
            .size = (((32 - 2) * (4 - 1)) + ((8) * (16 - 1))) * MF_CLASSIC_BLOCK_SIZE,
            .protocol = NfcProtocolMfClassic,
            .generator = NfcDataGeneratorTypeMfClassic4k_4b,
        },
    [CardMfClassic4K7b] =
        {
            .name = "MIFARE Classic 4K UID7",
            .size = (((32 - 2) * (4 - 1)) + ((8) * (16 - 1))) * MF_CLASSIC_BLOCK_SIZE,
            .protocol = NfcProtocolMfClassic,
            .generator = NfcDataGeneratorTypeMfClassic4k_7b,
        },

    // Slix (size excludes first block which is Capability Container)
    [CardSlix] =
        {
            .name = "SLIX (108B)",
            .size = (28 - 1) * SLIX_BLOCK_SIZE,
            .protocol = NfcProtocolSlix,
        },
    [CardSlixS] =
        {
            .name = "SLIX-S (156B)",
            .size = (40 - 1) * SLIX_BLOCK_SIZE,
            .protocol = NfcProtocolSlix,
        },
    [CardSlixL] =
        {
            .name = "SLIX-L (28B)",
            .size = (8 - 1) * SLIX_BLOCK_SIZE,
            .protocol = NfcProtocolSlix,
        },
    [CardSlix2] =
        {
            .name = "SLIX2 (316B)",
            .size = (80 - 1) * SLIX_BLOCK_SIZE,
            .protocol = NfcProtocolSlix,
        },
};

static bool nfc_maker_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    NfcMaker* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool nfc_maker_back_event_callback(void* context) {
    furi_assert(context);
    NfcMaker* app = context;

    return scene_manager_handle_back_event(app->scene_manager);
}

NfcMaker* nfc_maker_alloc() {
    NfcMaker* app = malloc(sizeof(NfcMaker));
    app->gui = furi_record_open(RECORD_GUI);

    // View Dispatcher and Scene Manager
    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&nfc_maker_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, nfc_maker_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, nfc_maker_back_event_callback);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Gui Modules
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NfcMakerViewSubmenu, submenu_get_view(app->submenu));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NfcMakerViewTextInput, text_input_get_view(app->text_input));

    app->byte_input = byte_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NfcMakerViewByteInput, byte_input_get_view(app->byte_input));

    app->popup = popup_alloc();
    view_dispatcher_add_view(app->view_dispatcher, NfcMakerViewPopup, popup_get_view(app->popup));

    // Nfc Device
    app->nfc_device = nfc_device_alloc();

    return app;
}

void nfc_maker_free(NfcMaker* app) {
    furi_assert(app);

    // Nfc Device
    nfc_device_free(app->nfc_device);
    if(app->ndef_buffer) {
        free(app->ndef_buffer);
    }

    // Gui modules
    view_dispatcher_remove_view(app->view_dispatcher, NfcMakerViewSubmenu);
    submenu_free(app->submenu);
    view_dispatcher_remove_view(app->view_dispatcher, NfcMakerViewTextInput);
    text_input_free(app->text_input);
    view_dispatcher_remove_view(app->view_dispatcher, NfcMakerViewByteInput);
    byte_input_free(app->byte_input);
    view_dispatcher_remove_view(app->view_dispatcher, NfcMakerViewPopup);
    popup_free(app->popup);

    // View Dispatcher and Scene Manager
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    // Records
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t nfc_maker(void* p) {
    UNUSED(p);
    NfcMaker* app = nfc_maker_alloc();
    scene_manager_set_scene_state(app->scene_manager, NfcMakerSceneStart, NfcMakerSceneHttps);
    scene_manager_next_scene(app->scene_manager, NfcMakerSceneStart);
    view_dispatcher_run(app->view_dispatcher);
    nfc_maker_free(app);
    return 0;
}
