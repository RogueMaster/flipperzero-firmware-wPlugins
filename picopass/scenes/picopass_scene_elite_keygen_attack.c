#include "../picopass_i.h"
#include <dolphin/dolphin.h>
#include "../picopass_elite_keygen.h"

#define PICOPASS_SCENE_DICT_ATTACK_KEYS_BATCH_UPDATE (10)
#define PICOPASS_SCENE_ELITE_KEYGEN_ATTACK_LIMIT     (2000)

NfcCommand picopass_elite_keygen_attack_worker_callback(PicopassPollerEvent event, void* context) {
    furi_assert(context);
    NfcCommand command = NfcCommandContinue;

    Picopass* picopass = context;

    if(event.type == PicopassPollerEventTypeRequestMode) {
        event.data->req_mode.mode = PicopassPollerModeRead;
    } else if(event.type == PicopassPollerEventTypeRequestKey) {
        uint8_t key[PICOPASS_KEY_LEN] = {};
        bool is_key_provided = false;
        if(picopass->dict_attack_ctx.current_key < PICOPASS_SCENE_ELITE_KEYGEN_ATTACK_LIMIT) {
            picopass_elite_nextKey(key);
            is_key_provided = true;
        }

        memcpy(event.data->req_key.key, key, PICOPASS_KEY_LEN);
        event.data->req_key.is_elite_key = true;
        event.data->req_key.is_key_provided = is_key_provided;
        if(is_key_provided) {
            picopass->dict_attack_ctx.current_key++;
            if(picopass->dict_attack_ctx.current_key %
                   PICOPASS_SCENE_DICT_ATTACK_KEYS_BATCH_UPDATE ==
               0) {
                view_dispatcher_send_custom_event(
                    picopass->view_dispatcher, PicopassCustomEventDictAttackUpdateView);
            }
        }
    } else if(
        event.type == PicopassPollerEventTypeSuccess ||
        event.type == PicopassPollerEventTypeFail ||
        event.type == PicopassPollerEventTypeAuthFail) {
        const PicopassDeviceData* data = picopass_poller_get_data(picopass->poller);
        memcpy(&picopass->dev->dev_data, data, sizeof(PicopassDeviceData));
        view_dispatcher_send_custom_event(
            picopass->view_dispatcher, PicopassCustomEventPollerSuccess);
    } else if(event.type == PicopassPollerEventTypeCardLost) {
        picopass->dict_attack_ctx.card_detected = false;
        view_dispatcher_send_custom_event(
            picopass->view_dispatcher, PicopassCustomEventDictAttackUpdateView);
    } else if(event.type == PicopassPollerEventTypeCardDetected) {
        picopass->dict_attack_ctx.card_detected = true;
        view_dispatcher_send_custom_event(
            picopass->view_dispatcher, PicopassCustomEventDictAttackUpdateView);
    }

    return command;
}

static void picopass_scene_elite_keygen_attack_update_view(Picopass* instance) {
    if(instance->dict_attack_ctx.card_detected) {
        dict_attack_set_card_detected(instance->dict_attack);
        dict_attack_set_header(instance->dict_attack, instance->dict_attack_ctx.name);
        dict_attack_set_total_dict_keys(
            instance->dict_attack, PICOPASS_SCENE_ELITE_KEYGEN_ATTACK_LIMIT);
        dict_attack_set_current_dict_key(
            instance->dict_attack, instance->dict_attack_ctx.current_key);
    } else {
        dict_attack_set_card_removed(instance->dict_attack);
    }
}

static void picopass_scene_elite_keygen_attack_callback(void* context) {
    Picopass* instance = context;

    view_dispatcher_send_custom_event(
        instance->view_dispatcher, PicopassCustomEventDictAttackSkip);
}

void picopass_scene_elite_keygen_attack_on_enter(void* context) {
    Picopass* picopass = context;
    dolphin_deed(DolphinDeedNfcRead);

    // Setup dict attack context
    uint32_t state = PicopassSceneEliteKeygenAttack;

    picopass->dict = keys_dict_alloc(
        PICOPASS_ICLASS_STANDARD_DICT_FLIPPER_NAME, KeysDictModeOpenExisting, PICOPASS_KEY_LEN);

    dict_attack_reset(picopass->dict_attack);
    picopass->dict_attack_ctx.card_detected = false;
    picopass->dict_attack_ctx.total_keys = PICOPASS_SCENE_ELITE_KEYGEN_ATTACK_LIMIT;
    picopass->dict_attack_ctx.current_key = 0;
    picopass->dict_attack_ctx.name = "Elite Keygen Attack";
    scene_manager_set_scene_state(picopass->scene_manager, PicopassSceneEliteKeygenAttack, state);

    // Setup view
    picopass_scene_elite_keygen_attack_update_view(picopass);
    dict_attack_set_callback(
        picopass->dict_attack, picopass_scene_elite_keygen_attack_callback, picopass);

    // Start worker
    picopass->poller = picopass_poller_alloc(picopass->nfc);
    picopass_poller_start(
        picopass->poller, picopass_elite_keygen_attack_worker_callback, picopass);

    view_dispatcher_switch_to_view(picopass->view_dispatcher, PicopassViewDictAttack);
    picopass_blink_start(picopass);
}

bool picopass_scene_elite_keygen_attack_on_event(void* context, SceneManagerEvent event) {
    Picopass* picopass = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == PicopassCustomEventPollerSuccess) {
            scene_manager_next_scene(picopass->scene_manager, PicopassSceneReadCardSuccess);
            consumed = true;
        } else if(event.event == PicopassCustomEventDictAttackUpdateView) {
            picopass_scene_elite_keygen_attack_update_view(picopass);
            consumed = true;
        } else if(event.event == PicopassCustomEventDictAttackSkip) {
            scene_manager_next_scene(picopass->scene_manager, PicopassSceneReadCardSuccess);
            consumed = true;
        }
    }
    return consumed;
}

void picopass_scene_elite_keygen_attack_on_exit(void* context) {
    Picopass* picopass = context;

    if(picopass->dict) {
        keys_dict_free(picopass->dict);
        picopass->dict = NULL;
    }
    picopass->dict_attack_ctx.current_key = 0;
    picopass->dict_attack_ctx.total_keys = 0;
    picopass_elite_reset();

    picopass_poller_stop(picopass->poller);
    picopass_poller_free(picopass->poller);

    // Clear view
    popup_reset(picopass->popup);

    picopass_blink_stop(picopass);
}
