#pragma once

#include <furi.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_hid.h>

typedef enum {
    CardioReportIdISO15693 = 1,
    CardioReportIdFeliCa = 2,
} CardioReportId;

typedef enum {
    CardioPlayerId1,
    CardioPlayerId2,
} CardioPlayerId;

typedef struct {
    CardioPlayerId player_id;
} CardioUsbHidConfig;

extern FuriHalUsbInterface usb_cardio;

bool cardio_is_connected(void);
bool cardio_send_report(CardioReportId report_id, const uint8_t value[8]);
void cardio_set_state_callback(HidStateCallback cb, void* ctx);
