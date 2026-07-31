#pragma once

#include <furi.h>
#include <gui/view_dispatcher.h>
#include <storage/storage.h>

#include <stdbool.h>
#include <stdint.h>

#define CK_FIDO2_SERVICE_MAX_MESSAGE 1024U
#define CK_FIDO2_SERVICE_MAX_CREDENTIALS 20U
#define CK_FIDO2_SERVICE_APPROVAL_TIMEOUT_MS 30000U
#define CK_FIDO2_SERVICE_POLL_INTERVAL_MS 100U

typedef struct CkFido2Service CkFido2Service;

typedef enum {
    CkFido2ServiceEventPresence = 0xF200U,
    CkFido2ServiceEventPresenceDone = 0xF201U,
} CkFido2ServiceEvent;

CkFido2Service* ck_fido2_service_alloc(
    Storage* storage,
    const uint8_t vault_key[32],
    ViewDispatcher* dispatcher);
void ck_fido2_service_free(CkFido2Service* service);

bool ck_fido2_service_start(CkFido2Service* service);
void ck_fido2_service_stop(CkFido2Service* service);
bool ck_fido2_service_is_running(const CkFido2Service* service);
bool ck_fido2_service_presence_pending(const CkFido2Service* service);
void ck_fido2_service_answer_presence(CkFido2Service* service, bool approved);
