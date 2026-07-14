#include "../passy_i.h"

#define ASN_EMIT_DEBUG 0
#include <lib/asn1/COM.h>

#define TAG "SceneAdvancedMenu"

static const char* dg_names[] = {
    "DG3 (fingers)",
    "DG4 (iris)",
    "DG5",
    "DG6",
    "DG7 (signature)",
    "DG8",
    "DG9",
    "DG10",
    "DG11 (addl person info)",
    "DG12 (addl doc info)",
    "DG13",
    "DG14",
    "DG15"};
static const uint8_t dg_ids[] =
    {0x63, 0x76, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x77, 0x42};
static const uint16_t dg_file_ids[] = {
    0x0103,
    0x0104,
    0x0105,
    0x0106,
    0x0107,
    0x0108,
    0x0109,
    0x010A,
    0x010B,
    0x010C,
    0x010D,
    0x010E,
    0x010F};
enum SubmenuIndex {
    SubmenuIndexReadDG3 = 0,
    SubmenuIndexReadDG4,
    SubmenuIndexReadDG5,
    SubmenuIndexReadDG6,
    SubmenuIndexReadDG7,
    SubmenuIndexReadDG8,
    SubmenuIndexReadDG9,
    SubmenuIndexReadDG10,
    SubmenuIndexReadDG11,
    SubmenuIndexReadDG12,
    SubmenuIndexReadDG13,
    SubmenuIndexReadDG14,
    SubmenuIndexReadDG15,
};

void passy_scene_advanced_menu_submenu_callback(void* context, uint32_t index) {
    Passy* passy = context;
    view_dispatcher_send_custom_event(passy->view_dispatcher, index);
}

void passy_scene_advanced_menu_on_enter(void* context) {
    Passy* passy = context;
    Submenu* submenu = passy->submenu;
    submenu_reset(submenu);

    COM_t* com = 0;
    com = calloc(1, sizeof *com);
    assert(com);
    asn_dec_rval_t rval = asn_decode(
        0,
        ATS_DER,
        &asn_DEF_COM,
        (void**)&com,
        bit_buffer_get_data(passy->COM),
        bit_buffer_get_size_bytes(passy->COM));

    if(rval.code == RC_OK) {
        FURI_LOG_I(TAG, "ASN.1 decode success");

        char payloadDebug[384] = {0};
        memset(payloadDebug, 0, sizeof(payloadDebug));
        (&asn_DEF_COM)
            ->op->print_struct(&asn_DEF_COM, com, 1, passy_print_struct_callback, payloadDebug);
        if(strlen(payloadDebug) > 0) {
            FURI_LOG_D(TAG, "COM: %s", payloadDebug);
        } else {
            FURI_LOG_D(TAG, "Received empty Payload");
        }
    } else {
        FURI_LOG_E(TAG, "ASN.1 decode failed: %d.  %d consumed", rval.code, rval.consumed);
        passy_log_bitbuffer(TAG, "COM", passy->COM);
    }

    for(size_t i = 0; i < com->dataGroups.size; i++) {
        uint8_t value = com->dataGroups.buf[i];
        int8_t dg_id = -1;
        for(size_t j = 0; j < sizeof(dg_ids); j++) {
            if(value == dg_ids[j]) {
                dg_id = j;
                break;
            }
        }

        if(dg_id == -1) {
            continue;
        }
        FuriString* dg_name = furi_string_alloc();
        furi_string_printf(dg_name, "Read %s", dg_names[dg_id]);
        submenu_add_item(
            submenu,
            furi_string_get_cstr(dg_name),
            dg_id, // using the index requires that the enum aligns with the dg_id
            passy_scene_advanced_menu_submenu_callback,
            passy);
        furi_string_free(dg_name);
    }

    free(com);
    com = 0;

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(passy->scene_manager, PassySceneAdvancedMenu));
    view_dispatcher_switch_to_view(passy->view_dispatcher, PassyViewMenu);
}

bool passy_scene_advanced_menu_on_event(void* context, SceneManagerEvent event) {
    Passy* passy = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(passy->scene_manager, PassySceneAdvancedMenu, event.event);
        passy->read_type = dg_file_ids[event.event];
        scene_manager_next_scene(passy->scene_manager, PassySceneRead);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(
            passy->scene_manager, PassySceneMainMenu);
        consumed = true;
    }

    return consumed;
}

void passy_scene_advanced_menu_on_exit(void* context) {
    Passy* passy = context;

    submenu_reset(passy->submenu);
}
