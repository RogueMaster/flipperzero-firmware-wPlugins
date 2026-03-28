#include "seader_i.h"
#include "board_power_lifecycle.h"
#include "hf_release_sequence.h"
#include "hf_read_lifecycle.h"
#include "sam_startup_ui.h"
#include "trace_log.h"

#define TAG                                       "Seader"
#define SEADER_PLUGIN_DIR                         APP_ASSETS_PATH("plugins")
#define SEADER_WIEGAND_PLUGIN_PATH                APP_ASSETS_PATH("plugins/seader_plugin_wiegand.fal")
#define SEADER_HF_PLUGIN_PATH                     APP_ASSETS_PATH("plugins/seader_plugin_hf.fal")
#define SEADER_BOARD_POWER_SETTLE_MS              250U
#define SEADER_BOARD_ENABLE_SETTLE_MS             150U
#define SEADER_BOARD_RETRY_COOLDOWN_MS            100U
#define SEADER_BOARD_POWER_MONITOR_INTERVAL_TICKS 5U
#define SEADER_BOARD_POWER_HANDOFF_GRACE_MS       2000U
#define SEADER_HF_CONVERSATION_TIMEOUT_MS         3000U

typedef struct {
    volatile bool done;
    volatile bool detected;
} SeaderHfPicopassDetectContext;

static void seader_hf_worker_event_callback(uint32_t event, void* context);
static void seader_hf_teardown_blocking(Seader* seader);
static bool seader_hf_has_runtime(const Seader* seader);
static void seader_hf_read_reset(Seader* seader);
static void seader_hf_read_note_progress(Seader* seader);
static void seader_hf_read_fail(Seader* seader, SeaderHfReadFailureReason reason);
static bool seader_board_auto_recover_begin(Seader* seader);
static void seader_hf_mode_reset(Seader* seader);

bool seader_temp_strings_ensure(Seader* seader, size_t count) {
    if(!seader || count > 4U) {
        return false;
    }

    FuriString** slots[] = {
        &seader->temp_string1,
        &seader->temp_string2,
        &seader->temp_string3,
        &seader->temp_string4,
    };

    for(size_t i = 0; i < count; i++) {
        if(!*slots[i]) {
            *slots[i] = furi_string_alloc();
            if(!*slots[i]) {
                seader_temp_strings_release(seader, i);
                return false;
            }
        }
    }

    return true;
}

void seader_temp_strings_release(Seader* seader, size_t count) {
    if(!seader || count > 4U) {
        return;
    }

    FuriString** slots[] = {
        &seader->temp_string1,
        &seader->temp_string2,
        &seader->temp_string3,
        &seader->temp_string4,
    };

    for(size_t i = 0; i < count; i++) {
        if(*slots[i]) {
            furi_string_free(*slots[i]);
            *slots[i] = NULL;
        }
    }
}

TextInput* seader_get_text_input(Seader* seader) {
    if(!seader) {
        return NULL;
    }

    if(!seader->text_input) {
        seader->text_input = text_input_alloc();
        if(!seader->text_input) {
            FURI_LOG_E(TAG, "Failed to allocate text input view");
            return NULL;
        }

        view_dispatcher_add_view(
            seader->view_dispatcher, SeaderViewTextInput, text_input_get_view(seader->text_input));
    }

    return seader->text_input;
}

TextBox* seader_get_text_box(Seader* seader) {
    if(!seader) {
        return NULL;
    }

    if(!seader->text_box) {
        seader->text_box = text_box_alloc();
        if(!seader->text_box) {
            FURI_LOG_E(TAG, "Failed to allocate text box view");
            return NULL;
        }

        view_dispatcher_add_view(
            seader->view_dispatcher, SeaderViewTextBox, text_box_get_view(seader->text_box));
    }

    return seader->text_box;
}

Widget* seader_get_widget(Seader* seader) {
    if(!seader) {
        return NULL;
    }

    if(!seader->widget) {
        seader->widget = widget_alloc();
        if(!seader->widget) {
            FURI_LOG_E(TAG, "Failed to allocate widget view");
            return NULL;
        }

        view_dispatcher_add_view(
            seader->view_dispatcher, SeaderViewWidget, widget_get_view(seader->widget));
    }

    return seader->widget;
}

static void seader_board_runtime_clear_loss_pending(Seader* seader) {
    if(!seader) {
        return;
    }

    seader->board_power_loss_pending = false;
    seader->board_power_loss_started_at = 0U;
}

static void seader_board_prepare_missing_state(Seader* seader, SeaderBoardStatus status) {
    if(!seader) {
        return;
    }

    seader->board_status = status;
    seader->sam_present = false;
    seader_sam_key_label_format(
        false,
        SeaderSamKeyProbeStatusUnknown,
        NULL,
        0U,
        seader->sam_key_label,
        sizeof(seader->sam_key_label));
}

static bool seader_board_auto_recover_begin(Seader* seader) {
    if(!seader) {
        return false;
    }

    if(!seader->sam_present || seader->board_auto_recover_pending) {
        return false;
    }

    const bool resume_read = seader_hf_has_runtime(seader);
    seader->board_auto_recover_pending = true;
    seader->board_auto_recover_resume_read = resume_read;
    seader->board_auto_recover_read_type =
        resume_read ? seader_hf_mode_get_selected_read_type(seader) : SeaderCredentialTypeNone;

    FURI_LOG_I(
        TAG, "Begin board auto recovery resume_read=%d", seader->board_auto_recover_resume_read);
    seader_board_runtime_clear_loss_pending(seader);
    seader_show_loading_popup(seader, true);

    if(seader->board_auto_recover_resume_read) {
        return seader_hf_request_teardown(seader, SeaderHfTeardownActionAutoRecover);
    }

    seader->board_status = SeaderBoardStatusRetryRequested;
    scene_manager_next_scene(seader->scene_manager, SeaderSceneStart);
    return true;
}

static void seader_hf_mode_reset(Seader* seader) {
    if(!seader) {
        return;
    }

    seader->hf_mode_ctx.selected_read_type = SeaderCredentialTypeNone;
    memset(
        seader->hf_mode_ctx.detected_card_types,
        0,
        sizeof(seader->hf_mode_ctx.detected_card_types));
    seader->hf_mode_ctx.detected_card_type_count = 0U;
    seader->hf_mode_active = false;
}

static bool seader_board_handle_runtime_power_lost(Seader* seader) {
    if(!seader) {
        return false;
    }

    FURI_LOG_W(TAG, "Runtime board power lost");
    seader_board_runtime_clear_loss_pending(seader);
    seader->board_power_enabled = false;
    seader_show_loading_popup(seader, false);
    seader_board_prepare_missing_state(seader, SeaderBoardStatusPowerLost);
    seader_hf_read_fail(seader, SeaderHfReadFailureReasonBoardMissing);
    seader_sam_force_idle_for_recovery(seader);

    if(seader_hf_has_runtime(seader)) {
        return seader_hf_request_teardown(seader, SeaderHfTeardownActionBoardMissing);
    }

    if(seader->worker) {
        SeaderWorkerState worker_state = seader_worker_get_state(seader->worker);
        if(worker_state != SeaderWorkerStateReady && worker_state != SeaderWorkerStateNone &&
           worker_state != SeaderWorkerStateBroken) {
            seader_worker_stop(seader->worker);
        }
    }

    return scene_manager_search_and_switch_to_another_scene(
        seader->scene_manager, SeaderSceneSamMissing);
}

static void seader_hf_read_reset(Seader* seader) {
    if(!seader) {
        return;
    }

    seader->hf_read_state = SeaderHfReadStateIdle;
    seader->hf_read_failure_reason = SeaderHfReadFailureReasonNone;
    seader->hf_read_last_progress_tick = 0U;
}

static void seader_hf_read_note_progress(Seader* seader) {
    if(!seader) {
        return;
    }

    seader->hf_read_last_progress_tick = furi_get_tick();
}

static void seader_hf_read_fail(Seader* seader, SeaderHfReadFailureReason reason) {
    if(!seader) {
        return;
    }

    seader->hf_read_state = SeaderHfReadStateTerminalFail;
    seader->hf_read_failure_reason = reason;
    strlcpy(
        seader->read_error,
        seader_hf_read_failure_reason_text(reason),
        sizeof(seader->read_error));
}

static uint16_t seader_board_vbus_mv_from_volts(float vbus_voltage) {
    if(vbus_voltage <= 0.0f) {
        return 0U;
    }

    const float scaled_mv = (vbus_voltage * 1000.0f) + 0.5f;
    if(scaled_mv >= 65535.0f) {
        return UINT16_MAX;
    }

    return (uint16_t)scaled_mv;
}

static void seader_board_runtime_monitor_tick(Seader* seader) {
    if(!seader || !seader->power || !seader->board_power_enabled ||
       seader->board_status == SeaderBoardStatusPowerLost) {
        return;
    }

    seader->board_power_monitor_tick_divider++;
    if(seader->board_power_monitor_tick_divider < SEADER_BOARD_POWER_MONITOR_INTERVAL_TICKS) {
        return;
    }
    seader->board_power_monitor_tick_divider = 0U;

    PowerInfo info;
    power_get_info(seader->power, &info);
    const uint16_t vbus_mv = seader_board_vbus_mv_from_volts(info.voltage_vbus);

    const bool otg_requested = power_is_otg_enabled(seader->power);
    const uint32_t now = furi_get_tick();
    const uint32_t elapsed =
        seader->board_power_loss_pending ? (now - seader->board_power_loss_started_at) : 0U;
    const bool otg_fault = furi_hal_power_check_otg_fault() && vbus_mv < 4500U;

    const SeaderBoardRuntimePowerState runtime_state = seader_board_runtime_power_state(
        otg_requested,
        info.is_otg_enabled,
        vbus_mv,
        otg_fault,
        seader->board_power_loss_pending,
        elapsed,
        SEADER_BOARD_POWER_HANDOFF_GRACE_MS);
    const SeaderBoardRuntimeEventAction runtime_action = seader_board_runtime_event_action(
        runtime_state, seader->sam_present, seader->board_auto_recover_pending);

    if(runtime_state == SeaderBoardRuntimePowerStateHealthy) {
        seader_board_runtime_clear_loss_pending(seader);
        return;
    }

    if(runtime_state == SeaderBoardRuntimePowerStateGracePending) {
        if(!seader->board_power_loss_pending) {
            seader->board_power_loss_pending = true;
            seader->board_power_loss_started_at = now;
            FURI_LOG_I(TAG, "Waiting for OTG handoff");
        }
    }

    if(runtime_action == SeaderBoardRuntimeEventActionWait ||
       runtime_action == SeaderBoardRuntimeEventActionNone) {
        return;
    }

    if(runtime_action == SeaderBoardRuntimeEventActionAutoRecover && seader->view_dispatcher) {
        view_dispatcher_send_custom_event(
            seader->view_dispatcher, SeaderCustomEventBoardAutoRecover);
        return;
    }

    if(runtime_action == SeaderBoardRuntimeEventActionBoardPowerLost && seader->view_dispatcher) {
        view_dispatcher_send_custom_event(
            seader->view_dispatcher, SeaderCustomEventBoardPowerLost);
    }
}

static void seader_board_set_enable_pin(bool enabled) {
    furi_hal_gpio_init(&gpio_ext_pc3, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(&gpio_ext_pc3, enabled);
}

void seader_start_popup_set_stage(Seader* seader, SeaderStartupStage stage) {
    if(!seader || !seader->popup) {
        return;
    }

    Popup* popup = seader->popup;
    seader->startup_stage = stage;
    popup_set_header(popup, seader_startup_stage_header(stage), 64, 18, AlignCenter, AlignTop);

    const char* text = seader_startup_stage_text(stage);
    popup_set_text(popup, text, 64, 36, AlignCenter, AlignTop);
}

static void seader_board_prepare_external_bus(Seader* seader) {
    if(!seader || !seader->expansion || seader->expansion_disabled) {
        return;
    }

    expansion_disable(seader->expansion);
    seader->expansion_disabled = true;
}

static void seader_board_restore_external_bus(Seader* seader) {
    if(!seader || !seader->expansion || !seader->expansion_disabled) {
        return;
    }

    expansion_enable(seader->expansion);
    seader->expansion_disabled = false;
}

static void seader_board_power_fail(Seader* seader, SeaderBoardStatus status) {
    if(!seader) {
        return;
    }

    seader_board_set_enable_pin(false);
    if(seader->power && seader_board_should_disable_owned_otg(
                            seader->board_power_owned, furi_hal_power_is_otg_enabled())) {
        power_enable_otg(seader->power, false);
    }
    seader->board_power_enabled = false;
    seader->board_power_owned = false;
    seader->board_status = status;
}

static bool seader_board_power_on(Seader* seader) {
    if(!seader) {
        return false;
    }

    if(seader->board_power_enabled) {
        seader_board_set_enable_pin(true);
        return true;
    }

    seader_board_prepare_external_bus(seader);

    const bool otg_already_enabled = furi_hal_power_is_otg_enabled();
    const SeaderBoardPowerAcquirePlan plan = seader_board_power_plan_acquire(otg_already_enabled);

    seader_board_set_enable_pin(false);
    if(plan.should_enable_otg) {
        if(!seader->power) {
            seader->board_status = SeaderBoardStatusFaultPreEnable;
            return false;
        }
        power_enable_otg(seader->power, true);
    }

    furi_delay_ms(SEADER_BOARD_POWER_SETTLE_MS);
    const bool otg_enabled = furi_hal_power_is_otg_enabled();
    const uint16_t vbus_mv = seader_board_vbus_mv_from_volts(furi_hal_power_get_usb_voltage());
    seader->board_power_owned = plan.owns_otg && otg_enabled;

    if(!seader_board_power_is_available(otg_enabled, vbus_mv)) {
        FURI_LOG_W(
            TAG,
            "Board power unavailable after OTG request: otg=%d vbus=%umV",
            otg_enabled,
            vbus_mv);
        seader_board_power_fail(seader, SeaderBoardStatusFaultPreEnable);
        return false;
    }

    if(!otg_enabled && vbus_mv >= 4500U) {
        FURI_LOG_I(TAG, "Using external VBUS power at %umV", vbus_mv);
    }

    if(furi_hal_power_check_otg_fault()) {
        FURI_LOG_W(TAG, "Board power fault before enable");
        seader_board_power_fail(seader, SeaderBoardStatusFaultPreEnable);
        return false;
    }

    seader_board_set_enable_pin(true);
    furi_delay_ms(SEADER_BOARD_ENABLE_SETTLE_MS);
    if(furi_hal_power_check_otg_fault()) {
        FURI_LOG_W(TAG, "Board power fault after enable");
        seader_board_power_fail(seader, SeaderBoardStatusFaultPostEnable);
        return false;
    }
    seader->board_power_enabled = true;
    seader->board_status = SeaderBoardStatusPowerReadyPendingValidation;
    return true;
}

static void seader_board_power_off(Seader* seader) {
    if(!seader) {
        return;
    }

    seader_board_set_enable_pin(false);
    if(seader->power && seader_board_should_disable_owned_otg(
                            seader->board_power_owned, furi_hal_power_is_otg_enabled())) {
        power_enable_otg(seader->power, false);
    }
    seader->board_power_enabled = false;
    seader->board_power_owned = false;
}

bool seader_board_retry_power_cycle(Seader* seader) {
    if(!seader) {
        return false;
    }

    seader_board_power_off(seader);
    furi_delay_ms(SEADER_BOARD_RETRY_COOLDOWN_MS);
    return seader_board_power_on(seader);
}

static NfcCommand seader_hf_picopass_detect_callback(PicopassPollerEvent event, void* context) {
    SeaderHfPicopassDetectContext* detect_context = context;

    if(event.type == PicopassPollerEventTypeCardDetected ||
       event.type == PicopassPollerEventTypeSuccess) {
        detect_context->detected = true;
        detect_context->done = true;
        return NfcCommandStop;
    } else if(event.type == PicopassPollerEventTypeFail) {
        detect_context->done = true;
        return NfcCommandStop;
    }

    return NfcCommandContinue;
}

static void seader_hf_plugin_notify_event(void* host_ctx, uint32_t event) {
    Seader* seader = host_ctx;
    if(!seader || !seader->view_dispatcher) {
        FURI_LOG_W(TAG, "Drop HF plugin event %lu without dispatcher", event);
        return;
    }
    view_dispatcher_send_custom_event(seader->view_dispatcher, event);
}

static void seader_hf_plugin_notify_worker_exit(void* host_ctx) {
    seader_hf_plugin_notify_event(host_ctx, SeaderCustomEventWorkerExit);
}

static bool seader_hf_plugin_begin_card_session(
    void* host_ctx,
    uint8_t sak,
    const uint8_t* uid,
    uint8_t uid_len,
    const uint8_t* ats,
    uint8_t ats_len) {
    Seader* seader = host_ctx;
    if(!seader || !seader->worker || !seader->credential || !uid || uid_len == 0U) {
        FURI_LOG_E(
            TAG,
            "Drop HF card session invalid state seader=%p worker=%p cred=%p uid=%p uid_len=%u",
            (void*)seader,
            seader ? (void*)seader->worker : NULL,
            seader ? (void*)seader->credential : NULL,
            (const void*)uid,
            uid_len);
        return false;
    }

    const SeaderHfCardSessionDecision decision =
        seader_hf_read_on_card_detect(seader->hf_read_state, seader_sam_can_accept_card(seader));
    if(decision != SeaderHfCardSessionDecisionStartConversation) {
        seader_hf_read_fail(
            seader,
            seader_sam_can_accept_card(seader) ? SeaderHfReadFailureReasonInternalState :
                                                 SeaderHfReadFailureReasonSamBusy);
        seader_sam_force_idle_for_recovery(seader);
        if(seader->worker) {
            seader->worker->stage = SeaderPollerEventTypeFail;
        }
        return false;
    }

    if(seader_worker_card_detect(seader, sak, NULL, uid, uid_len, (uint8_t*)ats, ats_len) !=
       NfcCommandContinue) {
        seader_hf_read_fail(seader, SeaderHfReadFailureReasonInternalState);
        seader_sam_force_idle_for_recovery(seader);
        if(seader->worker) {
            seader->worker->stage = SeaderPollerEventTypeFail;
        }
        return false;
    }

    seader->hf_read_state = SeaderHfReadStateConversationStarting;
    seader_hf_read_note_progress(seader);
    seader_hf_plugin_notify_event(host_ctx, SeaderCustomEventPollerDetect);
    return true;
}

static void seader_hf_plugin_send_nfc_rx(void* host_ctx, uint8_t* buffer, size_t len) {
    Seader* seader = host_ctx;
    seader_send_nfc_rx(seader, buffer, len);
}

static void seader_hf_plugin_run_conversation(void* host_ctx) {
    Seader* seader = host_ctx;
    if(!seader || !seader->worker) {
        FURI_LOG_W(TAG, "Skip HF conversation without worker");
        return;
    }
    FURI_LOG_D(TAG, "HF plugin run conversation stage=%d", seader->worker->stage);
    seader_worker_run_hf_conversation(seader);
}

static void seader_hf_plugin_set_stage(void* host_ctx, PluginHfStage stage) {
    Seader* seader = host_ctx;
    if(seader->worker) {
        switch(stage) {
        case PluginHfStageCardDetect:
            seader->worker->stage = SeaderPollerEventTypeCardDetect;
            break;
        case PluginHfStageConversation:
            seader->worker->stage = SeaderPollerEventTypeConversation;
            break;
        case PluginHfStageComplete:
            seader->worker->stage = SeaderPollerEventTypeComplete;
            break;
        case PluginHfStageSuccess:
            seader->worker->stage = SeaderPollerEventTypeSuccess;
            break;
        case PluginHfStageFail:
        default:
            seader->worker->stage = SeaderPollerEventTypeFail;
            break;
        }
    }
}

static PluginHfStage seader_hf_plugin_get_stage(void* host_ctx) {
    Seader* seader = host_ctx;
    if(!seader->worker) {
        return PluginHfStageFail;
    }

    switch(seader->worker->stage) {
    case SeaderPollerEventTypeCardDetect:
        return PluginHfStageCardDetect;
    case SeaderPollerEventTypeConversation:
        return PluginHfStageConversation;
    case SeaderPollerEventTypeComplete:
        return PluginHfStageComplete;
    case SeaderPollerEventTypeSuccess:
        return PluginHfStageSuccess;
    case SeaderPollerEventTypeFail:
    default:
        return PluginHfStageFail;
    }
}

static void seader_hf_plugin_set_credential_type(void* host_ctx, SeaderCredentialType type) {
    Seader* seader = host_ctx;
    seader->credential->type = type;
    seader->credential->sio_len = 0U;
    seader->credential->sio_start_block = 0U;
    seader->credential->isDesfireEV2 = false;
}

static SeaderCredentialType seader_hf_plugin_get_credential_type(void* host_ctx) {
    Seader* seader = host_ctx;
    return seader->credential->type;
}

static bool seader_hf_plugin_get_desfire_ev2(void* host_ctx) {
    Seader* seader = host_ctx;
    return seader->credential->isDesfireEV2;
}

static void seader_hf_plugin_set_desfire_ev2(void* host_ctx, bool is_desfire_ev2) {
    Seader* seader = host_ctx;
    seader->credential->isDesfireEV2 = is_desfire_ev2;
}

static void seader_hf_plugin_append_picopass_sio(
    void* host_ctx,
    uint8_t block_num,
    const uint8_t* data,
    size_t len) {
    Seader* seader = host_ctx;
    SeaderCredential* credential = seader->credential;

    if(!data || len == 0U || credential->type != SeaderCredentialTypePicopass) {
        return;
    }

    if(credential->sio_len == 0U && data[0] == 0x30U) {
        credential->sio_start_block = block_num;
    }

    const size_t offset = (size_t)(block_num - credential->sio_start_block) * PICOPASS_BLOCK_LEN;
    if(offset >= sizeof(credential->sio)) {
        return;
    }

    const size_t copy_len = MIN(len, sizeof(credential->sio) - offset);
    memcpy(credential->sio + offset, data, copy_len);
    credential->sio_len = MAX(credential->sio_len, offset + copy_len);
}

static void seader_hf_plugin_set_14a_sio(void* host_ctx, const uint8_t* data, size_t len) {
    Seader* seader = host_ctx;
    SeaderCredential* credential = seader->credential;

    if(!data || credential->type != SeaderCredentialType14A) {
        return;
    }

    const size_t copy_len = MIN(len, sizeof(credential->sio));
    memcpy(credential->sio, data, copy_len);
    credential->sio_len = copy_len;
}

static Nfc* seader_hf_plugin_get_nfc(void* host_ctx) {
    Seader* seader = host_ctx;
    return seader ? seader->nfc : NULL;
}

static NfcDevice* seader_hf_plugin_get_nfc_device(void* host_ctx) {
    Seader* seader = host_ctx;
    return seader ? seader->nfc_device : NULL;
}

static bool seader_hf_plugin_picopass_detect(void* host_ctx) {
    Seader* seader = host_ctx;
    bool detected = false;
    PicopassPoller* poller = picopass_poller_alloc(seader->nfc);
    SeaderHfPicopassDetectContext detect_context = {0};

    if(!poller) {
        FURI_LOG_W(TAG, "Failed to allocate Picopass detect poller");
        return false;
    }

    picopass_poller_start(poller, seader_hf_picopass_detect_callback, &detect_context);
    for(uint8_t i = 0; i < 10 && !detect_context.done; i++) {
        furi_delay_ms(10);
    }

    picopass_poller_stop(poller);
    detected = detect_context.detected;
    picopass_poller_free(poller);

    return detected;
}

static bool seader_hf_plugin_picopass_start(
    void* host_ctx,
    PicopassPollerCallback callback,
    void* callback_ctx) {
    Seader* seader = host_ctx;

    if(seader->picopass_poller) {
        picopass_poller_stop(seader->picopass_poller);
        picopass_poller_free(seader->picopass_poller);
        seader->picopass_poller = NULL;
    }

    seader->picopass_poller = picopass_poller_alloc(seader->nfc);
    if(!seader->picopass_poller) {
        return false;
    }

    picopass_poller_start(seader->picopass_poller, callback, callback_ctx);
    return true;
}

static void seader_hf_plugin_picopass_stop(void* host_ctx) {
    Seader* seader = host_ctx;

    if(seader->picopass_poller) {
        picopass_poller_stop(seader->picopass_poller);
        picopass_poller_free(seader->picopass_poller);
        seader->picopass_poller = NULL;
    }
}

static uint8_t* seader_hf_plugin_picopass_get_csn(void* host_ctx) {
    Seader* seader = host_ctx;
    if(!seader->picopass_poller) {
        return NULL;
    }

    return picopass_poller_get_csn(seader->picopass_poller);
}

static bool seader_hf_plugin_picopass_transmit(
    void* host_ctx,
    const uint8_t* tx_data,
    size_t tx_len,
    uint8_t* rx_data,
    size_t rx_capacity,
    size_t* rx_len,
    uint32_t fwt_fc) {
    Seader* seader = host_ctx;
    if(!seader->picopass_poller || !tx_data || !rx_data || !rx_len) {
        return false;
    }

    BitBuffer* tx_buffer = bit_buffer_alloc(tx_len);
    BitBuffer* rx_buffer = bit_buffer_alloc(rx_capacity);
    bool success = false;
    if(!tx_buffer || !rx_buffer) {
        FURI_LOG_E(TAG, "Failed to allocate picopass host tx/rx buffers");
        if(tx_buffer) bit_buffer_free(tx_buffer);
        if(rx_buffer) bit_buffer_free(rx_buffer);
        return false;
    }

    bit_buffer_append_bytes(tx_buffer, tx_data, tx_len);
    PicopassError error =
        picopass_poller_send_frame(seader->picopass_poller, tx_buffer, rx_buffer, fwt_fc);
    if(error == PicopassErrorIncorrectCrc) {
        error = PicopassErrorNone;
    }

    if(error == PicopassErrorNone) {
        *rx_len = bit_buffer_get_size_bytes(rx_buffer);
        memcpy(rx_data, bit_buffer_get_data(rx_buffer), *rx_len);
        success = true;
    }

    bit_buffer_free(tx_buffer);
    bit_buffer_free(rx_buffer);
    return success;
}

static void seader_hf_plugin_set_read_error(void* host_ctx, const char* text) {
    Seader* seader = host_ctx;
    if(!text) {
        seader->read_error[0] = '\0';
        return;
    }
    strlcpy(seader->read_error, text, sizeof(seader->read_error));
}

static const PluginHfHostApi seader_hf_plugin_host_api = {
    .notify_worker_exit = seader_hf_plugin_notify_worker_exit,
    .begin_card_session = seader_hf_plugin_begin_card_session,
    .send_nfc_rx = seader_hf_plugin_send_nfc_rx,
    .run_conversation = seader_hf_plugin_run_conversation,
    .set_stage = seader_hf_plugin_set_stage,
    .get_stage = seader_hf_plugin_get_stage,
    .set_credential_type = seader_hf_plugin_set_credential_type,
    .get_credential_type = seader_hf_plugin_get_credential_type,
    .get_desfire_ev2 = seader_hf_plugin_get_desfire_ev2,
    .set_desfire_ev2 = seader_hf_plugin_set_desfire_ev2,
    .append_picopass_sio = seader_hf_plugin_append_picopass_sio,
    .set_14a_sio = seader_hf_plugin_set_14a_sio,
    .get_nfc = seader_hf_plugin_get_nfc,
    .get_nfc_device = seader_hf_plugin_get_nfc_device,
    .picopass_detect = seader_hf_plugin_picopass_detect,
    .picopass_start = seader_hf_plugin_picopass_start,
    .picopass_stop = seader_hf_plugin_picopass_stop,
    .picopass_get_csn = seader_hf_plugin_picopass_get_csn,
    .picopass_transmit = seader_hf_plugin_picopass_transmit,
    .set_read_error = seader_hf_plugin_set_read_error,
};

static void seader_hf_worker_event_callback(uint32_t event, void* context) {
    Seader* seader = context;
    if(!seader || !seader->view_dispatcher) {
        return;
    }

    view_dispatcher_send_custom_event(seader->view_dispatcher, event);
}

static void seader_hf_session_force_unloaded(Seader* seader) {
    if(!seader) {
        return;
    }

    seader->hf_plugin_ctx = NULL;
    seader->plugin_hf = NULL;
    seader->hf_plugin_manager = NULL;
    seader->poller = NULL;
    seader->picopass_poller = NULL;
    seader->hf_session_state = SeaderHfSessionStateUnloaded;
    if(seader->mode_runtime == SeaderModeRuntimeHF) {
        seader->mode_runtime = SeaderModeRuntimeNone;
    }
}

static void seader_hf_release_plugin_stop(void* context) {
    Seader* seader = context;
    if(seader && seader->plugin_hf && seader->hf_plugin_ctx) {
        seader->plugin_hf->stop(seader->hf_plugin_ctx);
    }
}

static void seader_hf_release_host_poller(void* context) {
    Seader* seader = context;
    if(seader && seader->poller) {
        FURI_LOG_I(TAG, "Stopping host NFC poller");
        nfc_poller_stop(seader->poller);
        nfc_poller_free(seader->poller);
        seader->poller = NULL;
    }
}

static void seader_hf_release_host_picopass(void* context) {
    Seader* seader = context;
    if(seader && seader->picopass_poller) {
        FURI_LOG_I(TAG, "Stopping host Picopass poller");
        picopass_poller_stop(seader->picopass_poller);
        picopass_poller_free(seader->picopass_poller);
        seader->picopass_poller = NULL;
    }
}

static void seader_hf_release_plugin_free(void* context) {
    Seader* seader = context;
    if(seader && seader->plugin_hf && seader->hf_plugin_ctx) {
        seader->plugin_hf->free(seader->hf_plugin_ctx);
    }
    if(seader) {
        seader->hf_plugin_ctx = NULL;
        seader->plugin_hf = NULL;
    }
}

static void seader_hf_release_plugin_manager(void* context) {
    Seader* seader = context;
    if(seader && seader->hf_plugin_manager) {
        FURI_LOG_I(TAG, "Unloading HF plugin");
        plugin_manager_free(seader->hf_plugin_manager);
        seader->hf_plugin_manager = NULL;
    }
}

static void seader_hf_release_worker_reset(void* context) {
    Seader* seader = context;
    if(seader && seader->worker) {
        seader_worker_reset_poller_session(seader->worker);
    }
}

bool seader_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    Seader* seader = context;
    if(event == SeaderCustomEventBoardAutoRecover) {
        return seader_board_auto_recover_begin(seader);
    }
    if(event == SeaderCustomEventBoardPowerLost) {
        return seader_board_handle_runtime_power_lost(seader);
    }
    return scene_manager_handle_custom_event(seader->scene_manager, event);
}

bool seader_back_event_callback(void* context) {
    furi_assert(context);
    Seader* seader = context;
    return scene_manager_handle_back_event(seader->scene_manager);
}

void seader_tick_event_callback(void* context) {
    furi_assert(context);
    Seader* seader = context;
    seader_board_runtime_monitor_tick(seader);
    scene_manager_handle_tick_event(seader->scene_manager);
}

Seader* seader_alloc() {
    Seader* seader = malloc(sizeof(Seader));
    seader_trace_reset();

    seader->board_power_enabled = false;
    seader->board_power_owned = false;
    seader->expansion_disabled = false;
    seader->board_status = SeaderBoardStatusUnknown;
    seader->startup_stage = SeaderStartupStageNone;
    seader->board_retry_remaining = 0U;
    seader->board_power_loss_pending = false;
    seader->board_auto_recover_pending = false;
    seader->board_auto_recover_resume_read = false;
    seader->board_auto_recover_read_type = SeaderCredentialTypeNone;
    seader->board_power_monitor_tick_divider = 0U;
    seader->board_power_loss_started_at = 0U;
    seader->expansion = furi_record_open(RECORD_EXPANSION);
    seader->power = furi_record_open(RECORD_POWER);
    if(!seader_board_power_on(seader)) {
        FURI_LOG_W(TAG, "Shared board power enable failed");
    }
    seader->is_debug_enabled = furi_hal_rtc_is_flag_set(FuriHalRtcFlagDebug);
    seader->samCommand = SamCommand_PR_NOTHING;
    seader->sam_state = SeaderSamStateIdle;
    seader->sam_intent = SeaderSamIntentNone;
    seader->sam_present = false;
    memset(seader->sam_version, 0, sizeof(seader->sam_version));
    seader->sam_key_probe_status = SeaderSamKeyProbeStatusUnknown;
    seader->uhf_probe_status = SeaderUhfProbeStatusUnknown;
    seader_sam_key_label_format(
        false,
        seader->sam_key_probe_status,
        NULL,
        0U,
        seader->sam_key_label,
        sizeof(seader->sam_key_label));
    seader_uhf_status_label_format(
        seader->uhf_probe_status,
        false,
        false,
        false,
        false,
        seader->uhf_status_label,
        sizeof(seader->uhf_status_label));
    seader_uhf_snmp_probe_init(&seader->snmp_probe);
    seader->nfc = nfc_alloc();
    seader->nfc_device = seader->nfc ? nfc_device_alloc() : NULL;
    memset(&seader->hf_mode_ctx, 0, sizeof(seader->hf_mode_ctx));
    seader->hf_mode_active = false;

    seader->worker = seader_worker_alloc();
    seader->view_dispatcher = view_dispatcher_alloc();
    seader->scene_manager = scene_manager_alloc(&seader_scene_handlers, seader);
    view_dispatcher_enable_queue(seader->view_dispatcher);
    view_dispatcher_set_event_callback_context(seader->view_dispatcher, seader);
    view_dispatcher_set_custom_event_callback(
        seader->view_dispatcher, seader_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        seader->view_dispatcher, seader_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        seader->view_dispatcher, seader_tick_event_callback, 100);

    seader->uart = seader_uart_alloc(seader);

    seader->credential = seader_credential_alloc();

    if(!seader->nfc || !seader->nfc_device) {
        FURI_LOG_W(
            TAG,
            "HF host NFC objects unavailable at startup nfc=%p device=%p",
            seader->nfc,
            seader->nfc_device);
    }

    // Open GUI record
    seader->gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(
        seader->view_dispatcher, seader->gui, ViewDispatcherTypeFullscreen);

    // Open Notification record
    seader->notifications = furi_record_open(RECORD_NOTIFICATION);

    // Submenu
    seader->submenu = submenu_alloc();
    view_dispatcher_add_view(
        seader->view_dispatcher, SeaderViewMenu, submenu_get_view(seader->submenu));

    // Popup
    seader->popup = popup_alloc();
    view_dispatcher_add_view(
        seader->view_dispatcher, SeaderViewPopup, popup_get_view(seader->popup));

    // Loading
    seader->loading = loading_alloc();
    view_dispatcher_add_view(
        seader->view_dispatcher, SeaderViewLoading, loading_get_view(seader->loading));

    // Text Input / TextBox / Widget are allocated lazily on first use.
    seader->text_input = NULL;
    seader->text_box = NULL;
    seader->text_box_store = NULL;
    seader->widget = NULL;

    // Scene strings are allocated lazily by the scenes that need them.
    seader->temp_string1 = NULL;
    seader->temp_string2 = NULL;
    seader->temp_string3 = NULL;
    seader->temp_string4 = NULL;

    seader->plugin_manager = NULL;
    seader->plugin_wiegand = NULL;
    seader->hf_plugin_manager = NULL;
    seader->plugin_hf = NULL;
    seader->hf_plugin_ctx = NULL;
    seader->mode_runtime = SeaderModeRuntimeNone;
    seader->hf_session_state = SeaderHfSessionStateUnloaded;
    seader->hf_teardown_action = SeaderHfTeardownActionNone;
    seader_hf_read_reset(seader);
    seader->loading_popup_enabled = true;
    seader->start_scene_active = false;
    seader->sam_present_menu_guard_active = false;

    if(seader->nfc_device) {
        nfc_device_set_loading_callback(seader->nfc_device, seader_nfc_loading_callback, seader);
    }

    if(seader->is_debug_enabled) {
        FURI_LOG_D(
            TAG,
            "Launch heap free=%zu min=%zu",
            memmgr_get_free_heap(),
            memmgr_get_minimum_free_heap());
    }

    return seader;
}

void seader_free(Seader* seader) {
    furi_assert(seader);

    seader->loading_popup_enabled = false;
    seader_hf_teardown_blocking(seader);
    seader_hf_mode_deactivate(seader);
    seader_worker_release(seader);
    if(seader->worker) {
        seader_worker_free(seader->worker);
        seader->worker = NULL;
    }

    seader_wiegand_plugin_release(seader);

    if(seader->nfc_device) {
        nfc_device_free(seader->nfc_device);
        seader->nfc_device = NULL;
    }

    if(seader->nfc) {
        nfc_free(seader->nfc);
        seader->nfc = NULL;
    }

    seader_uart_free(seader->uart);
    seader->uart = NULL;
    seader_board_power_off(seader);
    seader_board_restore_external_bus(seader);

    if(seader->power) {
        furi_record_close(RECORD_POWER);
        seader->power = NULL;
    }

    if(seader->expansion) {
        furi_record_close(RECORD_EXPANSION);
        seader->expansion = NULL;
    }

    seader_credential_free(seader->credential);
    seader->credential = NULL;

    // Submenu
    view_dispatcher_remove_view(seader->view_dispatcher, SeaderViewMenu);
    submenu_free(seader->submenu);

    // Popup
    view_dispatcher_remove_view(seader->view_dispatcher, SeaderViewPopup);
    popup_free(seader->popup);

    // Loading
    view_dispatcher_remove_view(seader->view_dispatcher, SeaderViewLoading);
    loading_free(seader->loading);

    // TextInput
    if(seader->text_input) {
        view_dispatcher_remove_view(seader->view_dispatcher, SeaderViewTextInput);
        text_input_free(seader->text_input);
    }

    // TextBox
    if(seader->text_box) {
        view_dispatcher_remove_view(seader->view_dispatcher, SeaderViewTextBox);
        text_box_free(seader->text_box);
    }
    if(seader->text_box_store) {
        furi_string_free(seader->text_box_store);
    }

    // Custom Widget
    if(seader->widget) {
        view_dispatcher_remove_view(seader->view_dispatcher, SeaderViewWidget);
        widget_free(seader->widget);
    }

    // Free reusable strings
    if(seader->temp_string1) furi_string_free(seader->temp_string1);
    if(seader->temp_string2) furi_string_free(seader->temp_string2);
    if(seader->temp_string3) furi_string_free(seader->temp_string3);
    if(seader->temp_string4) furi_string_free(seader->temp_string4);

    // View Dispatcher
    view_dispatcher_free(seader->view_dispatcher);

    // Scene Manager
    scene_manager_free(seader->scene_manager);

    // GUI
    furi_record_close(RECORD_GUI);
    seader->gui = NULL;

    // Notifications
    furi_record_close(RECORD_NOTIFICATION);
    seader->notifications = NULL;

    free(seader);
}

static const NotificationSequence seader_sequence_blink_start_blue = {
    &message_blink_start_10,
    &message_blink_set_color_blue,
    &message_do_not_reset,
    NULL,
};

static const NotificationSequence seader_sequence_blink_stop = {
    &message_blink_stop,
    NULL,
};

void seader_blink_start(Seader* seader) {
    notification_message(seader->notifications, &seader_sequence_blink_start_blue);
}

void seader_blink_stop(Seader* seader) {
    notification_message(seader->notifications, &seader_sequence_blink_stop);
}

void seader_nfc_loading_callback(void* context, bool show) {
    Seader* seader = context;
    if(!seader || !seader->loading_popup_enabled || !seader->view_dispatcher) {
        return;
    }

    seader_show_loading_popup(seader, show);
}

void seader_show_loading_popup(void* context, bool show) {
    Seader* seader = context;
    if(!seader || !seader->loading_popup_enabled || !seader->view_dispatcher) {
        return;
    }

    if(show) {
        // Raise timer priority so that animations can play
        furi_timer_set_thread_priority(FuriTimerThreadPriorityElevated);
        view_dispatcher_switch_to_view(seader->view_dispatcher, SeaderViewLoading);
    } else {
        // Restore default timer priority
        furi_timer_set_thread_priority(FuriTimerThreadPriorityNormal);
    }
}

bool seader_wiegand_plugin_acquire(Seader* seader) {
    furi_assert(seader);

    if(seader->plugin_wiegand) {
        return true;
    }

    if(!seader->plugin_manager) {
        seader->plugin_manager =
            plugin_manager_alloc(PLUGIN_APP_ID, PLUGIN_API_VERSION, firmware_api_interface);
        if(!seader->plugin_manager) {
            FURI_LOG_E(TAG, "Failed to allocate plugin manager");
            return false;
        }
    }

    FURI_LOG_I(TAG, "Loading Wiegand plugin from %s", SEADER_WIEGAND_PLUGIN_PATH);
    if(plugin_manager_load_single(seader->plugin_manager, SEADER_WIEGAND_PLUGIN_PATH) !=
       PluginManagerErrorNone) {
        FURI_LOG_E(TAG, "Failed to load Wiegand plugin");
        plugin_manager_free(seader->plugin_manager);
        seader->plugin_manager = NULL;
        return false;
    }

    if(plugin_manager_get_count(seader->plugin_manager) == 0) {
        FURI_LOG_E(TAG, "Wiegand plugin manager is empty after load");
        plugin_manager_free(seader->plugin_manager);
        seader->plugin_manager = NULL;
        return false;
    }

    seader->plugin_wiegand = (PluginWiegand*)plugin_manager_get_ep(seader->plugin_manager, 0);

    if(!seader->plugin_wiegand) {
        FURI_LOG_E(TAG, "Failed to resolve Wiegand plugin entry point");
        plugin_manager_free(seader->plugin_manager);
        seader->plugin_manager = NULL;
        return false;
    }

    FURI_LOG_I(TAG, "Wiegand plugin loaded: %s", seader->plugin_wiegand->name);
    return true;
}

void seader_wiegand_plugin_release(Seader* seader) {
    furi_assert(seader);

    if(!seader->plugin_manager) {
        seader->plugin_wiegand = NULL;
        return;
    }

    FURI_LOG_I(TAG, "Unloading Wiegand plugin");
    seader->plugin_wiegand = NULL;
    plugin_manager_free(seader->plugin_manager);
    seader->plugin_manager = NULL;
}

bool seader_hf_plugin_acquire(Seader* seader) {
    furi_assert(seader);

    /* UHF maintenance and HF runtime are mutually exclusive mode owners. */
    if(seader->mode_runtime == SeaderModeRuntimeUHF) {
        FURI_LOG_W(TAG, "Reject HF plugin acquire while UHF runtime is active");
        return false;
    }

    if(seader->hf_session_state == SeaderHfSessionStateTearingDown) {
        FURI_LOG_W(TAG, "Reject HF plugin acquire during teardown");
        return false;
    }

    /* Re-acquire is allowed only when the live runtime is already coherent. */
    if(seader->plugin_hf && seader->hf_plugin_ctx) {
        if(seader->hf_session_state == SeaderHfSessionStateUnloaded) {
            seader->hf_session_state = SeaderHfSessionStateLoaded;
        }
        seader->mode_runtime = SeaderModeRuntimeHF;
        return true;
    }

    /* Partial pointer state is always a bug; normalize through the single release path
       instead of trying to reason about each damaged combination inline. */
    if(seader->hf_plugin_manager || seader->plugin_hf || seader->hf_plugin_ctx) {
        FURI_LOG_W(
            TAG,
            "Normalize partial HF session manager=%p plugin=%p ctx=%p state=%d",
            (void*)seader->hf_plugin_manager,
            (void*)seader->plugin_hf,
            seader->hf_plugin_ctx,
            seader->hf_session_state);
        seader_hf_plugin_release(seader);
    }

    if(!seader->nfc || !seader->nfc_device) {
        FURI_LOG_E(
            TAG, "Host NFC objects unavailable nfc=%p device=%p", seader->nfc, seader->nfc_device);
        return false;
    }

    if(!seader->hf_plugin_manager) {
        seader->hf_plugin_manager =
            plugin_manager_alloc(HF_PLUGIN_APP_ID, HF_PLUGIN_API_VERSION, firmware_api_interface);
        if(!seader->hf_plugin_manager) {
            FURI_LOG_E(TAG, "Failed to allocate HF plugin manager");
            return false;
        }
    }

    FURI_LOG_I(TAG, "Loading HF plugin from %s", SEADER_HF_PLUGIN_PATH);
    if(plugin_manager_load_single(seader->hf_plugin_manager, SEADER_HF_PLUGIN_PATH) !=
       PluginManagerErrorNone) {
        FURI_LOG_E(TAG, "Failed to load HF plugin");
        plugin_manager_free(seader->hf_plugin_manager);
        seader_hf_session_force_unloaded(seader);
        return false;
    }

    seader->plugin_hf = (PluginHf*)plugin_manager_get_ep(seader->hf_plugin_manager, 0);

    if(!seader->plugin_hf) {
        FURI_LOG_E(TAG, "Failed to resolve HF plugin entry point");
        plugin_manager_free(seader->hf_plugin_manager);
        seader_hf_session_force_unloaded(seader);
        return false;
    }

    seader->hf_plugin_ctx = seader->plugin_hf->alloc(&seader_hf_plugin_host_api, seader);
    if(!seader->hf_plugin_ctx) {
        FURI_LOG_E(TAG, "Failed to allocate HF plugin context");
        plugin_manager_free(seader->hf_plugin_manager);
        seader_hf_session_force_unloaded(seader);
        return false;
    }

    seader->hf_session_state = SeaderHfSessionStateLoaded;
    seader->mode_runtime = SeaderModeRuntimeHF;
    FURI_LOG_I(TAG, "HF plugin loaded: %s", seader->plugin_hf->name);
    return true;
}

static bool seader_hf_has_runtime(const Seader* seader) {
    return seader && (seader->hf_plugin_manager || seader->plugin_hf || seader->hf_plugin_ctx ||
                      seader->poller || seader->picopass_poller);
}

/* App shutdown uses the same teardown primitive as normal navigation. The only difference
   is that shutdown waits synchronously for the worker-owned teardown to finish. */
static void seader_hf_teardown_blocking(Seader* seader) {
    if(!seader || !seader_hf_has_runtime(seader)) {
        return;
    }

    seader->hf_session_state = SeaderHfSessionStateTearingDown;
    if(!seader_worker_acquire(seader) || !seader->worker || !seader->uart) {
        FURI_LOG_W(TAG, "HF blocking teardown fallback");
        seader_hf_plugin_release(seader);
        return;
    }

    seader_worker_stop(seader->worker);
    FURI_LOG_I(TAG, "HF teardown blocking");
    seader_worker_start(seader->worker, SeaderWorkerStateHfTeardown, seader->uart, NULL, seader);
    seader_worker_join(seader->worker);
}

/* All HF runtime shutdown funnels through the canonical release sequence so stop/free/unload
   order cannot silently diverge between code paths. */
void seader_hf_plugin_release(Seader* seader) {
    furi_assert(seader);
    SeaderHfReleaseSequence release_sequence = {
        .context = seader,
        .hf_session_state = &seader->hf_session_state,
        .mode_runtime = &seader->mode_runtime,
        .plugin_stop = seader_hf_release_plugin_stop,
        .host_poller_release = seader_hf_release_host_poller,
        .host_picopass_release = seader_hf_release_host_picopass,
        .plugin_free = seader_hf_release_plugin_free,
        .plugin_manager_unload = seader_hf_release_plugin_manager,
        .worker_reset = seader_hf_release_worker_reset,
    };
    seader_hf_release_sequence_run(&release_sequence);
}

/* Teardown completion is the single place that collapses HF UI/runtime mode back into
   ordinary app navigation. Scenes request teardown targets; they do not perform teardown. */
bool seader_hf_finish_teardown_action(Seader* seader) {
    if(!seader) {
        return false;
    }

    FURI_LOG_I(TAG, "HF teardown complete action=%d", seader->hf_teardown_action);
    seader_show_loading_popup(seader, false);
    seader_hf_mode_reset(seader);

    const SeaderHfTeardownAction action = seader->hf_teardown_action;
    seader->hf_teardown_action = SeaderHfTeardownActionNone;

    switch(action) {
    case SeaderHfTeardownActionSamPresent:
        return scene_manager_search_and_switch_to_another_scene(
            seader->scene_manager, SeaderSceneSamPresent);
    case SeaderHfTeardownActionBoardMissing:
        return scene_manager_search_and_switch_to_another_scene(
            seader->scene_manager, SeaderSceneSamMissing);
    case SeaderHfTeardownActionAutoRecover:
        seader->board_status = SeaderBoardStatusRetryRequested;
        scene_manager_next_scene(seader->scene_manager, SeaderSceneStart);
        return true;
    case SeaderHfTeardownActionPrepareSave:
        scene_manager_next_scene(seader->scene_manager, SeaderSceneSaveName);
        return true;
    case SeaderHfTeardownActionRestartRead:
        scene_manager_next_scene(seader->scene_manager, SeaderSceneRead);
        return true;
    case SeaderHfTeardownActionStopApp:
        scene_manager_stop(seader->scene_manager);
        view_dispatcher_stop(seader->view_dispatcher);
        return true;
    case SeaderHfTeardownActionNone:
    default:
        return false;
    }
}

/* Requesting teardown is intentionally cheap: record the target, handle the no-runtime and
   already-tearing-down fast paths, and hand off actual release work to the worker. */
bool seader_hf_request_teardown(Seader* seader, SeaderHfTeardownAction action) {
    furi_assert(seader);

    FURI_LOG_I(
        TAG,
        "HF teardown requested action=%d state=%d worker_state=%d",
        action,
        seader->hf_session_state,
        seader->worker ? seader_worker_get_state(seader->worker) : -1);

    seader->hf_teardown_action = action;
    if(!seader_hf_has_runtime(seader)) {
        seader->hf_session_state = SeaderHfSessionStateUnloaded;
        return seader_hf_finish_teardown_action(seader);
    }

    if(!seader_worker_acquire(seader)) {
        return seader_hf_finish_teardown_action(seader);
    }

    if(seader->hf_session_state == SeaderHfSessionStateTearingDown ||
       (seader->worker &&
        seader_worker_get_state(seader->worker) == SeaderWorkerStateHfTeardown)) {
        return true;
    }

    seader->hf_session_state = SeaderHfSessionStateTearingDown;
    seader_worker_stop(seader->worker);
    seader_worker_start(
        seader->worker,
        SeaderWorkerStateHfTeardown,
        seader->uart,
        seader_hf_worker_event_callback,
        seader);
    return true;
}

bool seader_worker_acquire(Seader* seader) {
    furi_assert(seader);

    if(seader->worker) {
        return true;
    }

    seader->worker = seader_worker_alloc();
    return seader->worker != NULL;
}

void seader_worker_release(Seader* seader) {
    furi_assert(seader);

    if(!seader->worker) {
        return;
    }

    seader_worker_stop(seader->worker);
    seader->worker->callback = NULL;
    seader->worker->context = NULL;
    seader_worker_change_state(seader->worker, SeaderWorkerStateReady);
}

bool seader_hf_mode_activate(Seader* seader) {
    furi_assert(seader);

    if(seader->hf_mode_active) {
        return true;
    }

    seader_hf_mode_reset(seader);
    seader->hf_mode_active = true;
    seader->hf_mode_ctx.selected_read_type = SeaderCredentialTypeNone;
    return true;
}

void seader_hf_mode_deactivate(Seader* seader) {
    furi_assert(seader);

    seader_hf_mode_reset(seader);
}

SeaderCredentialType seader_hf_mode_get_selected_read_type(const Seader* seader) {
    return seader && seader->hf_mode_active ? seader->hf_mode_ctx.selected_read_type :
                                              SeaderCredentialTypeNone;
}

void seader_hf_mode_set_selected_read_type(Seader* seader, SeaderCredentialType type) {
    if(!seader || !seader->hf_mode_active) {
        FURI_LOG_W(
            TAG,
            "Ignoring HF selected read type update without mode context seader=%p active=%d type=%d",
            seader,
            seader ? (int)seader->hf_mode_active : 0,
            type);
        return;
    }
    seader->hf_mode_ctx.selected_read_type = type;
}

void seader_hf_mode_set_detected_types(
    Seader* seader,
    const SeaderCredentialType* types,
    size_t count) {
    if(!seader || !seader->hf_mode_active) {
        FURI_LOG_W(
            TAG,
            "Ignoring HF detected types update without mode context seader=%p active=%d count=%zu",
            seader,
            seader ? (int)seader->hf_mode_active : 0,
            count);
        return;
    }

    if(count > SEADER_MAX_DETECTED_CARD_TYPES) {
        count = SEADER_MAX_DETECTED_CARD_TYPES;
    }

    memset(
        seader->hf_mode_ctx.detected_card_types,
        0,
        sizeof(seader->hf_mode_ctx.detected_card_types));
    if(types && count > 0U) {
        memcpy(seader->hf_mode_ctx.detected_card_types, types, count * sizeof(types[0]));
    }
    seader->hf_mode_ctx.detected_card_type_count = count;
}

size_t seader_hf_mode_get_detected_type_count(const Seader* seader) {
    return seader && seader->hf_mode_active ? seader->hf_mode_ctx.detected_card_type_count : 0U;
}

const SeaderCredentialType* seader_hf_mode_get_detected_types(const Seader* seader) {
    return seader && seader->hf_mode_active ? seader->hf_mode_ctx.detected_card_types : NULL;
}

void seader_hf_mode_clear_detected_types(Seader* seader) {
    seader_hf_mode_set_detected_types(seader, NULL, 0U);
}

int32_t seader_app(void* p) {
    UNUSED(p);
    Seader* seader = seader_alloc();

    scene_manager_next_scene(seader->scene_manager, SeaderSceneStart);

    view_dispatcher_run(seader->view_dispatcher);

    seader_free(seader);

    return 0;
}
