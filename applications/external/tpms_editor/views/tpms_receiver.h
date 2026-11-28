#pragma once

#include <gui/view.h>
#include "../helpers/tpms_types.h"
#include "../helpers/tpms_event.h"

typedef struct TPMSReceiver TPMSReceiver;

typedef void (*TPMSReceiverCallback)(TPMSCustomEvent event, void* context);

void tpms_view_receiver_set_rssi(TPMSReceiver* instance, float rssi);
void tpms_view_receiver_reset_rssi(TPMSReceiver* instance);

void tpms_view_receiver_set_lock(TPMSReceiver* tpms_receiver, TPMSLock keyboard);

void tpms_view_receiver_set_auto_rx(TPMSReceiver* tpms_receiver, bool auto_rx);

void tpms_view_receiver_set_relearn_enabled(TPMSReceiver* tpms_receiver, bool enabled);

void tpms_view_receiver_set_ford_profile(
    TPMSReceiver* tpms_receiver,
    TPMSFordLFProfile profile);

void tpms_view_receiver_set_ford_duration(
    TPMSReceiver* tpms_receiver,
    uint32_t duration_ms);

void tpms_view_receiver_set_cw_frequency(
    TPMSReceiver* tpms_receiver,
    uint32_t frequency_hz);

bool tpms_view_receiver_relearn_is_active(TPMSReceiver* tpms_receiver);

void tpms_view_receiver_set_callback(
    TPMSReceiver* tpms_receiver,
    TPMSReceiverCallback callback,
    void* context);

TPMSReceiver* tpms_view_receiver_alloc();

void tpms_view_receiver_free(TPMSReceiver* tpms_receiver);

View* tpms_view_receiver_get_view(TPMSReceiver* tpms_receiver);

void tpms_view_receiver_add_data_statusbar(
    TPMSReceiver* tpms_receiver,
    const char* frequency_str,
    const char* preset_str,
    const char* history_stat_str,
    bool external);

void tpms_view_receiver_add_item_to_menu(
    TPMSReceiver* tpms_receiver,
    const char* name,
    uint8_t type);

void tpms_view_receiver_update_item(
    TPMSReceiver* tpms_receiver,
    uint16_t idx,
    const char* name,
    uint8_t type);

uint16_t tpms_view_receiver_get_idx_menu(TPMSReceiver* tpms_receiver);

void tpms_view_receiver_set_idx_menu(TPMSReceiver* tpms_receiver, uint16_t idx);

/* Emit an LF wake signal while Sub-GHz RX remains active.
   Common/CW = selectable 125.0 or 134.2 kHz CW for 1 s;
   EL-50448 = 125 kHz CW for 5 s;
   Ford EL-50449 = gated 125 kHz data telegram repeated for 5 s with
   fixed RX settings, or 2.5 s per frozen UHF profile when band/modulation AUTO is selected.
   A start request received while LF is already active is ignored so the
   current telegram/carrier always completes normally. */
void tpms_view_receiver_relearn_stop(TPMSReceiver* tpms_receiver);

void tpms_view_receiver_relearn_start(
    TPMSReceiver* tpms_receiver,
    TPMSRelearnType relearn_type);

void tpms_view_receiver_exit(void* context);
