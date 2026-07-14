// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file xiaomi_filter_worker.c
 * @brief NFC worker implementation (custom NTAG213 poller state machine).
 */
#include "xiaomi_filter_worker.h"

#include <furi.h>
#include <string.h>

#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller.h>

struct XiaomiFilterWorker {
    Nfc* nfc; /**< Borrowed, owned by the app. */
    NfcPoller* poller; /**< Owned while a poll is running. */

    XiaomiFilterWorkerOp op; /**< Reset the counter, or only read it. */
    XiaomiFilterWorkerCallback callback;
    void* context;

    XiaomiFilterWorkerResult result;
    uint32_t old_counter;
    bool old_counter_valid; /**< Whether old_counter was actually read (step 5). */
    uint8_t factory_id_page[XIAOMI_FILTER_PAGE_SIZE];
    uint8_t product_id_page[XIAOMI_FILTER_PAGE_SIZE];
};

/**
 * @brief Run the tag transaction against an activated tag (reset or read-only check).
 *
 * Always authenticates and reads the counter; for op == Reset it then writes zeros and
 * verifies, while op == Check stops after the read and never writes. Runs entirely inside
 * the poller callback (worker thread), where the instanced MfUltralight commands are legal
 * to call. Returns the outcome; never writes the counter unless authentication succeeded first.
 */
static XiaomiFilterWorkerResult
    xiaomi_filter_worker_run(XiaomiFilterWorker* worker, MfUltralightPoller* poller) {
    MfUltralightPageReadCommandData read = {0};

    // 1. Read pages 0..3 (unprotected) to obtain the 7-byte UID.
    if(mf_ultralight_poller_read_page(poller, 0, &read) != MfUltralightErrorNone) {
        return XiaomiFilterWorkerResultReadFailed;
    }
    uint8_t uid[XIAOMI_FILTER_UID_LEN];
    xiaomi_filter_uid_from_pages(read.page[0].data, read.page[1].data, uid);

    // 2. Derive the tag password from the UID.
    MfUltralightPollerAuthContext auth = {0};
    auth.skip_auth = false;
    if(!xiaomi_filter_derive_password(uid, sizeof(uid), auth.password.data)) {
        return XiaomiFilterWorkerResultReadFailed;
    }

    // 3. Authenticate. mf_ultralight_poller_auth_pwd() returns MfUltralightErrorNone
    // only when the tag answers PWD_AUTH with a valid PACK, i.e. accepts the password
    // — the definitive "genuine Xiaomi filter" signal. Note: it does NOT populate
    // auth.auth_success (only the standard read flow does), so the error code is the
    // only meaningful result here.
    MfUltralightError err = mf_ultralight_poller_auth_pwd(poller, &auth);
    if(err != MfUltralightErrorNone) {
        return XiaomiFilterWorkerResultAuthFailed;
    }

    // 4. Read the identification pages (page 4 factory id, page 5 product id).
    if(mf_ultralight_poller_read_page(poller, XIAOMI_FILTER_FACTORY_ID_PAGE, &read) ==
       MfUltralightErrorNone) {
        memcpy(worker->factory_id_page, read.page[0].data, XIAOMI_FILTER_PAGE_SIZE);
        memcpy(worker->product_id_page, read.page[1].data, XIAOMI_FILTER_PAGE_SIZE);
    }

    // 5. Read the current usage counter (page 8). For a Check this IS the result; for a
    // Reset it is best-effort before/after feedback (a failed read leaves old_counter_valid
    // false, so the UI never claims a prior state it could not read).
    const MfUltralightError counter_err =
        mf_ultralight_poller_read_page(poller, XIAOMI_FILTER_COUNTER_PAGE, &read);
    if(counter_err == MfUltralightErrorNone) {
        worker->old_counter = xiaomi_filter_counter_from_page(read.page[0].data);
        worker->old_counter_valid = true;
    }

    // A Check stops here: the tag is authenticated (a genuine filter) and its counter read,
    // with nothing written back. A failed counter read is fatal for a Check, since the
    // counter is the whole point of the probe.
    if(worker->op == XiaomiFilterWorkerOpCheck) {
        return (counter_err == MfUltralightErrorNone) ? XiaomiFilterWorkerResultSuccess :
                                                        XiaomiFilterWorkerResultReadFailed;
    }

    // 6. Write zeros to the counter page: resets filter life to 100%.
    const MfUltralightPage zero = {0};
    err = mf_ultralight_poller_write_page(poller, XIAOMI_FILTER_COUNTER_PAGE, &zero);
    if(err != MfUltralightErrorNone) {
        return XiaomiFilterWorkerResultWriteFailed;
    }

    // 7. Read the counter back and verify it is cleared.
    if(mf_ultralight_poller_read_page(poller, XIAOMI_FILTER_COUNTER_PAGE, &read) !=
           MfUltralightErrorNone ||
       !xiaomi_filter_page_is_zero(read.page[0].data)) {
        return XiaomiFilterWorkerResultVerifyFailed;
    }

    return XiaomiFilterWorkerResultSuccess;
}

static NfcCommand xiaomi_filter_worker_poller_callback(NfcGenericEventEx event, void* context) {
    XiaomiFilterWorker* worker = context;
    furi_assert(event.poller);
    furi_assert(event.parent_event_data);

    const Iso14443_3aPollerEvent* iso_event = event.parent_event_data;
    if(iso_event->type != Iso14443_3aPollerEventTypeReady) {
        // Not activated yet (or activation error): keep polling for a tag.
        return NfcCommandContinue;
    }

    worker->result = xiaomi_filter_worker_run(worker, event.poller);

    if(worker->callback) {
        worker->callback(worker->context);
    }
    return NfcCommandStop;
}

XiaomiFilterWorker* xiaomi_filter_worker_alloc(void) {
    XiaomiFilterWorker* worker = malloc(sizeof(XiaomiFilterWorker));
    memset(worker, 0, sizeof(*worker));
    return worker;
}

void xiaomi_filter_worker_free(XiaomiFilterWorker* worker) {
    furi_assert(worker);
    furi_assert(worker->poller == NULL);
    free(worker);
}

void xiaomi_filter_worker_start(
    XiaomiFilterWorker* worker,
    Nfc* nfc,
    XiaomiFilterWorkerOp op,
    XiaomiFilterWorkerCallback callback,
    void* context) {
    furi_assert(worker);
    furi_assert(nfc);
    furi_assert(worker->poller == NULL);

    worker->nfc = nfc;
    worker->op = op;
    worker->callback = callback;
    worker->context = context;
    worker->result = XiaomiFilterWorkerResultNotDetected;
    worker->old_counter = 0;
    worker->old_counter_valid = false;
    memset(worker->factory_id_page, 0, sizeof(worker->factory_id_page));
    memset(worker->product_id_page, 0, sizeof(worker->product_id_page));

    worker->poller = nfc_poller_alloc(nfc, NfcProtocolMfUltralight);
    nfc_poller_start_ex(worker->poller, xiaomi_filter_worker_poller_callback, worker);
}

void xiaomi_filter_worker_stop(XiaomiFilterWorker* worker) {
    furi_assert(worker);
    if(worker->poller) {
        nfc_poller_stop(worker->poller);
        nfc_poller_free(worker->poller);
        worker->poller = NULL;
    }
}

XiaomiFilterWorkerResult xiaomi_filter_worker_get_result(const XiaomiFilterWorker* worker) {
    return worker->result;
}

bool xiaomi_filter_worker_get_old_counter(const XiaomiFilterWorker* worker, uint32_t* out_counter) {
    if(worker->old_counter_valid && out_counter) {
        *out_counter = worker->old_counter;
    }
    return worker->old_counter_valid;
}

void xiaomi_filter_worker_get_product_code(
    const XiaomiFilterWorker* worker,
    char out_code[XIAOMI_FILTER_PRODUCT_CODE_SIZE]) {
    xiaomi_filter_product_code(worker->factory_id_page, worker->product_id_page, out_code);
}
