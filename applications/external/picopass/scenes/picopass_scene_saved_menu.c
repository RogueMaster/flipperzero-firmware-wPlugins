#include "../picopass_i.h"

enum SubmenuIndex {
    SubmenuIndexInfo,
    SubmenuIndexWrite,
    SubmenuIndexEmulate,
    SubmenuIndexSave,
    SubmenuIndexRename,
    SubmenuIndexDelete,
    SubmenuIndexSaveAsLF,
    SubmenuIndexSaveLegacy,
    SubmenuIndexSaveAsSeader,
};

void picopass_scene_saved_menu_submenu_callback(void* context, uint32_t index) {
    Picopass* picopass = context;

    view_dispatcher_send_custom_event(picopass->view_dispatcher, index);
}

void picopass_scene_saved_menu_on_enter(void* context) {
    Picopass* picopass = context;
    Submenu* submenu = picopass->submenu;

    PicopassPacs* pacs = &picopass->dev->dev_data.pacs;
    PicopassBlock* card_data = picopass->dev->dev_data.card_data;

    bool is_saved = picopass->dev->dev_name[0] != '\0';
    bool secured = (card_data[PICOPASS_CONFIG_BLOCK_INDEX].data[7] & PICOPASS_FUSE_CRYPT10) !=
                   PICOPASS_FUSE_CRYPT0;
    bool no_credential = picopass_is_memset(pacs->credential, 0x00, sizeof(pacs->credential));
    bool SE = card_data[PICOPASS_ICLASS_PACS_CFG_BLOCK_INDEX].valid &&
              0x30 == card_data[PICOPASS_ICLASS_PACS_CFG_BLOCK_INDEX].data[0];
    bool SR = card_data[PICOPASS_ICLASS_PACS_CFG_BLOCK_INDEX].data[0] == 0xA3 &&
              card_data[10].valid && 0x30 == card_data[10].data[0];
    bool has_sio = SE || SR;

    submenu_add_item(
        submenu, "Info", SubmenuIndexInfo, picopass_scene_saved_menu_submenu_callback, picopass);
    submenu_add_item(
        submenu, "Write", SubmenuIndexWrite, picopass_scene_saved_menu_submenu_callback, picopass);
    submenu_add_item(
        submenu,
        "Emulate",
        SubmenuIndexEmulate,
        picopass_scene_saved_menu_submenu_callback,
        picopass);

    if(!is_saved) {
        submenu_add_item(
            submenu,
            "Save",
            SubmenuIndexSave,
            picopass_scene_saved_menu_submenu_callback,
            picopass);
    }

    if(secured && has_sio) {
        submenu_add_item(
            submenu,
            "Save in Seader fmt",
            SubmenuIndexSaveAsSeader,
            picopass_scene_saved_menu_submenu_callback,
            picopass);
    }

    if(secured && !no_credential) {
        submenu_add_item(
            submenu,
            "Save as LFRFID",
            SubmenuIndexSaveAsLF,
            picopass_scene_saved_menu_submenu_callback,
            picopass);

        if(SR) {
            submenu_add_item(
                submenu,
                "Save as Legacy",
                SubmenuIndexSaveLegacy,
                picopass_scene_saved_menu_submenu_callback,
                picopass);
        }
    }

    if(is_saved) {
        submenu_add_item(
            submenu,
            "Rename",
            SubmenuIndexRename,
            picopass_scene_saved_menu_submenu_callback,
            picopass);
        submenu_add_item(
            submenu,
            "Delete",
            SubmenuIndexDelete,
            picopass_scene_saved_menu_submenu_callback,
            picopass);
    }

    submenu_set_selected_item(
        picopass->submenu,
        scene_manager_get_scene_state(picopass->scene_manager, PicopassSceneSavedMenu));

    view_dispatcher_switch_to_view(picopass->view_dispatcher, PicopassViewMenu);
}

bool picopass_scene_saved_menu_on_event(void* context, SceneManagerEvent event) {
    Picopass* picopass = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(
            picopass->scene_manager, PicopassSceneSavedMenu, event.event);

        if(event.event == SubmenuIndexDelete) {
            scene_manager_next_scene(picopass->scene_manager, PicopassSceneDelete);
            consumed = true;
        } else if(event.event == SubmenuIndexInfo) {
            scene_manager_next_scene(picopass->scene_manager, PicopassSceneDeviceInfo);
            consumed = true;
        } else if(event.event == SubmenuIndexWrite) {
            scene_manager_next_scene(picopass->scene_manager, PicopassSceneWriteCard);
            consumed = true;
        } else if(event.event == SubmenuIndexEmulate) {
            scene_manager_next_scene(picopass->scene_manager, PicopassSceneEmulate);
            consumed = true;
        } else if(event.event == SubmenuIndexSave) {
            scene_manager_set_scene_state(
                picopass->scene_manager, PicopassSceneSavedMenu, SubmenuIndexSave);
            picopass->dev->format = PicopassDeviceSaveFormatOriginal;
            scene_manager_next_scene(picopass->scene_manager, PicopassSceneSaveName);
            consumed = true;
        } else if(event.event == SubmenuIndexRename) {
            scene_manager_next_scene(picopass->scene_manager, PicopassSceneSaveName);
            consumed = true;
        } else if(event.event == SubmenuIndexSaveAsLF) {
            scene_manager_set_scene_state(
                picopass->scene_manager, PicopassSceneSavedMenu, SubmenuIndexSaveAsLF);
            picopass->dev->format = PicopassDeviceSaveFormatLF;
            scene_manager_next_scene(picopass->scene_manager, PicopassSceneSaveName);
            consumed = true;
        } else if(event.event == SubmenuIndexSaveLegacy) {
            scene_manager_set_scene_state(
                picopass->scene_manager, PicopassSceneSavedMenu, SubmenuIndexSaveLegacy);
            picopass->dev->format = PicopassDeviceSaveFormatLegacy;
            scene_manager_next_scene(picopass->scene_manager, PicopassSceneSaveName);
            consumed = true;
        } else if(event.event == SubmenuIndexSaveAsSeader) {
            scene_manager_set_scene_state(
                picopass->scene_manager, PicopassSceneSavedMenu, event.event);
            scene_manager_next_scene(picopass->scene_manager, PicopassSceneSaveName);
            picopass->dev->format = PicopassDeviceSaveFormatSeader;
            consumed = true;
        }
    }

    return consumed;
}

void picopass_scene_saved_menu_on_exit(void* context) {
    Picopass* picopass = context;

    submenu_reset(picopass->submenu);
}
