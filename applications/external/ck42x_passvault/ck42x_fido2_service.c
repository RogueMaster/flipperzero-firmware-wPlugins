#include "ck42x_fido2_service.h"

#include "ck42x_ctap2.h"
#include "ck42x_fido2.h"

#include <furi_hal.h>
#include <furi_hal_usb_hid_u2f.h>
#include <storage/storage.h>

#include <stdlib.h>
#include <string.h>

#define CK_FIDO2_FILE             APP_DATA_PATH("fido2.pv1")
#define CK_FIDO2_TEMP_SUFFIX      ".tmp"
#define CK_FIDO2_BACKUP_SUFFIX    ".bak"
#define CK_FIDO2_KEY_SIZE         32U
#define CK_FIDO2_NONCE_SIZE       12U
#define CK_FIDO2_TAG_SIZE         16U
#define CK_FIDO2_MAGIC_SIZE       8U
#define CK_FIDO2_FILE_HEADER_SIZE (CK_FIDO2_MAGIC_SIZE + CK_FIDO2_NONCE_SIZE + CK_FIDO2_TAG_SIZE)
#define CK_FIDO2_FILE_MAX_SIZE   \
    (CK_FIDO2_FILE_HEADER_SIZE + \
     CK_FIDO2_SERVICE_MAX_CREDENTIALS * CK_CTAP2_CREDENTIAL_RECORD_SIZE)

#define CK_FIDO2_WORKER_STACK        (16U * 1024U)
#define CK_FIDO2_REQUEST_QUEUE_DEPTH 32U

static const uint8_t ck_fido2_magic[CK_FIDO2_MAGIC_SIZE] = {'C', 'K', 'F', '2', 'P', 'V', '1', 0};

struct CkFido2Service {
    Storage* storage;
    ViewDispatcher* dispatcher;
    FuriThread* worker;
    FuriSemaphore* approval_semaphore;
    FuriSemaphore* request_semaphore;
    FuriHalUsbInterface* previous_usb;
    uint8_t vault_key[CK_FIDO2_KEY_SIZE];
    volatile bool running;
    volatile bool presence_pending;
    volatile bool presence_approved;
    volatile bool presence_cancelled;
    volatile uint32_t active_channel;
    uint8_t deferred_reports[CK_FIDO2_REQUEST_QUEUE_DEPTH][CK_FIDO2_HID_REPORT_SIZE];
    size_t deferred_head;
    size_t deferred_count;
};

typedef enum {
    CkFido2StoreMissing,
    CkFido2StoreValid,
    CkFido2StoreInvalid,
} CkFido2StoreState;

static void ck_fido2_send_message(
    uint32_t channel,
    uint8_t command,
    const uint8_t* message,
    size_t message_length);

static void ck_fido2_zero(void* data, size_t length) {
    volatile uint8_t* byte = data;
    while(length--)
        *byte++ = 0;
}

static FuriString* ck_fido2_path(CkFido2Service* service) {
    FuriString* path = furi_string_alloc_set(CK_FIDO2_FILE);
    storage_common_resolve_path_and_ensure_app_directory(service->storage, path);
    return path;
}

static void ck_fido2_recover_interrupted_update(CkFido2Service* service) {
    FuriString* path = ck_fido2_path(service);
    FuriString* temporary =
        furi_string_alloc_printf("%s%s", furi_string_get_cstr(path), CK_FIDO2_TEMP_SUFFIX);
    FuriString* backup =
        furi_string_alloc_printf("%s%s", furi_string_get_cstr(path), CK_FIDO2_BACKUP_SUFFIX);
    bool target_exists = storage_common_stat(service->storage, furi_string_get_cstr(path), NULL) ==
                         FSE_OK;
    bool backup_exists =
        storage_common_stat(service->storage, furi_string_get_cstr(backup), NULL) == FSE_OK;
    if(!target_exists && backup_exists) {
        storage_common_rename(
            service->storage, furi_string_get_cstr(backup), furi_string_get_cstr(path));
    }
    storage_common_remove(service->storage, furi_string_get_cstr(temporary));
    furi_string_free(backup);
    furi_string_free(temporary);
    furi_string_free(path);
}

static bool ck_fido2_read_file(
    CkFido2Service* service,
    uint8_t* output,
    size_t capacity,
    size_t* output_length) {
    bool ok = false;
    FuriString* path = ck_fido2_path(service);
    File* file = storage_file_alloc(service->storage);
    *output_length = 0;
    if(storage_file_open(file, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t size = storage_file_size(file);
        if(size <= capacity && storage_file_read(file, output, (size_t)size) == (size_t)size) {
            *output_length = (size_t)size;
            ok = true;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_string_free(path);
    return ok;
}

static CkFido2StoreState ck_fido2_decrypt_records(
    CkFido2Service* service,
    uint8_t records[CK_FIDO2_SERVICE_MAX_CREDENTIALS][CK_CTAP2_CREDENTIAL_RECORD_SIZE],
    size_t* count) {
    uint8_t file_data[CK_FIDO2_FILE_MAX_SIZE];
    size_t file_length = 0;
    bool ok = false;
    *count = 0;
    ck_fido2_recover_interrupted_update(service);
    FuriString* path = ck_fido2_path(service);
    FS_Error stat = storage_common_stat(service->storage, furi_string_get_cstr(path), NULL);
    furi_string_free(path);
    if(stat == FSE_NOT_EXIST) {
        ck_fido2_zero(file_data, sizeof(file_data));
        return CkFido2StoreMissing;
    }
    if(stat != FSE_OK) {
        ck_fido2_zero(file_data, sizeof(file_data));
        return CkFido2StoreInvalid;
    }
    if(!ck_fido2_read_file(service, file_data, sizeof(file_data), &file_length)) goto cleanup;
    if(file_length < CK_FIDO2_FILE_HEADER_SIZE ||
       memcmp(file_data, ck_fido2_magic, CK_FIDO2_MAGIC_SIZE) != 0)
        goto cleanup;

    size_t plaintext_length = file_length - CK_FIDO2_FILE_HEADER_SIZE;
    if(plaintext_length % CK_CTAP2_CREDENTIAL_RECORD_SIZE != 0U) goto cleanup;
    *count = plaintext_length / CK_CTAP2_CREDENTIAL_RECORD_SIZE;
    if(*count > CK_FIDO2_SERVICE_MAX_CREDENTIALS) goto cleanup;

    const uint8_t* nonce = file_data + CK_FIDO2_MAGIC_SIZE;
    const uint8_t* tag = nonce + CK_FIDO2_NONCE_SIZE;
    const uint8_t* cipher = tag + CK_FIDO2_TAG_SIZE;
    if(furi_hal_crypto_gcm_decrypt_and_verify(
           service->vault_key,
           nonce,
           ck_fido2_magic,
           CK_FIDO2_MAGIC_SIZE,
           cipher,
           (uint8_t*)records,
           plaintext_length,
           tag) != FuriHalCryptoGCMStateOk) {
        *count = 0;
        goto cleanup;
    }
    for(size_t i = 0; i < *count; i++) {
        if(records[i][0] != CK_CTAP2_CREDENTIAL_RECORD_VERSION) {
            *count = 0;
            goto cleanup;
        }
    }
    ok = true;

cleanup:
    ck_fido2_zero(file_data, sizeof(file_data));
    if(!ok) ck_fido2_zero(records, CK_FIDO2_FILE_MAX_SIZE - CK_FIDO2_FILE_HEADER_SIZE);
    return ok ? CkFido2StoreValid : CkFido2StoreInvalid;
}

static bool ck_fido2_encrypt_records(
    CkFido2Service* service,
    uint8_t records[CK_FIDO2_SERVICE_MAX_CREDENTIALS][CK_CTAP2_CREDENTIAL_RECORD_SIZE],
    size_t count) {
    uint8_t cipher[CK_FIDO2_SERVICE_MAX_CREDENTIALS * CK_CTAP2_CREDENTIAL_RECORD_SIZE];
    uint8_t nonce[CK_FIDO2_NONCE_SIZE];
    uint8_t tag[CK_FIDO2_TAG_SIZE];
    bool ok = false;
    size_t plaintext_length = count * CK_CTAP2_CREDENTIAL_RECORD_SIZE;
    if(count > CK_FIDO2_SERVICE_MAX_CREDENTIALS) goto cleanup;

    furi_hal_random_fill_buf(nonce, sizeof(nonce));
    if(furi_hal_crypto_gcm_encrypt_and_tag(
           service->vault_key,
           nonce,
           ck_fido2_magic,
           CK_FIDO2_MAGIC_SIZE,
           (const uint8_t*)records,
           cipher,
           plaintext_length,
           tag) != FuriHalCryptoGCMStateOk)
        goto cleanup;

    FuriString* path = ck_fido2_path(service);
    FuriString* temporary =
        furi_string_alloc_printf("%s%s", furi_string_get_cstr(path), CK_FIDO2_TEMP_SUFFIX);
    FuriString* backup =
        furi_string_alloc_printf("%s%s", furi_string_get_cstr(path), CK_FIDO2_BACKUP_SUFFIX);
    storage_common_remove(service->storage, furi_string_get_cstr(temporary));
    File* file = storage_file_alloc(service->storage);
    if(storage_file_open(file, furi_string_get_cstr(temporary), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        ok = storage_file_write(file, ck_fido2_magic, sizeof(ck_fido2_magic)) ==
                 sizeof(ck_fido2_magic) &&
             storage_file_write(file, nonce, sizeof(nonce)) == sizeof(nonce) &&
             storage_file_write(file, tag, sizeof(tag)) == sizeof(tag) &&
             storage_file_write(file, cipher, plaintext_length) == plaintext_length;
        if(ok) ok = storage_file_sync(file);
    }
    storage_file_close(file);
    storage_file_free(file);
    if(ok) {
        bool had_target =
            storage_common_stat(service->storage, furi_string_get_cstr(path), NULL) == FSE_OK;
        storage_common_remove(service->storage, furi_string_get_cstr(backup));
        if(had_target &&
           storage_common_rename(
               service->storage, furi_string_get_cstr(path), furi_string_get_cstr(backup)) !=
               FSE_OK) {
            ok = false;
        }
        if(ok &&
           storage_common_rename(
               service->storage, furi_string_get_cstr(temporary), furi_string_get_cstr(path)) !=
               FSE_OK) {
            ok = false;
            if(had_target)
                storage_common_rename(
                    service->storage, furi_string_get_cstr(backup), furi_string_get_cstr(path));
        }
        if(ok && had_target) storage_common_remove(service->storage, furi_string_get_cstr(backup));
    }
    if(!ok) storage_common_remove(service->storage, furi_string_get_cstr(temporary));
    furi_string_free(backup);
    furi_string_free(temporary);
    furi_string_free(path);

cleanup:
    ck_fido2_zero(cipher, sizeof(cipher));
    ck_fido2_zero(nonce, sizeof(nonce));
    ck_fido2_zero(tag, sizeof(tag));
    return ok;
}

static bool ck_fido2_random(void* context, uint8_t* output, size_t length) {
    UNUSED(context);
    if(!output) return false;
    furi_hal_random_fill_buf(output, length);
    return true;
}

static bool ck_fido2_report_is_cancel(
    const uint8_t report[CK_FIDO2_HID_REPORT_SIZE],
    uint32_t active_channel) {
    uint32_t channel = ((uint32_t)report[0] << 24) | ((uint32_t)report[1] << 16) |
                       ((uint32_t)report[2] << 8) | report[3];
    return channel == active_channel &&
           report[4] == (CK_FIDO2_HID_CMD_MASK | CK_FIDO2_HID_CMD_CANCEL) && report[5] == 0U &&
           report[6] == 0U;
}

static bool ck_fido2_consume_cancel(CkFido2Service* service) {
    if(furi_semaphore_acquire(service->request_semaphore, 0U) != FuriStatusOk) return false;
    uint8_t report[CK_FIDO2_HID_REPORT_SIZE];
    if(furi_hal_hid_u2f_get_request(report) != CK_FIDO2_HID_REPORT_SIZE) {
        ck_fido2_zero(report, sizeof(report));
        return false;
    }
    bool cancel = ck_fido2_report_is_cancel(report, service->active_channel);
    if(!cancel && service->deferred_count < CK_FIDO2_REQUEST_QUEUE_DEPTH) {
        size_t tail =
            (service->deferred_head + service->deferred_count) % CK_FIDO2_REQUEST_QUEUE_DEPTH;
        memcpy(service->deferred_reports[tail], report, sizeof(report));
        service->deferred_count++;
    }
    ck_fido2_zero(report, sizeof(report));
    return cancel;
}

static bool ck_fido2_user_present(void* context) {
    CkFido2Service* service = context;
    while(furi_semaphore_acquire(service->approval_semaphore, 0) == FuriStatusOk) {
    }
    service->presence_approved = false;
    service->presence_cancelled = false;
    service->presence_pending = true;
    view_dispatcher_send_custom_event(service->dispatcher, CkFido2ServiceEventPresence);
    uint32_t elapsed = 0U;
    bool approved = false;
    while(service->running && elapsed < CK_FIDO2_SERVICE_APPROVAL_TIMEOUT_MS) {
        if(ck_fido2_consume_cancel(service)) {
            service->presence_cancelled = true;
            break;
        }
        uint8_t keepalive = CK_FIDO2_KEEPALIVE_STATUS_UPNEEDED;
        ck_fido2_send_message(
            service->active_channel, CK_FIDO2_HID_CMD_KEEPALIVE, &keepalive, sizeof(keepalive));
        FuriStatus status =
            furi_semaphore_acquire(service->approval_semaphore, CK_FIDO2_SERVICE_POLL_INTERVAL_MS);
        if(status == FuriStatusOk) {
            approved = service->presence_approved;
            break;
        }
        elapsed += CK_FIDO2_SERVICE_POLL_INTERVAL_MS;
    }
    service->presence_pending = false;
    service->presence_approved = false;
    view_dispatcher_send_custom_event(service->dispatcher, CkFido2ServiceEventPresenceDone);
    return approved;
}

static bool ck_fido2_was_cancelled(void* context) {
    CkFido2Service* service = context;
    return service->presence_cancelled;
}

static bool ck_fido2_save_credential(void* context, const uint8_t* record, size_t length) {
    CkFido2Service* service = context;
    uint8_t records[CK_FIDO2_SERVICE_MAX_CREDENTIALS][CK_CTAP2_CREDENTIAL_RECORD_SIZE];
    size_t count = 0;
    bool ok = false;
    memset(records, 0, sizeof(records));
    if(!record || length != CK_CTAP2_CREDENTIAL_RECORD_SIZE ||
       record[0] != CK_CTAP2_CREDENTIAL_RECORD_VERSION)
        goto cleanup;

    CkFido2StoreState state = ck_fido2_decrypt_records(service, records, &count);
    if(state == CkFido2StoreInvalid) goto cleanup;
    if(state == CkFido2StoreMissing) count = 0;
    size_t index = count;
    for(size_t i = 0; i < count; i++) {
        if(memcmp(
               records[i] + CK_CTAP2_RECORD_CREDENTIAL_ID_OFFSET,
               record + CK_CTAP2_RECORD_CREDENTIAL_ID_OFFSET,
               CK_CTAP2_CREDENTIAL_ID_SIZE) == 0) {
            index = i;
            break;
        }
    }
    if(index == count) {
        if(count >= CK_FIDO2_SERVICE_MAX_CREDENTIALS) goto cleanup;
        count++;
    }
    memcpy(records[index], record, length);
    ok = ck_fido2_encrypt_records(service, records, count);

cleanup:
    ck_fido2_zero(records, sizeof(records));
    return ok;
}

static bool ck_fido2_load_credential(
    void* context,
    const uint8_t credential_id[CK_CTAP2_CREDENTIAL_ID_SIZE],
    uint8_t* record,
    size_t capacity,
    size_t* length) {
    CkFido2Service* service = context;
    uint8_t records[CK_FIDO2_SERVICE_MAX_CREDENTIALS][CK_CTAP2_CREDENTIAL_RECORD_SIZE];
    size_t count = 0;
    bool found = false;
    if(length) *length = 0;
    memset(records, 0, sizeof(records));
    if(!credential_id || !record || !length || capacity < CK_CTAP2_CREDENTIAL_RECORD_SIZE)
        goto cleanup;
    if(ck_fido2_decrypt_records(service, records, &count) != CkFido2StoreValid) goto cleanup;
    for(size_t i = 0; i < count; i++) {
        if(memcmp(
               records[i] + CK_CTAP2_RECORD_CREDENTIAL_ID_OFFSET,
               credential_id,
               CK_CTAP2_CREDENTIAL_ID_SIZE) == 0) {
            memcpy(record, records[i], CK_CTAP2_CREDENTIAL_RECORD_SIZE);
            *length = CK_CTAP2_CREDENTIAL_RECORD_SIZE;
            found = true;
            break;
        }
    }

cleanup:
    ck_fido2_zero(records, sizeof(records));
    return found;
}

static void ck_fido2_usb_callback(HidU2fEvent event, void* context) {
    CkFido2Service* service = context;
    if(event == HidU2fRequest && service->request_semaphore)
        furi_semaphore_release(service->request_semaphore);
}

static void ck_fido2_send_message(
    uint32_t channel,
    uint8_t command,
    const uint8_t* message,
    size_t message_length) {
    uint8_t reports[18][CK_FIDO2_HID_REPORT_SIZE];
    size_t report_count =
        ck_fido2_hid_encode(channel, command, message, message_length, reports, 18U);
    for(size_t i = 0; i < report_count; i++)
        furi_hal_hid_u2f_send_response(reports[i], CK_FIDO2_HID_REPORT_SIZE);
    ck_fido2_zero(reports, sizeof(reports));
}

static int32_t ck_fido2_worker(void* context) {
    CkFido2Service* service = context;
    CkCtap2 ctap2;
    CkFido2Hid hid;
    CkFido2HidParser parser;
    CkCtap2Platform platform = {
        .context = service,
        .random = ck_fido2_random,
        .user_present = ck_fido2_user_present,
        .was_cancelled = ck_fido2_was_cancelled,
        .save_credential = ck_fido2_save_credential,
        .load_credential = ck_fido2_load_credential,
    };
    uint8_t request[CK_FIDO2_SERVICE_MAX_MESSAGE];
    uint8_t response[CK_FIDO2_SERVICE_MAX_MESSAGE];
    uint8_t report[CK_FIDO2_HID_REPORT_SIZE];
    memset(request, 0, sizeof(request));
    memset(response, 0, sizeof(response));
    memset(report, 0, sizeof(report));
    ck_ctap2_init(&ctap2, &platform);
    ck_fido2_hid_init(&hid, &ctap2, 1U);
    ck_fido2_hid_parser_reset(&parser);

    while(service->running) {
        if(service->deferred_count) {
            memcpy(report, service->deferred_reports[service->deferred_head], sizeof(report));
            ck_fido2_zero(
                service->deferred_reports[service->deferred_head], CK_FIDO2_HID_REPORT_SIZE);
            service->deferred_head = (service->deferred_head + 1U) % CK_FIDO2_REQUEST_QUEUE_DEPTH;
            service->deferred_count--;
        } else {
            if(furi_semaphore_acquire(service->request_semaphore, FuriWaitForever) != FuriStatusOk)
                continue;
            if(!service->running) break;
            if(furi_hal_hid_u2f_get_request(report) != CK_FIDO2_HID_REPORT_SIZE) continue;
        }
        uint32_t offending_channel = ((uint32_t)report[0] << 24) | ((uint32_t)report[1] << 16) |
                                     ((uint32_t)report[2] << 8) | report[3];
        size_t request_length = 0;
        uint32_t channel = 0;
        uint8_t command = 0;
        uint8_t error = 0;
        bool complete = ck_fido2_hid_parser_consume(
            &parser, report, request, sizeof(request), &request_length, &channel, &command, &error);
        ck_fido2_zero(report, sizeof(report));
        if(!complete) {
            if(error) ck_fido2_send_message(offending_channel, CK_FIDO2_HID_CMD_ERROR, &error, 1U);
            continue;
        }

        size_t response_length = 0;
        uint8_t response_command = CK_FIDO2_HID_CMD_ERROR;
        service->active_channel = channel;
        if(ck_fido2_hid_dispatch(
               &hid,
               channel,
               command,
               request,
               request_length,
               response,
               sizeof(response),
               &response_length,
               &response_command)) {
            ck_fido2_send_message(channel, response_command, response, response_length);
        }
        service->active_channel = 0U;
        ck_fido2_zero(request, sizeof(request));
        ck_fido2_zero(response, sizeof(response));
    }

    ck_fido2_zero(request, sizeof(request));
    ck_fido2_zero(response, sizeof(response));
    ck_fido2_zero(report, sizeof(report));
    ck_fido2_zero(&ctap2, sizeof(ctap2));
    ck_fido2_zero(&hid, sizeof(hid));
    return 0;
}

CkFido2Service* ck_fido2_service_alloc(
    Storage* storage,
    const uint8_t vault_key[32],
    ViewDispatcher* dispatcher) {
    if(!storage || !vault_key || !dispatcher) return NULL;
    CkFido2Service* service = malloc(sizeof(CkFido2Service));
    if(!service) return NULL;
    memset(service, 0, sizeof(*service));
    service->storage = storage;
    service->dispatcher = dispatcher;
    memcpy(service->vault_key, vault_key, sizeof(service->vault_key));
    service->approval_semaphore = furi_semaphore_alloc(1U, 0U);
    service->request_semaphore = furi_semaphore_alloc(CK_FIDO2_REQUEST_QUEUE_DEPTH, 0U);
    service->worker =
        furi_thread_alloc_ex("CkFido2", CK_FIDO2_WORKER_STACK, ck_fido2_worker, service);
    if(!service->approval_semaphore || !service->request_semaphore || !service->worker) {
        if(service->worker) furi_thread_free(service->worker);
        if(service->approval_semaphore) furi_semaphore_free(service->approval_semaphore);
        if(service->request_semaphore) furi_semaphore_free(service->request_semaphore);
        ck_fido2_zero(service, sizeof(*service));
        free(service);
        return NULL;
    }
    return service;
}

bool ck_fido2_service_start(CkFido2Service* service) {
    if(!service) return false;
    if(service->running) return true;
    service->previous_usb = furi_hal_usb_get_config();
    if(service->previous_usb != &usb_hid_u2f && !furi_hal_usb_set_config(&usb_hid_u2f, NULL)) {
        if(service->previous_usb) furi_hal_usb_set_config(service->previous_usb, NULL);
        service->previous_usb = NULL;
        return false;
    }
    service->running = true;
    furi_thread_start(service->worker);
    furi_hal_hid_u2f_set_callback(ck_fido2_usb_callback, service);
    return true;
}

void ck_fido2_service_stop(CkFido2Service* service) {
    if(!service) return;
    furi_hal_hid_u2f_set_callback(NULL, NULL);
    if(service->running) {
        service->running = false;
        service->presence_approved = false;
        if(service->presence_pending) furi_semaphore_release(service->approval_semaphore);
        furi_semaphore_release(service->request_semaphore);
        furi_thread_join(service->worker);
    }
    if(service->previous_usb && furi_hal_usb_get_config() != service->previous_usb)
        furi_hal_usb_set_config(service->previous_usb, NULL);
    service->previous_usb = NULL;
    service->presence_pending = false;
}

void ck_fido2_service_free(CkFido2Service* service) {
    if(!service) return;
    ck_fido2_service_stop(service);
    furi_thread_free(service->worker);
    furi_semaphore_free(service->approval_semaphore);
    furi_semaphore_free(service->request_semaphore);
    ck_fido2_zero(service, sizeof(*service));
    free(service);
}

bool ck_fido2_service_is_running(const CkFido2Service* service) {
    return service && service->running;
}

bool ck_fido2_service_presence_pending(const CkFido2Service* service) {
    return service && service->presence_pending;
}

void ck_fido2_service_answer_presence(CkFido2Service* service, bool approved) {
    if(!service || !service->presence_pending) return;
    service->presence_approved = approved;
    furi_semaphore_release(service->approval_semaphore);
}
