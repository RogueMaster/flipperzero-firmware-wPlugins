#include "../nfc_magic_app_i.h"
#include <nfc/protocols/mf_classic/mf_classic.h>

// Source dump types we can write to a USCUID-UL (UL-C is display-only; NTAG I2C / Origin
// are out of scope).
static bool uscuid_ul_source_type_supported(MfUltralightType type) {
    return type == MfUltralightTypeUL11 || type == MfUltralightTypeUL21 ||
           type == MfUltralightTypeNTAG213 || type == MfUltralightTypeNTAG215 ||
           type == MfUltralightTypeNTAG216;
}

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
    } else if(instance->protocol == NfcMagicProtocolIso15693) {
        // Any ISO15693-3 dump is a valid clone source for a magic ISO15693 target, and so is one saved
        // under a protocol built on ISO15693-3: the stock NFC app saves an NXP SLIX tag as SLIX. What
        // a clone writes is the ISO15693-3 data underneath, which nfc_device_get_data hands over when
        // asked for the parent protocol, so the SLIX-only settings are not copied.
        if(protocol == NfcProtocolIso15693_3 ||
           nfc_protocol_has_parent(protocol, NfcProtocolIso15693_3)) {
            suitable = true;
        }
    } else if(
        instance->protocol == NfcMagicProtocolUscuidUl ||
        instance->protocol == NfcMagicProtocolUscuidUlNotDetected) {
        if(protocol == NfcProtocolMfUltralight) {
            const MfUltralightData* mfu_data =
                nfc_device_get_data(instance->source_dev, NfcProtocolMfUltralight);
            if(mfu_data != NULL) {
                if(instance->protocol == NfcMagicProtocolUscuidUlNotDetected ||
                   instance->uscuid_ul_data.maybe_ul5) {
                    // Not-detected (write-anyway) or UL-5: the target's type is unknown, so
                    // accept any supported UL/NTAG dump (the user owns the size/type risk).
                    suitable = uscuid_ul_source_type_supported(mfu_data->type);
                } else if(uscuid_ul_data_is_writable(&instance->uscuid_ul_data)) {
                    // Safeguard: source dump type must match the target's configured type
                    // (e.g. block writing an NTAG215 dump onto a UL21-configured tag).
                    suitable = (mfu_data->type == instance->uscuid_ul_data.type);
                }
            }
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
        } else if(instance->protocol == NfcMagicProtocolIso15693) {
            // ISO15693 clone goes straight to the write. The gen2 backdoor write is a harmless custom
            // command on a non-magic tag, and data blocks follow only once the UID reads back as the
            // target (see the poller), so nothing is clobbered before the card proves it takes that
            // UID; consent for the one destructive path, the gen1 fallback, is asked mid-write where it
            // becomes real. Gen2 reaches the write unprompted too when its pre-write checks find
            // nothing (gen2_write_check.c) -- NOT Classic, whose check sets uid_locked unconditionally,
            // so it always shows at least one WriteProblems screen. Gen1/Gen4/USCUID-UL show the static
            // confirm below regardless of the card. The wipe prompts because destruction is its only
            // product, whereas a clone leaves the card holding the image the user picked.
            scene_manager_next_scene(instance->scene_manager, NfcMagicSceneWrite);
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
