#include "../picopass_i.h"

enum SubmenuIndex {
    SubmenuIndexParse,
    SubmenuIndexParseSIO,

    SubmenuIndexSave,
    SubmenuIndexSaveLegacy,
    SubmenuIndexSaveAsLF,
    SubmenuIndexSaveAsSeader,
    SubmenuIndexSavePartial,

    SubmenuIndexChangeKey,
    SubmenuIndexWrite,
    SubmenuIndexEmulate,
    SubmenuIndexMax,
};

void picopass_scene_card_menu_submenu_callback(void* context, uint32_t index) {
    Picopass* picopass = context;

    view_dispatcher_send_custom_event(picopass->view_dispatcher, index);
}

void picopass_scene_card_menu_add_items(void* context, bool included[]) {
    Picopass* picopass = context;
    Submenu* submenu = picopass->submenu;

    // Clear the menu
    submenu_reset(submenu);
    if(included[SubmenuIndexParse]) {
        submenu_add_item(
            submenu,
            "Parse",
            SubmenuIndexParse,
            picopass_scene_card_menu_submenu_callback,
            picopass);
    }
    if(included[SubmenuIndexParseSIO]) {
        submenu_add_item(
            submenu,
            "Parse SIO",
            SubmenuIndexParseSIO,
            picopass_scene_card_menu_submenu_callback,
            picopass);
    }
    if(included[SubmenuIndexSave]) {
        submenu_add_item(
            submenu, "Save", SubmenuIndexSave, picopass_scene_card_menu_submenu_callback, picopass);
    }
    if(included[SubmenuIndexSaveLegacy]) {
        submenu_add_item(
            submenu,
            "Save as Legacy",
            SubmenuIndexSaveLegacy,
            picopass_scene_card_menu_submenu_callback,
            picopass);
    }
    if(included[SubmenuIndexSaveAsLF]) {
        submenu_add_item(
            submenu,
            "Save as LFRFID",
            SubmenuIndexSaveAsLF,
            picopass_scene_card_menu_submenu_callback,
            picopass);
    }
    if(included[SubmenuIndexSaveAsSeader]) {
        submenu_add_item(
            submenu,
            "Save in Seader fmt",
            SubmenuIndexSaveAsSeader,
            picopass_scene_card_menu_submenu_callback,
            picopass);
    }
    if(included[SubmenuIndexSavePartial]) {
        submenu_add_item(
            submenu,
            "Save Partial",
            SubmenuIndexSavePartial,
            picopass_scene_card_menu_submenu_callback,
            picopass);
    }
    if(included[SubmenuIndexChangeKey]) {
        submenu_add_item(
            submenu,
            "Change Key",
            SubmenuIndexChangeKey,
            picopass_scene_card_menu_submenu_callback,
            picopass);
    }
    if(included[SubmenuIndexWrite]) {
        submenu_add_item(
            submenu,
            "Write",
            SubmenuIndexWrite,
            picopass_scene_card_menu_submenu_callback,
            picopass);
    }
    if(included[SubmenuIndexEmulate]) {
        submenu_add_item(
            submenu,
            "Emulate",
            SubmenuIndexEmulate,
            picopass_scene_card_menu_submenu_callback,
            picopass);
    }

    submenu_set_selected_item(
        picopass->submenu,
        scene_manager_get_scene_state(picopass->scene_manager, PicopassSceneCardMenu));
}

void picopass_scene_card_menu_on_enter(void* context) {
    Picopass* picopass = context;
    PicopassPacs* pacs = &picopass->dev->dev_data.pacs;
    PicopassBlock* card_data = picopass->dev->dev_data.card_data;
    PicopassDeviceAuthMethod auth = picopass->dev->dev_data.auth;

    bool SE = card_data[PICOPASS_ICLASS_PACS_CFG_BLOCK_INDEX].valid &&
              0x30 == card_data[PICOPASS_ICLASS_PACS_CFG_BLOCK_INDEX].data[0];
    bool SR = card_data[PICOPASS_ICLASS_PACS_CFG_BLOCK_INDEX].data[0] == 0xA3 &&
              card_data[10].valid && 0x30 == card_data[10].data[0];
    bool has_sio = SE || SR;
    bool secured = (card_data[PICOPASS_CONFIG_BLOCK_INDEX].data[7] & PICOPASS_FUSE_CRYPT10) !=
                   PICOPASS_FUSE_CRYPT0;
    bool no_credential = picopass_is_memset(pacs->credential, 0x00, sizeof(pacs->credential));

    // To allow us to disconnect the order of the menu items from the order we determine what is valid
    bool included[SubmenuIndexMax];
    for(size_t i = 0; i < SubmenuIndexMax; i++) {
        included[i] = false;
    }

    if(auth == PicopassDeviceAuthMethodFailed) {
        included[SubmenuIndexSavePartial] = true;
    } else {
        included[SubmenuIndexSave] = true;
    }

    if(secured && has_sio) {
        included[SubmenuIndexParseSIO] = true;
        included[SubmenuIndexSaveAsSeader] = true;
    }

    if(secured && !no_credential) {
        included[SubmenuIndexSaveAsLF] = true;
        if(SR) {
            included[SubmenuIndexSaveLegacy] = true;
        }

        wiegand_message_t wiegand_msg = picopass_pacs_extract_wmo(pacs);
        size_t format_count = picopass_wiegand_format_count(&wiegand_msg);
        if(format_count > 0) {
            included[SubmenuIndexParse] = true;
        }
    }

    if(auth == PicopassDeviceAuthMethodNone || auth == PicopassDeviceAuthMethodKey) {
        included[SubmenuIndexEmulate] = true;
        if(!has_sio) {
            included[SubmenuIndexWrite] = true;
        }
        if(secured) {
            included[SubmenuIndexChangeKey] = true;
        }
    }

    // It is possible (https://www.youtube.com/watch?v=Eef70bSRl_0)
    if(auth == PicopassDeviceAuthMethodNrMac) {
        included[SubmenuIndexEmulate] = true;
    }

    picopass_scene_card_menu_add_items(picopass, included);

    view_dispatcher_switch_to_view(picopass->view_dispatcher, PicopassViewMenu);
}

bool picopass_scene_card_menu_on_event(void* context, SceneManagerEvent event) {
    Picopass* picopass = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexSave) {
            scene_manager_set_scene_state(
                picopass->scene_manager, PicopassSceneCardMenu, SubmenuIndexSave);
            scene_manager_next_scene(picopass->scene_manager, PicopassSceneSaveName);
            picopass->dev->format = PicopassDeviceSaveFormatOriginal;
            consumed = true;
        } else if(event.event == SubmenuIndexSavePartial) {
            scene_manager_set_scene_state(
                picopass->scene_manager, PicopassSceneCardMenu, SubmenuIndexSave);
            scene_manager_next_scene(picopass->scene_manager, PicopassSceneSaveName);
            picopass->dev->format = PicopassDeviceSaveFormatPartial;
            consumed = true;
        } else if(event.event == SubmenuIndexParseSIO) {
            scene_manager_set_scene_state(
                picopass->scene_manager, PicopassSceneCardMenu, event.event);
            scene_manager_next_scene(picopass->scene_manager, PicopassSceneParseSIO);
            consumed = true;
        } else if(event.event == SubmenuIndexSaveAsSeader) {
            scene_manager_set_scene_state(
                picopass->scene_manager, PicopassSceneCardMenu, event.event);
            scene_manager_next_scene(picopass->scene_manager, PicopassSceneSaveName);
            picopass->dev->format = PicopassDeviceSaveFormatSeader;
            consumed = true;
        } else if(event.event == SubmenuIndexSaveAsLF) {
            scene_manager_set_scene_state(
                picopass->scene_manager, PicopassSceneCardMenu, SubmenuIndexSaveAsLF);
            picopass->dev->format = PicopassDeviceSaveFormatLF;
            scene_manager_next_scene(picopass->scene_manager, PicopassSceneSaveName);
            consumed = true;
        } else if(event.event == SubmenuIndexWrite) {
            scene_manager_next_scene(picopass->scene_manager, PicopassSceneWriteCard);
            consumed = true;
        } else if(event.event == SubmenuIndexEmulate) {
            scene_manager_next_scene(picopass->scene_manager, PicopassSceneEmulate);
            consumed = true;
        } else if(event.event == SubmenuIndexChangeKey) {
            scene_manager_set_scene_state(
                picopass->scene_manager, PicopassSceneCardMenu, SubmenuIndexChangeKey);
            scene_manager_next_scene(picopass->scene_manager, PicopassSceneKeyMenu);
            consumed = true;
        } else if(event.event == SubmenuIndexParse) {
            scene_manager_set_scene_state(
                picopass->scene_manager, PicopassSceneCardMenu, SubmenuIndexParse);
            scene_manager_next_scene(picopass->scene_manager, PicopassSceneFormats);
            consumed = true;
        } else if(event.event == SubmenuIndexSaveLegacy) {
            scene_manager_set_scene_state(
                picopass->scene_manager, PicopassSceneCardMenu, SubmenuIndexSaveLegacy);
            picopass->dev->format = PicopassDeviceSaveFormatLegacy;
            scene_manager_next_scene(picopass->scene_manager, PicopassSceneSaveName);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_search_and_switch_to_previous_scene(
            picopass->scene_manager, PicopassSceneStart);
    }

    return consumed;
}

void picopass_scene_card_menu_on_exit(void* context) {
    Picopass* picopass = context;

    submenu_reset(picopass->submenu);
}
