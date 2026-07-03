#include "../nfc_magic_app_i.h"
#include <nfc/protocols/mf_classic/mf_classic.h>

static bool nfc_magic_scene_file_select_is_file_suitable(NfcMagicApp* instance) {
    NfcProtocol protocol = nfc_device_get_protocol(instance->source_dev);
    size_t uid_len = 0;
    nfc_device_get_uid(instance->source_dev, &uid_len);

    bool suitable = false;
    if(instance->protocol == NfcMagicProtocolGen1) {
        if((uid_len == 4 || uid_len == 7) && (protocol == NfcProtocolMfClassic)) {
            const MfClassicData* mfc_data =
                nfc_device_get_data(instance->source_dev, NfcProtocolMfClassic);
            if(mfc_data->type == MfClassicType1k) {
                suitable = true;
            }
        }
    } else if(instance->protocol == NfcMagicProtocolGen4) {
        if(protocol == NfcProtocolMfClassic) {
            suitable = true;
        } else if(protocol == NfcProtocolMfUltralight) {
            const MfUltralightData* mfu_data =
                nfc_device_get_data(instance->source_dev, NfcProtocolMfUltralight);
            const Iso14443_3aData* iso3_data = mfu_data->iso14443_3a_data;
            if(iso3_data->uid_len == 7) {
                MfUltralightType mfu_type = mfu_data->type;
                suitable = (mfu_type != MfUltralightTypeNTAGI2C1K) &&
                           (mfu_type != MfUltralightTypeNTAGI2C2K) &&
                           (mfu_type != MfUltralightTypeNTAGI2CPlus1K) &&
                           (mfu_type != MfUltralightTypeNTAGI2CPlus2K);
            }
        }
    } else if(instance->protocol == NfcMagicProtocolGen2) {
        if(protocol == NfcProtocolMfClassic) {
            suitable = true;
        }
    } else if(instance->protocol == NfcMagicProtocolClassic) {
        if(protocol == NfcProtocolMfClassic) {
            suitable = true;
        }
    }

    return suitable;
}

// For a detected Gen1 tag whose UID length is known, the source file's UID length
// must match it: writing a mismatched block 0 would corrupt the UID/BCC. Returns true
// (rejecting the file) on mismatch; allows it when the tag length is unknown.
static bool nfc_magic_scene_file_select_is_uid_len_mismatch(NfcMagicApp* instance) {
    if(instance->protocol != NfcMagicProtocolGen1) return false;
    if(instance->gen1_uid_len == 0) return false;

    size_t source_uid_len = 0;
    nfc_device_get_uid(instance->source_dev, &source_uid_len);

    return source_uid_len != instance->gen1_uid_len;
}

void nfc_magic_scene_file_select_on_enter(void* context) {
    NfcMagicApp* instance = context;

    instance->source_uid_mismatch = false;

    if(nfc_magic_load_from_file_select(instance)) {
        if(!nfc_magic_scene_file_select_is_file_suitable(instance)) {
            scene_manager_next_scene(instance->scene_manager, NfcMagicSceneWrongCard);
        } else if(nfc_magic_scene_file_select_is_uid_len_mismatch(instance)) {
            instance->source_uid_mismatch = true;
            scene_manager_next_scene(instance->scene_manager, NfcMagicSceneWrongCard);
        } else if(
            instance->protocol == NfcMagicProtocolClassic ||
            instance->protocol == NfcMagicProtocolGen2) {
            scene_manager_next_scene(instance->scene_manager, NfcMagicSceneMfClassicDictAttack);
        } else {
            scene_manager_next_scene(instance->scene_manager, NfcMagicSceneWriteConfirm);
        }
    } else {
        scene_manager_previous_scene(instance->scene_manager);
    }
}

bool nfc_magic_scene_file_select_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void nfc_magic_scene_file_select_on_exit(void* context) {
    UNUSED(context);
}
