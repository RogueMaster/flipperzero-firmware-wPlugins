#include "../picopass_i.h"
#include <dolphin/dolphin.h>
#include <picopass_keys.h>

#define TAG "PicopassSceneDeviceInfo"

void picopass_scene_device_info_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    furi_assert(context);
    Picopass* picopass = context;

    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(picopass->view_dispatcher, result);
    }
}

void picopass_scene_device_info_on_enter(void* context) {
    Picopass* picopass = context;

    FuriString* csn_str = furi_string_alloc_set("CSN:");
    FuriString* credential_str = furi_string_alloc();
    FuriString* info_str = furi_string_alloc();
    FuriString* key_str = furi_string_alloc();

    dolphin_deed(DolphinDeedNfcReadSuccess);

    // Setup view
    PicopassBlock* card_data = picopass->dev->dev_data.card_data;
    PicopassPacs* pacs = &picopass->dev->dev_data.pacs;
    Widget* widget = picopass->widget;

    uint8_t csn[PICOPASS_BLOCK_LEN] = {0};
    memcpy(csn, card_data[PICOPASS_CSN_BLOCK_INDEX].data, PICOPASS_BLOCK_LEN);
    for(uint8_t i = 0; i < PICOPASS_BLOCK_LEN; i++) {
        furi_string_cat_printf(csn_str, "%02X", csn[i]);
    }
    bool sio = 0x30 == card_data[PICOPASS_ICLASS_PACS_CFG_BLOCK_INDEX].data[0];

    if(sio) {
        furi_string_cat_printf(info_str, "SIO");
    } else if(pacs->bitLength == 0 || pacs->bitLength == 255) {
        // Neither of these are valid.  Indicates the block was all 0x00 or all 0xff
        furi_string_cat_printf(info_str, "Invalid PACS");
    } else {
        size_t bytesLength = 1 + pacs->bitLength / 8;
        furi_string_set(credential_str, "");
        furi_string_cat_printf(credential_str, "(%d) ", pacs->bitLength);
        for(uint8_t i = PICOPASS_BLOCK_LEN - bytesLength; i < PICOPASS_BLOCK_LEN; i++) {
            furi_string_cat_printf(credential_str, "%02X", pacs->credential[i]);
        }

        if(pacs->sio) {
            furi_string_cat_printf(info_str, " +SIO");
        }
    }

    uint8_t crypt = card_data[PICOPASS_CONFIG_BLOCK_INDEX].data[7] & PICOPASS_FUSE_CRYPT10;
    bool unsecure = crypt == PICOPASS_FUSE_CRYPT0;
    if(unsecure) {
        furi_string_cat_printf(key_str, "Unsecure card");
    } else if(card_data[PICOPASS_SECURE_KD_BLOCK_INDEX].valid) {
        uint8_t key[PICOPASS_BLOCK_LEN] = {};
        loclass_iclass_calc_div_key(
            card_data[PICOPASS_CSN_BLOCK_INDEX].data, picopass_iclass_key, key, false);
        bool standard =
            memcmp(card_data[PICOPASS_SECURE_KD_BLOCK_INDEX].data, key, PICOPASS_BLOCK_LEN) == 0;
        if(standard) {
            furi_string_cat_printf(key_str, "Key: Standard");
        } else {
            furi_string_cat_printf(key_str, "Key: Not Standard");
        }
    } else {
        furi_string_cat_printf(key_str, "No Key: used NR-MAC");
    }

    widget_add_button_element(
        widget, GuiButtonTypeLeft, "Back", picopass_scene_device_info_widget_callback, picopass);

    wiegand_message_t wiegand_msg = picopass_pacs_extract_wmo(pacs);
    size_t format_count = picopass_wiegand_format_count(&wiegand_msg);
    if(format_count > 0) {
        widget_add_button_element(
            widget,
            GuiButtonTypeCenter,
            "Parse",
            picopass_scene_device_info_widget_callback,
            picopass);
    }

    widget_add_button_element(
        widget, GuiButtonTypeRight, "Raw", picopass_scene_device_info_widget_callback, picopass);

    widget_add_string_element(
        widget, 64, 5, AlignCenter, AlignCenter, FontSecondary, furi_string_get_cstr(csn_str));
    widget_add_string_element(
        widget,
        64,
        20,
        AlignCenter,
        AlignCenter,
        FontPrimary,
        furi_string_get_cstr(credential_str));
    widget_add_string_element(
        widget, 64, 36, AlignCenter, AlignCenter, FontSecondary, furi_string_get_cstr(info_str));
    widget_add_string_element(
        widget, 64, 46, AlignCenter, AlignCenter, FontSecondary, furi_string_get_cstr(key_str));

    furi_string_free(csn_str);
    furi_string_free(credential_str);
    furi_string_free(info_str);
    furi_string_free(key_str);

    view_dispatcher_switch_to_view(picopass->view_dispatcher, PicopassViewWidget);
}

bool picopass_scene_device_info_on_event(void* context, SceneManagerEvent event) {
    Picopass* picopass = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            consumed = scene_manager_previous_scene(picopass->scene_manager);
        } else if(event.event == GuiButtonTypeRight) {
            scene_manager_next_scene(picopass->scene_manager, PicopassSceneMoreInfo);
            consumed = true;
        } else if(event.event == GuiButtonTypeCenter) {
            scene_manager_next_scene(picopass->scene_manager, PicopassSceneFormats);
            consumed = true;
        } else if(event.event == PicopassCustomEventViewExit) {
            view_dispatcher_switch_to_view(picopass->view_dispatcher, PicopassViewWidget);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_previous_scene(picopass->scene_manager);
    }
    return consumed;
}

void picopass_scene_device_info_on_exit(void* context) {
    Picopass* picopass = context;

    // Clear view
    widget_reset(picopass->widget);
}
