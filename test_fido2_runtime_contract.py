#!/usr/bin/env python3
"""Source-shape checks for the clean-room physical FIDO2 runtime seam."""
from pathlib import Path

service = Path("ck42x_fido2_service.c").read_text(encoding="utf-8")
header = Path("ck42x_fido2_service.h").read_text(encoding="utf-8")
app = Path("ck42x_passvault.c").read_text(encoding="utf-8")
ctap = Path("ck42x_ctap2.c").read_text(encoding="utf-8")
uecc = Path("vendor/micro-ecc/uECC.c").read_text(encoding="utf-8")

assert "#define uECC_OPTIMIZATION_LEVEL 2" in uecc

callback_start = service.index("static void ck_fido2_usb_callback")
callback = service[
    callback_start:
    service.index("static void ck_fido2_send_message", callback_start)
]
assert "furi_semaphore_release(service->request_semaphore)" in callback
assert "furi_thread_flags_set" not in callback
assert "furi_hal_hid_u2f_get_request" not in callback
assert "ck_fido2_hid_dispatch" not in callback

worker = service[
    service.index("static int32_t ck_fido2_worker"):
    service.index("CkFido2Service* ck_fido2_service_alloc")
]
assert "furi_hal_hid_u2f_get_request(report)" in worker
assert "ck_fido2_hid_parser_consume" in worker
assert "ck_fido2_hid_dispatch" in worker
assert "furi_hal_hid_u2f_send_response" in service
assert "CK_FIDO2_HID_CMD_KEEPALIVE" in service
assert "CK_FIDO2_KEEPALIVE_STATUS_UPNEEDED" in service
assert "CK_CTAP2_ERR_KEEPALIVE_CANCEL" in ctap
assert "CK_FIDO2_SERVICE_POLL_INTERVAL_MS 100U" in header
assert "FuriSemaphore* request_semaphore;" in service
assert "furi_semaphore_acquire(service->request_semaphore" in worker

assert "#define CK_FIDO2_SERVICE_MAX_MESSAGE 1024U" in header
assert "#define CK_FIDO2_SERVICE_MAX_CREDENTIALS 20U" in header
assert "CK_CTAP2_CREDENTIAL_RECORD_SIZE" in service
assert 'APP_DATA_PATH("fido2.pv1")' in service
assert "'C', 'K', 'F', '2', 'P', 'V', '1', 0" in service
assert "furi_hal_crypto_gcm_encrypt_and_tag" in service
assert "furi_hal_crypto_gcm_decrypt_and_verify" in service
assert "CK_FIDO2_SERVICE_MAX_CREDENTIALS" in service

start = service[service.index("bool ck_fido2_service_start"):]
assert "service->previous_usb = furi_hal_usb_get_config();" in start
assert "furi_hal_usb_set_config(&usb_hid_u2f, NULL)" in start
assert start.count("furi_hal_usb_set_config(service->previous_usb, NULL)") >= 2
assert "furi_hal_hid_u2f_set_callback(NULL, NULL);" in start
assert "ck_fido2_service_stop(service);" in service

presence = service[
    service.index("static bool ck_fido2_user_present"):
    service.index("static bool ck_fido2_save_credential")
]
assert "furi_semaphore_acquire" in presence
assert "CK_FIDO2_SERVICE_APPROVAL_TIMEOUT_MS" in presence
assert "CK_FIDO2_SERVICE_POLL_INTERVAL_MS" in presence
assert "ck_fido2_consume_cancel" in presence
assert "service->active_channel" in presence
assert "CK_FIDO2_HID_CMD_CANCEL" in service
assert "service->presence_approved = false;" in presence
assert "CkFido2ServiceEventPresence" in presence

assert "ck_fido2_service_answer_presence(app->fido2_service, false)" in app
assert "ck_fido2_service_answer_presence(app->fido2_service, true)" in app
assert "ck_fido2_service_free(app->fido2_service);" in app
assert '"Approve"' in app and '"Deny"' in app

store_write = service[
    service.index("static bool ck_fido2_encrypt_records"):
    service.index("static bool ck_fido2_random")
]
assert 'CK_FIDO2_TEMP_SUFFIX ".tmp"' in service
assert 'CK_FIDO2_BACKUP_SUFFIX ".bak"' in service
assert "storage_file_sync(file)" in store_write
assert "storage_common_rename" in store_write
assert "storage_common_remove" in store_write
save = service[
    service.index("static bool ck_fido2_save_credential"):
    service.index("static bool ck_fido2_load_credential")
]
assert save.index("if(state == CkFido2StoreInvalid) goto cleanup;") < save.index(
    "if(state == CkFido2StoreMissing) count = 0;"
)
assert "CkFido2StoreInvalid" in save
assert "FSE_NOT_EXIST" in service
recovery = service[
    service.index("static void ck_fido2_recover_interrupted_update"):
    service.index("static bool ck_fido2_read_file")
]
assert "if(!target_exists && backup_exists)" in recovery
assert "storage_common_rename" in recovery
assert "storage_common_remove" in recovery

assert "if(credential.sign_count != UINT32_MAX)" in ctap
assert "type_public_key" in ctap
assert "user_found" in ctap
assert "uv_requested" in ctap

print("OK: FIDO2 runtime lifecycle contract checks passed")
