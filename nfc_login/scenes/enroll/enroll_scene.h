#pragma once

#include "nfc_login_app.h"

#define ISO14443_3A_ASYNC_FLAG_COMPLETE (1UL << 0)

typedef struct {
    Iso14443_3aData iso14443_3a_data;
    Iso14443_3aError error;
    bool detected;
    FuriThreadId thread_id;
    uint32_t reset_counter;
    NfcPoller* poller;
} AsyncPollerContext;

NfcCommand iso14443_3a_async_callback(NfcGenericEvent event, void* context);
int32_t app_enroll_scan_thread(void* context);
bool app_custom_event_callback(void* context, uint32_t event);
void app_text_input_result_callback(void* context);
void app_enroll_uid_byte_input_done(void* context);
bool app_import_nfc_file(App* app, const char* path);