#include "../picopass_i.h"
#include <dolphin/dolphin.h>

void picopass_scene_nr_mac_saved_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    furi_assert(context);
    Picopass* picopass = context;

    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(picopass->view_dispatcher, result);
    }
}

static uint32_t picopass_scene_nr_mac_saved_count(Picopass* picopass) {
    PicopassDevice* dev = picopass->dev;
    const uint8_t* csn = dev->dev_data.card_data[PICOPASS_CSN_BLOCK_INDEX].data;
    const uint8_t* epurse = dev->dev_data.card_data[PICOPASS_SECURE_EPURSE_BLOCK_INDEX].data;

    FuriString* filename = furi_string_alloc();
    FuriString* path = furi_string_alloc();
    FlipperFormat* file = flipper_format_file_alloc(dev->storage);
    uint8_t nr_mac[PICOPASS_BLOCK_LEN];
    uint32_t count = 0;

    for(size_t i = 0; i < PICOPASS_BLOCK_LEN; i++) {
        furi_string_cat_printf(filename, "%02x", csn[i]);
    }
    furi_string_cat_printf(filename, "_");
    for(size_t i = 0; i < PICOPASS_BLOCK_LEN; i++) {
        furi_string_cat_printf(filename, "%02x", epurse[i]);
    }
    furi_string_printf(
        path, "%s/%s%s", STORAGE_APP_DATA_PATH_PREFIX, furi_string_get_cstr(filename), ".mac");

    if(flipper_format_file_open_existing(file, furi_string_get_cstr(path))) {
        while(flipper_format_read_hex(file, "NR-MAC", nr_mac, PICOPASS_BLOCK_LEN)) {
            count++;
        }
    }

    flipper_format_free(file);
    furi_string_free(path);
    furi_string_free(filename);
    return count;
}

void picopass_scene_nr_mac_saved_on_enter(void* context) {
    Picopass* picopass = context;
    FuriString* str = furi_string_alloc();

    dolphin_deed(DolphinDeedNfcSave);

    // Setup view
    Widget* widget = picopass->widget;
    widget_add_icon_element(widget, 32, 5, &I_DolphinNice_96x59);
    widget_add_string_element(widget, 13, 22, AlignLeft, AlignBottom, FontPrimary, "NR-MAC");
    widget_add_string_element(widget, 13, 34, AlignLeft, AlignBottom, FontPrimary, "Saved!");
    furi_string_printf(str, "Captured: %lu", picopass_scene_nr_mac_saved_count(picopass));
    widget_add_string_element(
        widget, 13, 46, AlignLeft, AlignBottom, FontSecondary, furi_string_get_cstr(str));
    widget_add_button_element(
        widget,
        GuiButtonTypeLeft,
        "Another",
        picopass_scene_nr_mac_saved_widget_callback,
        picopass);

    furi_string_free(str);
    view_dispatcher_switch_to_view(picopass->view_dispatcher, PicopassViewWidget);
}

bool picopass_scene_nr_mac_saved_on_event(void* context, SceneManagerEvent event) {
    Picopass* picopass = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            // Emulate's on_enter restarts the field for another capture
            consumed = scene_manager_previous_scene(picopass->scene_manager);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        if(picopass->nr_mac_type == AutoNRMAC) {
            consumed = scene_manager_search_and_switch_to_previous_scene(
                picopass->scene_manager, PicopassSceneEliteDictAttack);
            picopass->nr_mac_type = ManualNRMAC;
        } else {
            scene_manager_set_scene_state(
                picopass->scene_manager, PicopassSceneStart, 0); // Set back to "read card"
            consumed = scene_manager_search_and_switch_to_previous_scene(
                picopass->scene_manager, PicopassSceneStart);
        }
    }
    return consumed;
}

void picopass_scene_nr_mac_saved_on_exit(void* context) {
    Picopass* picopass = context;

    // Clear view
    widget_reset(picopass->widget);
}
