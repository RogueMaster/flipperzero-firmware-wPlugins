#include "../nfc_magic_app_i.h"
#include "../magic/protocols/iso15693/iso15693_poller.h"
#include "../magic/protocols/iso15693/iso15693_info.h"

void nfc_magic_scene_iso15693_info_on_enter(void* context) {
    NfcMagicApp* instance = context;
    widget_reset(instance->widget);

    const Iso15693_3Data* iso_data = instance->iso15693_data->iso15693_3_data;
    const Iso15693_3SystemInfo* sys_info = &iso_data->system_info;

    FuriString* temp_str = furi_string_alloc();

    furi_string_cat_str(temp_str, "ISO15693 / NfcV\n");

    // UID (stored MSB-first: uid[0] == 0xE0, uid[1] == manufacturer, uid[2] == IC id)
    furi_string_cat_str(temp_str, "UID:");
    for(size_t i = 0; i < ISO15693_3_UID_SIZE; ++i) {
        furi_string_cat_printf(temp_str, " %02X", iso_data->uid[i]);
    }
    furi_string_push_back(temp_str, '\n');

    // Manufacturer + chip type, decoded from the UID. The chip decode inspects the full UID so
    // it can tell NXP SLI / SLIX / SLIX2 apart via the type-indicator bits of uid[3].
    const uint8_t manufacturer_id = iso15693_3_get_manufacturer_id(iso_data);
    furi_string_cat_printf(
        temp_str, "Mfr: %s\n", iso15693_info_get_manufacturer_name(manufacturer_id));
    furi_string_cat_printf(
        temp_str, "Chip: %s\n", iso15693_info_get_chip_info_ex(iso_data->uid));

    // Memory geometry from GET SYSTEM INFO (only valid when the flag bit is set).
    if(sys_info->flags & ISO15693_3_SYSINFO_FLAG_MEMORY) {
        furi_string_cat_printf(
            temp_str,
            "Memory: %u blocks x %u bytes\n",
            sys_info->block_count,
            sys_info->block_size);
    } else {
        furi_string_cat_str(temp_str, "Memory: not reported\n");
    }

    if(sys_info->flags & ISO15693_3_SYSINFO_FLAG_DSFID) {
        furi_string_cat_printf(temp_str, "DSFID: %02X\n", sys_info->dsfid);
    }
    if(sys_info->flags & ISO15693_3_SYSINFO_FLAG_AFI) {
        furi_string_cat_printf(temp_str, "AFI: %02X\n", sys_info->afi);
    }
    if(sys_info->flags & ISO15693_3_SYSINFO_FLAG_IC_REF) {
        furi_string_cat_printf(temp_str, "IC ref: %02X\n", sys_info->ic_ref);
    }

    // Full memory contents, read during activation (the view scrolls, so listing every block is
    // fine). "*" after a row marks a locked block. Only shown when the card reported its geometry.
    if((sys_info->flags & ISO15693_3_SYSINFO_FLAG_MEMORY) &&
       iso15693_3_get_block_count(iso_data) > 0) {
        const uint16_t block_count = iso15693_3_get_block_count(iso_data);
        const uint8_t block_size = iso15693_3_get_block_size(iso_data);
        furi_string_cat_printf(temp_str, "Blocks (%u x %u):\n", block_count, block_size);
        for(uint16_t block = 0; block < block_count; ++block) {
            const uint8_t* block_data = iso15693_3_get_block_data(iso_data, block);
            furi_string_cat_printf(temp_str, "%02u:", block);
            for(uint8_t i = 0; i < block_size; ++i) {
                furi_string_cat_printf(temp_str, " %02X", block_data[i]);
            }
            if(iso15693_3_is_block_locked(iso_data, block)) {
                furi_string_cat_str(temp_str, " *");
            }
            furi_string_push_back(temp_str, '\n');
        }
    }

    widget_add_text_scroll_element(
        instance->widget, 0, 0, 128, 64, furi_string_get_cstr(temp_str));

    furi_string_free(temp_str);

    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewWidget);
}

bool nfc_magic_scene_iso15693_info_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        // Skip the transient "detecting" scene; go straight back to the ISO15693 menu so Back
        // doesn't kick off another detection with a half-drawn popup.
        consumed = scene_manager_search_and_switch_to_previous_scene(
            instance->scene_manager, NfcMagicSceneIso15693);
    }

    return consumed;
}

void nfc_magic_scene_iso15693_info_on_exit(void* context) {
    NfcMagicApp* instance = context;
    widget_reset(instance->widget);
}
