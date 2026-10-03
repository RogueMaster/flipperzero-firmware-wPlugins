#include "../nfc_magic_app_i.h"

#include <dolphin/dolphin.h>
#include <lib/nfc/protocols/mf_classic/mf_classic_poller.h>

#include "views/dict_attack.h"

#define TAG "NfcMagicMfClassicDictAttack"

// The cache holds at most one key A and one key B per sector, so a sector is worth two tries.
#define DICT_ATTACK_CACHE_KEYS_PER_SECTOR (2)

// Declared in chain order: prepare_view walks it forwards, eliding any phase whose key source is
// missing or empty, and on_event advances by one. Keep the order and the chain follows.
typedef enum {
    DictAttackStateKeyCacheInProgress,
    DictAttackStateUserDictInProgress,
    DictAttackStateSystemDictInProgress,
} DictAttackState;

// Key source for the phase in progress. The cache answers only for the sector the poller is on:
// key A, then key B, then nothing, which is what moves the poller to the next sector. In this mode
// NextSector is the poller's only sector move, and both the NextSector and DataUpdate handlers
// carry that same poller field into current_sector, so it is the sector being asked about. Cached
// keys go out through RequestKey rather than into target_dev precisely so the poller has to
// authenticate with each one; see the on_enter comment below for what pre-found keys cost.
static bool nfc_dict_attack_get_next_key(NfcMagicApp* instance, MfClassicKey* key) {
    NfcMagicAppMfClassicDictAttackContext* dict_ctx = &instance->nfc_dict_context;

    if(dict_ctx->key_cache == NULL) {
        return keys_dict_get_next_key(dict_ctx->dict, key->data, sizeof(MfClassicKey));
    }

    while(dict_ctx->cache_key_index < DICT_ATTACK_CACHE_KEYS_PER_SECTOR) {
        const bool serve_key_a = (dict_ctx->cache_key_index++ == 0);
        const MfClassicKeyType key_type = serve_key_a ? MfClassicKeyTypeA : MfClassicKeyTypeB;
        if(mfc_key_cache_get_key(dict_ctx->key_cache, dict_ctx->current_sector, key_type, key)) {
            return true;
        }
    }

    return false;
}

// A dictionary is walked from the top for every sector and every key-attack return, so it gets
// rewound at both. The cache cursor is not: see the NextSector handler.
static void nfc_dict_attack_rewind_dict(NfcMagicApp* instance) {
    if(instance->nfc_dict_context.dict != NULL) {
        keys_dict_rewind(instance->nfc_dict_context.dict);
    }
}

NfcCommand nfc_dict_attack_worker_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.event_data);
    furi_assert(event.instance);
    furi_assert(event.protocol == NfcProtocolMfClassic);

    NfcCommand command = NfcCommandContinue;
    MfClassicPollerEvent* mfc_event = event.event_data;

    NfcMagicApp* instance = context;
    if(mfc_event->type == MfClassicPollerEventTypeCardDetected) {
        instance->nfc_dict_context.is_card_present = true;
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicAppCustomEventCardDetected);
    } else if(mfc_event->type == MfClassicPollerEventTypeCardLost) {
        instance->nfc_dict_context.is_card_present = false;
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicAppCustomEventCardLost);
    } else if(mfc_event->type == MfClassicPollerEventTypeRequestMode) {
        const MfClassicData* mfc_data = nfc_poller_get_data(instance->poller);

        if(nfc_device_get_protocol(instance->target_dev) == NfcProtocolInvalid) {
            FURI_LOG_D(TAG, "Setting MFC data to target device");
            nfc_device_set_data(instance->target_dev, NfcProtocolMfClassic, mfc_data);
        } else {
            FURI_LOG_D(TAG, "MFC data already set to target device");
            mfc_data = nfc_device_get_data(instance->target_dev, NfcProtocolMfClassic);
        }

        FURI_LOG_D(TAG, "MFC type: %d", mfc_data->type);
        mfc_event->data->poller_mode.mode = MfClassicPollerModeDictAttackStandard;
        mfc_event->data->poller_mode.data = mfc_data;
        instance->nfc_dict_context.sectors_total =
            mf_classic_get_total_sectors_num(mfc_data->type);
        FURI_LOG_D(TAG, "Total sectors: %d", mf_classic_get_total_sectors_num(mfc_data->type));
        mf_classic_get_read_sectors_and_keys(
            mfc_data,
            &instance->nfc_dict_context.sectors_read,
            &instance->nfc_dict_context.keys_found);
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicAppCustomEventDictAttackDataUpdate);
    } else if(mfc_event->type == MfClassicPollerEventTypeRequestKey) {
        MfClassicKey key = {};
        if(nfc_dict_attack_get_next_key(instance, &key)) {
            mfc_event->data->key_request_data.key = key;
            mfc_event->data->key_request_data.key_provided = true;
            instance->nfc_dict_context.dict_keys_current++;
            if(instance->nfc_dict_context.dict_keys_current % 10 == 0) {
                view_dispatcher_send_custom_event(
                    instance->view_dispatcher, NfcMagicAppCustomEventDictAttackDataUpdate);
            }
        } else {
            mfc_event->data->key_request_data.key_provided = false;
        }
    } else if(mfc_event->type == MfClassicPollerEventTypeDataUpdate) {
        MfClassicPollerEventDataUpdate* data_update = &mfc_event->data->data_update;
        instance->nfc_dict_context.sectors_read = data_update->sectors_read;
        instance->nfc_dict_context.keys_found = data_update->keys_found;
        instance->nfc_dict_context.current_sector = data_update->current_sector;
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicAppCustomEventDictAttackDataUpdate);
    } else if(mfc_event->type == MfClassicPollerEventTypeNextSector) {
        nfc_dict_attack_rewind_dict(instance);
        // The cache cursor is per-sector, and this is the only event that moves the sector. A key
        // attack returns the poller to the same one, so rewinding there would re-offer a key it
        // already tried and buy one more failed auth (~10 ms of radio) per sector.
        instance->nfc_dict_context.cache_key_index = 0;
        instance->nfc_dict_context.dict_keys_current = 0;
        instance->nfc_dict_context.current_sector =
            mfc_event->data->next_sector_data.current_sector;
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicAppCustomEventDictAttackDataUpdate);
    } else if(mfc_event->type == MfClassicPollerEventTypeFoundKeyA) {
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicAppCustomEventDictAttackDataUpdate);
    } else if(mfc_event->type == MfClassicPollerEventTypeFoundKeyB) {
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicAppCustomEventDictAttackDataUpdate);
    } else if(mfc_event->type == MfClassicPollerEventTypeKeyAttackStart) {
        instance->nfc_dict_context.key_attack_current_sector =
            mfc_event->data->key_attack_data.current_sector;
        instance->nfc_dict_context.is_key_attack = true;
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicAppCustomEventDictAttackDataUpdate);
    } else if(mfc_event->type == MfClassicPollerEventTypeKeyAttackStop) {
        nfc_dict_attack_rewind_dict(instance);
        instance->nfc_dict_context.is_key_attack = false;
        instance->nfc_dict_context.dict_keys_current = 0;
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicAppCustomEventDictAttackDataUpdate);
    } else if(mfc_event->type == MfClassicPollerEventTypeSuccess) {
        const MfClassicData* mfc_data = nfc_poller_get_data(instance->poller);
        nfc_device_set_data(instance->target_dev, NfcProtocolMfClassic, mfc_data);
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicAppCustomEventDictAttackComplete);
        command = NfcCommandStop;
    }

    return command;
}

void nfc_dict_attack_dict_attack_result_callback(DictAttackEvent event, void* context) {
    furi_assert(context);
    NfcMagicApp* instance = context;

    if(event == DictAttackEventSkipPressed) {
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicAppCustomEventDictAttackSkip);
    }
}

static void nfc_magic_scene_mf_classic_dict_attack_update_view(NfcMagicApp* instance) {
    NfcMagicAppMfClassicDictAttackContext* mfc_dict = &instance->nfc_dict_context;

    if(mfc_dict->is_key_attack) {
        dict_attack_set_key_attack(instance->dict_attack, mfc_dict->key_attack_current_sector);
    } else {
        dict_attack_reset_key_attack(instance->dict_attack);
        dict_attack_set_sectors_total(instance->dict_attack, mfc_dict->sectors_total);
        dict_attack_set_sectors_read(instance->dict_attack, mfc_dict->sectors_read);
        dict_attack_set_keys_found(instance->dict_attack, mfc_dict->keys_found);
        dict_attack_set_current_dict_key(instance->dict_attack, mfc_dict->dict_keys_current);
        dict_attack_set_current_sector(instance->dict_attack, mfc_dict->current_sector);
    }
}

static void nfc_magic_scene_mf_classic_dict_attack_prepare_view(NfcMagicApp* instance) {
    uint32_t state =
        scene_manager_get_scene_state(instance->scene_manager, NfcMagicSceneMfClassicDictAttack);
    if(state == DictAttackStateKeyCacheInProgress) {
        // A magic clone carries the original's UID, so the NFC app's cache entry for the original
        // is this card's keys as well -- and for a static encrypted nonce tag it is the only place
        // they exist, since neither shared dictionary ever sees them. A miss skips the phase
        // outright (no header, no poller pass), so a card that was never saved in the NFC app, and
        // one that never activated and so has no UID at all, run exactly as they did before.
        instance->nfc_dict_context.key_cache =
            mfc_key_cache_load(instance->storage, instance->card_uid, instance->card_uid_len);
        if(instance->nfc_dict_context.key_cache != NULL) {
            dict_attack_set_header(instance->dict_attack, "MF Classic Key Cache");
        } else {
            state = DictAttackStateUserDictInProgress;
        }
    }
    if(state == DictAttackStateUserDictInProgress) {
        do {
            if(!keys_dict_check_presence(NFC_APP_MF_CLASSIC_DICT_USER_PATH)) {
                state = DictAttackStateSystemDictInProgress;
                break;
            }

            instance->nfc_dict_context.dict = keys_dict_alloc(
                NFC_APP_MF_CLASSIC_DICT_USER_PATH, KeysDictModeOpenAlways, sizeof(MfClassicKey));
            if(keys_dict_get_total_keys(instance->nfc_dict_context.dict) == 0) {
                keys_dict_free(instance->nfc_dict_context.dict);
                state = DictAttackStateSystemDictInProgress;
                break;
            }

            dict_attack_set_header(instance->dict_attack, "MF Classic User Dictionary");
        } while(false);
    }
    if(state == DictAttackStateSystemDictInProgress) {
        instance->nfc_dict_context.dict = keys_dict_alloc(
            NFC_APP_MF_CLASSIC_DICT_SYSTEM_PATH, KeysDictModeOpenExisting, sizeof(MfClassicKey));
        dict_attack_set_header(instance->dict_attack, "MF Classic System Dictionary");
    }

    // The cache offers at most an A/B pair for the sector in hand, so the bar is fixed to that
    // pair rather than to the length of a dictionary it doesn't have.
    instance->nfc_dict_context.dict_keys_total =
        (instance->nfc_dict_context.key_cache != NULL) ?
            DICT_ATTACK_CACHE_KEYS_PER_SECTOR :
            keys_dict_get_total_keys(instance->nfc_dict_context.dict);
    dict_attack_set_total_dict_keys(
        instance->dict_attack, instance->nfc_dict_context.dict_keys_total);
    instance->nfc_dict_context.dict_keys_current = 0;
    instance->nfc_dict_context.cache_key_index = 0;

    dict_attack_set_callback(
        instance->dict_attack, nfc_dict_attack_dict_attack_result_callback, instance);
    nfc_magic_scene_mf_classic_dict_attack_update_view(instance);

    scene_manager_set_scene_state(
        instance->scene_manager, NfcMagicSceneMfClassicDictAttack, state);

    // The worker thread picks its key source off key_cache, on_event picks the phase off the
    // scene state, and this is the only place both are written -- an early return here splits them.
    furi_assert(
        (instance->nfc_dict_context.key_cache != NULL) ==
        (state == DictAttackStateKeyCacheInProgress));
}

// Hand the scene to the next phase. Each phase runs its own poller pass, and prepare_view drops
// a phase whose key source is missing or empty, so the chain is key cache -> user dict -> system
// dict with either of the first two elided.
static void nfc_magic_scene_mf_classic_dict_attack_start_phase(
    NfcMagicApp* instance,
    DictAttackState next_state) {
    nfc_poller_stop(instance->poller);
    nfc_poller_free(instance->poller);
    nfc_magic_app_free_dict_attack_keys(instance);
    scene_manager_set_scene_state(
        instance->scene_manager, NfcMagicSceneMfClassicDictAttack, next_state);
    nfc_magic_scene_mf_classic_dict_attack_prepare_view(instance);
    instance->poller = nfc_poller_alloc(instance->nfc, NfcProtocolMfClassic);
    nfc_poller_start(instance->poller, nfc_dict_attack_worker_callback, instance);
}

void nfc_magic_scene_mf_classic_dict_attack_on_enter(void* context) {
    NfcMagicApp* instance = context;

    // Re-read the card fresh each op. The dict attack seeds the poller from target_dev and the
    // poller trusts pre-found keys (skips re-auth), so keys cached by a prior write/wipe would be
    // replayed -- writing/wiping with the wrong keys. Runs once per op, so the phase chain's own
    // reuse below (target_dev carried between passes) is unaffected.
    nfc_device_clear(instance->target_dev);

    scene_manager_set_scene_state(
        instance->scene_manager,
        NfcMagicSceneMfClassicDictAttack,
        DictAttackStateKeyCacheInProgress);
    nfc_magic_scene_mf_classic_dict_attack_prepare_view(instance);
    dict_attack_set_card_state(instance->dict_attack, true);
    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewDictAttack);
    nfc_magic_app_blink_start(instance);
    notification_message(instance->notifications, &sequence_display_backlight_enforce_on);

    instance->poller = nfc_poller_alloc(instance->nfc, NfcProtocolMfClassic);
    nfc_poller_start(instance->poller, nfc_dict_attack_worker_callback, instance);
}

static void nfc_magic_scene_mf_classic_dict_attack_notify_read(NfcMagicApp* instance) {
    const MfClassicData* mfc_data = nfc_poller_get_data(instance->poller);
    bool is_card_fully_read = mf_classic_is_card_read(mfc_data);
    if(is_card_fully_read) {
        notification_message(instance->notifications, &sequence_success);
    } else {
        notification_message(instance->notifications, &sequence_semi_success);
    }
}

static void nfc_magic_scene_mf_classic_dict_attack_proceed_to_write(NfcMagicApp* instance) {
    // A wipe authenticates each sector with a found key; with none found it can only fail. Skip the
    // write-check warnings (which imply a workable "proceed") and report the real reason directly --
    // ahead of the read-success cue/deed below, which would otherwise chirp success on a dead wipe.
    if(instance->gen2_poller_is_wipe_mode && instance->nfc_dict_context.keys_found == 0) {
        scene_manager_set_scene_state(
            instance->scene_manager, NfcMagicSceneWipeFail, NfcMagicWipeFailReasonNoKeys);
        scene_manager_next_scene(instance->scene_manager, NfcMagicSceneWipeFail);
        return;
    }

    nfc_magic_scene_mf_classic_dict_attack_notify_read(instance);
    dolphin_deed(DolphinDeedNfcReadSuccess);

    if(instance->protocol == NfcMagicProtocolGen2) {
        scene_manager_next_scene(instance->scene_manager, NfcMagicSceneGen2WriteCheck);
    } else {
        scene_manager_next_scene(instance->scene_manager, NfcMagicSceneMfClassicWriteCheck);
    }
}

// One statement of the chain: the enum's order is the phase order, and there is nothing left to
// look for once every sector is read and both its keys are known. Stopping there matters because
// each further phase restarts the poller and reloads its dictionary -- the system one is ~67 KB
// read on this thread -- which a cache hit that finished the card would otherwise pay for twice.
// Only the cache phase can finish a card outright, so the dictionary chain is unchanged.
static void nfc_magic_scene_mf_classic_dict_attack_advance(NfcMagicApp* instance, uint32_t state) {
    const bool card_read = (state == DictAttackStateKeyCacheInProgress) &&
                           mf_classic_is_card_read(nfc_poller_get_data(instance->poller));

    if(state == DictAttackStateSystemDictInProgress || card_read) {
        nfc_magic_scene_mf_classic_dict_attack_proceed_to_write(instance);
    } else {
        nfc_magic_scene_mf_classic_dict_attack_start_phase(instance, (DictAttackState)(state + 1));
    }
}

bool nfc_magic_scene_mf_classic_dict_attack_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    bool consumed = false;

    uint32_t state =
        scene_manager_get_scene_state(instance->scene_manager, NfcMagicSceneMfClassicDictAttack);
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcMagicAppCustomEventDictAttackComplete) {
            nfc_magic_scene_mf_classic_dict_attack_advance(instance, state);
            consumed = true;
        } else if(event.event == NfcMagicAppCustomEventCardDetected) {
            dict_attack_set_card_state(instance->dict_attack, true);
            consumed = true;
        } else if(event.event == NfcMagicAppCustomEventCardLost) {
            dict_attack_set_card_state(instance->dict_attack, false);
            consumed = true;
        } else if(event.event == NfcMagicAppCustomEventDictAttackDataUpdate) {
            nfc_magic_scene_mf_classic_dict_attack_update_view(instance);
        } else if(event.event == NfcMagicAppCustomEventDictAttackSkip) {
            const MfClassicData* mfc_data = nfc_poller_get_data(instance->poller);
            nfc_device_set_data(instance->target_dev, NfcProtocolMfClassic, mfc_data);
            // Skipping once the card is gone means the user is done, not that they want the
            // next phase started on a card that isn't there.
            if(instance->nfc_dict_context.is_card_present) {
                nfc_magic_scene_mf_classic_dict_attack_advance(instance, state);
            } else {
                nfc_magic_scene_mf_classic_dict_attack_proceed_to_write(instance);
            }
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(instance->scene_manager);
        consumed = true;
    }
    return consumed;
}

void nfc_magic_scene_mf_classic_dict_attack_on_exit(void* context) {
    NfcMagicApp* instance = context;
    const MfClassicData* mfc_data = nfc_poller_get_data(instance->poller);
    nfc_device_set_data(instance->target_dev, NfcProtocolMfClassic, mfc_data);

    nfc_poller_stop(instance->poller);
    nfc_poller_free(instance->poller);

    dict_attack_reset(instance->dict_attack);
    scene_manager_set_scene_state(
        instance->scene_manager,
        NfcMagicSceneMfClassicDictAttack,
        DictAttackStateKeyCacheInProgress);

    nfc_magic_app_free_dict_attack_keys(instance);

    instance->nfc_dict_context.cache_key_index = 0;
    instance->nfc_dict_context.current_sector = 0;
    instance->nfc_dict_context.sectors_total = 0;
    instance->nfc_dict_context.sectors_read = 0;
    instance->nfc_dict_context.keys_found = 0;
    instance->nfc_dict_context.dict_keys_total = 0;
    instance->nfc_dict_context.dict_keys_current = 0;
    instance->nfc_dict_context.is_key_attack = false;
    instance->nfc_dict_context.key_attack_current_sector = 0;
    instance->nfc_dict_context.is_card_present = false;

    nfc_magic_app_blink_stop(instance);
    notification_message(instance->notifications, &sequence_display_backlight_enforce_auto);
}
