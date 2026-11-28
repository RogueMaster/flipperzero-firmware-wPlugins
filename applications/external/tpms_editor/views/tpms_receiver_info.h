#pragma once

#include <gui/view.h>
#include "../helpers/tpms_types.h"
#include "../helpers/tpms_event.h"
#include <lib/flipper_format/flipper_format.h>

typedef struct TPMSReceiverInfo TPMSReceiverInfo;
typedef void (*TPMSReceiverInfoCallback)(TPMSCustomEvent event, void* context);

void tpms_view_receiver_info_update(
    TPMSReceiverInfo* tpms_receiver_info,
    FlipperFormat* fff,
    uint32_t frame_frequency_hz,
    const char* frame_preset_name,
    int16_t frame_rssi_dbm,
    bool frame_rssi_valid,
    uint8_t wake_profile,
    bool wake_profile_valid);
void tpms_view_receiver_info_set_callback(
    TPMSReceiverInfo* tpms_receiver_info,
    TPMSReceiverInfoCallback callback,
    void* context);

TPMSReceiverInfo* tpms_view_receiver_info_alloc();

void tpms_view_receiver_info_free(TPMSReceiverInfo* tpms_receiver_info);

View* tpms_view_receiver_info_get_view(TPMSReceiverInfo* tpms_receiver_info);
