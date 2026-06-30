#include "../picopass_i.h"
#include "../picopass_keys.h"
#include "../picopass_wiegand.h"
#include <dolphin/dolphin.h>
#include <stdlib.h>
#include <stdio.h>

#define TAG "PicopassSceneCreate"

typedef enum {
    PicopassCreateFieldNone = 0,
    PicopassCreateFieldFacility,
    PicopassCreateFieldCard,
} PicopassCreateField;

typedef struct {
    WiegandFormat format;
    PicopassCreateField pending_field;
    uint32_t facility_code;
    uint64_t card_number;
    uint8_t cred_type;
} PicopassCreateState;

enum CreateMenuIndex {
    CreateMenuFormat,
    CreateMenuFacility,
    CreateMenuCard,
    CreateMenuCredentialType,
    CreateMenuRun,
};

typedef enum {
    CreateCredTypeIclassLegacyStandard = 0,
    CreateCredTypeCount,
} CreateCredentialType;

static PicopassCreateState create_state;
static bool create_state_initialized = false;

// Currently supports iclass legacy (standard keys) H10301 and C1k35s wiegand formats only.
// These formats have unique bit lengths (26 and 35 respectively) which allows
// reliable format detection during emulation without additional metadata.
// Support for H10302 and H10304 (both 37-bit) can be added in a future PR
// by introducing a format hint after credential generation which is checked during emulation to resolve ambiguity.

static const uint8_t picopass_create_default_csn[PICOPASS_BLOCK_LEN] =
    {0x6D, 0xC2, 0x5B, 0x15, 0xFE, 0xFF, 0x12, 0xE0};

static uint32_t picopass_create_max_facility_code(WiegandFormat format) {
    switch(format) {
    case WiegandFormat_H10301:
        return 0xFF;
    case WiegandFormat_C1k35s:
        return 0x7FF;
    default:
        return 0;
    }
}

static uint64_t picopass_create_max_card_number(WiegandFormat format) {
    switch(format) {
    case WiegandFormat_H10301:
        return 0xFFFF;
    case WiegandFormat_C1k35s:
        return 0xFFFFF;
    default:
        return 0xFFFFFFFFULL;
    }
}

static uint64_t picopass_create_clamp(uint64_t value, uint64_t max) {
    return (value > max) ? max : value;
}

static const char* picopass_create_cred_type_name(uint8_t type) {
    switch(type) {
    case CreateCredTypeIclassLegacyStandard:
        return "iClass-Std";
    default:
        return "Unknown";
    }
}

static void picopass_create_apply_cred_type_defaults(Picopass* picopass) {
    PicopassPacs* pacs = &picopass->dev->dev_data.pacs;
    switch(create_state.cred_type) {
    case CreateCredTypeIclassLegacyStandard:
    default:
        pacs->legacy = true;
        pacs->se_enabled = false;
        pacs->sio = false;
        pacs->biometrics = 0;
        pacs->pin_length = 0;
        pacs->encryption = PicopassDeviceEncryption3DES;
        pacs->elite_kdf = false;
        memcpy(pacs->key, picopass_iclass_key, PICOPASS_BLOCK_LEN);
        memset(pacs->pin0, 0, PICOPASS_BLOCK_LEN);
        memset(pacs->pin1, 0, PICOPASS_BLOCK_LEN);
        break;
    }
}

static void picopass_create_seed_legacy(PicopassDeviceData* dev_data) {
    picopass_device_data_clear(dev_data);
    dev_data->auth = PicopassDeviceAuthMethodKey;

    static const uint8_t default_config[PICOPASS_BLOCK_LEN] = {
        0x12, 0xFF, 0xFF, 0xFF, 0x7F, 0x1F, 0xFF, (PICOPASS_FUSE_CRYPT10 | 0x24)};
    static const uint8_t default_epurse[PICOPASS_BLOCK_LEN] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0x05, 0xFE, 0xFF, 0xFF};
    static const uint8_t default_aia[PICOPASS_BLOCK_LEN] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    static const uint8_t default_pacs_cfg[PICOPASS_BLOCK_LEN] = {
        0x03, 0x03, 0x03, 0x03, 0x00, 0x03, 0xE0, 0x17};

    memcpy(
        dev_data->card_data[PICOPASS_CSN_BLOCK_INDEX].data,
        picopass_create_default_csn,
        PICOPASS_BLOCK_LEN);
    dev_data->card_data[PICOPASS_CSN_BLOCK_INDEX].valid = true;

    memcpy(
        dev_data->card_data[PICOPASS_CONFIG_BLOCK_INDEX].data, default_config, PICOPASS_BLOCK_LEN);
    dev_data->card_data[PICOPASS_CONFIG_BLOCK_INDEX].valid = true;

    memcpy(
        dev_data->card_data[PICOPASS_SECURE_EPURSE_BLOCK_INDEX].data,
        default_epurse,
        PICOPASS_BLOCK_LEN);
    dev_data->card_data[PICOPASS_SECURE_EPURSE_BLOCK_INDEX].valid = true;

    dev_data->card_data[PICOPASS_SECURE_KD_BLOCK_INDEX].valid = true;
    dev_data->card_data[PICOPASS_SECURE_KC_BLOCK_INDEX].valid = true;

    memcpy(
        dev_data->card_data[PICOPASS_SECURE_AIA_BLOCK_INDEX].data,
        default_aia,
        PICOPASS_BLOCK_LEN);
    dev_data->card_data[PICOPASS_SECURE_AIA_BLOCK_INDEX].valid = true;

    memcpy(
        dev_data->card_data[PICOPASS_ICLASS_PACS_CFG_BLOCK_INDEX].data,
        default_pacs_cfg,
        PICOPASS_BLOCK_LEN);
    dev_data->card_data[PICOPASS_ICLASS_PACS_CFG_BLOCK_INDEX].valid = true;

    dev_data->card_data[7].valid = true;
    dev_data->card_data[8].valid = true;
    dev_data->card_data[9].valid = true;
}

static bool picopass_create_build(Picopass* picopass) {
    PicopassDeviceData* dev_data = &picopass->dev->dev_data;
    PicopassPacs* pacs = &dev_data->pacs;

    picopass_create_seed_legacy(dev_data);

    // Apply credential profile defaults based on selected type.
    picopass_create_apply_cred_type_defaults(picopass);

    wiegand_card_t card = {
        .FacilityCode = create_state.facility_code,
        .CardNumber = create_state.card_number,
        .ParityValid = true,
    };
    wiegand_message_t packed = {};
    bool packed_ok = false;

    switch(create_state.format) {
    case WiegandFormat_H10301:
        packed_ok = picopass_Pack_H10301(&card, &packed);
        break;
    case WiegandFormat_C1k35s:
        packed_ok = picopass_Pack_C1k35s(&card, &packed);
        break;
    default:
        break;
    }

    if(!packed_ok) {
        FURI_LOG_E(TAG, "Failed to pack credential, format: %d", create_state.format);
        return false;
    }

    pacs->bitLength = packed.Length;
    picopass_pacs_load_from_wmo(pacs, &packed);
    picopass_device_build_credential(pacs, dev_data->card_data);

    return true;
}

static void picopass_scene_create_submenu_callback(void* context, uint32_t index) {
    Picopass* picopass = context;
    view_dispatcher_send_custom_event(picopass->view_dispatcher, index);
}

static bool picopass_create_numeric_validator(const char* text, FuriString* error, void* context) {
    UNUSED(context);
    for(const char* p = text; *p; p++) {
        if((*p < '0') || (*p > '9')) {
            if(error) {
                furi_string_set(error, "Digits only");
            }
            return false;
        }
    }
    return true;
}

static void picopass_scene_create_update_menu(Picopass* picopass) {
    Submenu* submenu = picopass->submenu;
    submenu_reset(submenu);

    char label[32];

    snprintf(
        label,
        sizeof(label),
        "Cred Type: %s",
        picopass_create_cred_type_name(create_state.cred_type));
    submenu_add_item(
        submenu, label, CreateMenuCredentialType, picopass_scene_create_submenu_callback, picopass);

    snprintf(
        label, sizeof(label), "Format: %s", picopass_wiegand_format_name(create_state.format));
    submenu_add_item(
        submenu, label, CreateMenuFormat, picopass_scene_create_submenu_callback, picopass);

    snprintf(
        label, sizeof(label), "Facility Code: %lu", (unsigned long)create_state.facility_code);
    submenu_add_item(
        submenu, label, CreateMenuFacility, picopass_scene_create_submenu_callback, picopass);

    snprintf(
        label, sizeof(label), "Card Number: %llu", (unsigned long long)create_state.card_number);
    submenu_add_item(
        submenu, label, CreateMenuCard, picopass_scene_create_submenu_callback, picopass);

    submenu_add_item(
        submenu, "Run", CreateMenuRun, picopass_scene_create_submenu_callback, picopass);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(picopass->scene_manager, PicopassSceneCreate));
}

static void picopass_scene_create_text_input_callback(void* context) {
    Picopass* picopass = context;
    view_dispatcher_send_custom_event(picopass->view_dispatcher, PicopassCustomEventTextInputDone);
}

static void picopass_scene_create_start_input(
    Picopass* picopass,
    PicopassCreateField field,
    const char* header,
    uint64_t value,
    bool digits_only) {
    create_state.pending_field = field;
    if(value > 0) {
        picopass_text_store_set(picopass, "%llu", (unsigned long long)value);
    } else {
        picopass_text_store_clear(picopass);
    }

    TextInput* text_input = picopass->text_input;
    text_input_set_header_text(text_input, header);
    text_input_set_validator(
        text_input, digits_only ? picopass_create_numeric_validator : NULL, NULL);
    text_input_set_result_callback(
        text_input,
        picopass_scene_create_text_input_callback,
        picopass,
        picopass->text_store,
        sizeof(picopass->text_store),
        true);
    view_dispatcher_switch_to_view(picopass->view_dispatcher, PicopassViewTextInput);
}

static void picopass_scene_create_cycle_format(void) {
    // Cycle between supported formats only (H10301 and C1k35s)
    if(create_state.format == WiegandFormat_H10301) {
        create_state.format = WiegandFormat_C1k35s;
    } else {
        create_state.format = WiegandFormat_H10301;
    }

    uint32_t fc_max = picopass_create_max_facility_code(create_state.format);
    if(fc_max == 0) {
        create_state.facility_code = 0;
    } else if(create_state.facility_code > fc_max) {
        create_state.facility_code = fc_max;
    }

    uint64_t cn_max = picopass_create_max_card_number(create_state.format);
    if(create_state.card_number > cn_max) {
        create_state.card_number = cn_max;
    }
}

static void picopass_scene_create_apply_text(Picopass* picopass) {
    char* endptr = NULL;
    uint64_t value = strtoull(picopass->text_store, &endptr, 10);
    if((endptr == picopass->text_store) || (*endptr != '\0')) {
        value = 0;
    }

    if(create_state.pending_field == PicopassCreateFieldFacility) {
        uint32_t max = picopass_create_max_facility_code(create_state.format);
        if(max > 0) {
            value = picopass_create_clamp(value, max);
        } else {
            value = 0;
        }
        create_state.facility_code = (uint32_t)value;
    } else if(create_state.pending_field == PicopassCreateFieldCard) {
        uint64_t max = picopass_create_max_card_number(create_state.format);
        value = picopass_create_clamp(value, max);
        create_state.card_number = value;
    }

    create_state.pending_field = PicopassCreateFieldNone;
}

void picopass_scene_create_on_enter(void* context) {
    Picopass* picopass = context;

    picopass->dev->dev_name[0] = '\0';
    furi_string_reset(picopass->dev->load_path);

    // Only initialize user-facing fields once; preserve FC/CN/format across returns.
    if(!create_state_initialized) {
        create_state.format = WiegandFormat_H10301;
        create_state.facility_code = 0;
        create_state.card_number = 0;
        create_state.cred_type = CreateCredTypeIclassLegacyStandard;
        create_state.pending_field = PicopassCreateFieldNone;
        create_state_initialized = true;
    }

    picopass_scene_create_update_menu(picopass);
    view_dispatcher_switch_to_view(picopass->view_dispatcher, PicopassViewMenu);
}

bool picopass_scene_create_on_event(void* context, SceneManagerEvent event) {
    Picopass* picopass = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == CreateMenuFormat) {
            scene_manager_set_scene_state(
                picopass->scene_manager, PicopassSceneCreate, CreateMenuFormat);
            picopass_scene_create_cycle_format();
            picopass_scene_create_update_menu(picopass);
            consumed = true;
        } else if(event.event == CreateMenuFacility) {
            scene_manager_set_scene_state(
                picopass->scene_manager, PicopassSceneCreate, CreateMenuFacility);
            picopass_scene_create_start_input(
                picopass,
                PicopassCreateFieldFacility,
                "Facility Code (dec)",
                create_state.facility_code,
                true);
            consumed = true;
        } else if(event.event == CreateMenuCard) {
            scene_manager_set_scene_state(
                picopass->scene_manager, PicopassSceneCreate, CreateMenuCard);
            picopass_scene_create_start_input(
                picopass,
                PicopassCreateFieldCard,
                "Card Number (dec)",
                create_state.card_number,
                true);
            consumed = true;
        } else if(event.event == CreateMenuCredentialType) {
            scene_manager_set_scene_state(
                picopass->scene_manager, PicopassSceneCreate, CreateMenuCredentialType);
            create_state.cred_type = (create_state.cred_type + 1) % CreateCredTypeCount;
            // When credential type changes, update PACS defaults accordingly
            picopass_create_apply_cred_type_defaults(picopass);
            picopass_scene_create_update_menu(picopass);
            consumed = true;
        } else if(event.event == CreateMenuRun) {
            scene_manager_set_scene_state(
                picopass->scene_manager, PicopassSceneCreate, CreateMenuRun);
            if(picopass_create_build(picopass)) {
                picopass->dev->format = PicopassDeviceSaveFormatOriginal;
                scene_manager_next_scene(picopass->scene_manager, PicopassSceneSavedMenu);
            } else {
                picopass_scene_create_update_menu(picopass);
            }
            consumed = true;
        } else if(event.event == PicopassCustomEventTextInputDone) {
            picopass_scene_create_apply_text(picopass);
            picopass_scene_create_update_menu(picopass);
            view_dispatcher_switch_to_view(picopass->view_dispatcher, PicopassViewMenu);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_previous_scene(picopass->scene_manager);
    }

    return consumed;
}

void picopass_scene_create_on_exit(void* context) {
    Picopass* picopass = context;

    // Clear views
    text_input_set_result_callback(picopass->text_input, NULL, NULL, NULL, 0, false);
    text_input_set_validator(picopass->text_input, NULL, NULL);
    text_input_set_header_text(picopass->text_input, "");
    text_input_reset(picopass->text_input);
    submenu_reset(picopass->submenu);
}
