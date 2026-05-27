/*
 * ZeroFIDO
 * Copyright (C) 2026 Alex Stoyanov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "session.h"

#include "apdu.h"
#include "response_encode.h"
#include "session_internal.h"
#include "persistence.h"
#include "../zerofido_crypto.h"

#include <furi.h>

#include <string.h>

#define TAG "U2f"

#if !ZF_RELEASE_DIAGNOSTICS
#undef FURI_LOG_E
#undef FURI_LOG_W
#define FURI_LOG_E(...) ((void)0)
#define FURI_LOG_W(...) ((void)0)
#endif

static const uint8_t ver_str[] = {"U2F_V2"};

/* Allocates U2F state. */
U2fData *u2f_alloc(void) {
    return calloc(1, sizeof(U2fData));
}

void u2f_free(U2fData *U2F) {
    furi_assert(U2F);
    zf_crypto_secure_zero(U2F, sizeof(*U2F));
    free(U2F);
}

/*
 * Loads or creates U2F attestation, device key, and counter state. Missing
 * device key/counter files are initialized, but malformed existing files fail
 * closed so credential derivation cannot silently change.
 */
bool u2f_init(U2fData *U2F) {
    furi_assert(U2F);

    U2F->cert_ready = false;
    if (u2f_data_cert_check() && u2f_data_cert_key_load(U2F->cert_key) &&
        u2f_data_cert_key_matches(U2F->cert_key)) {
        U2F->cert_ready = true;
    } else {
        FURI_LOG_W(TAG, "U2F attestation assets unavailable; U2F register disabled");
        zf_crypto_secure_zero(U2F->cert_key, sizeof(U2F->cert_key));
    }
    if (u2f_data_key_load(U2F->device_key) == false) {
        if (u2f_data_key_exists()) {
            FURI_LOG_E(TAG, "Device key load error");
            return false;
        }
        FURI_LOG_W(TAG, "Device key missing, generating new");
        if (!u2f_data_key_generate(U2F->device_key)) {
            FURI_LOG_E(TAG, "Key write failed");
            return false;
        }
    }
    if (u2f_data_cnt_read(&U2F->counter) == false) {
        if (u2f_data_cnt_exists()) {
            FURI_LOG_E(TAG, "Counter load error");
            return false;
        }
        FURI_LOG_W(TAG, "Counter missing, initializing to zero");
        U2F->counter = 0;
        if (!u2f_data_cnt_write(0)) {
            FURI_LOG_E(TAG, "Counter write failed");
            return false;
        }
    }
    U2F->counter_high_water = U2F->counter;

    U2F->ready = true;
    return true;
}

void u2f_set_event_callback(U2fData *U2F, U2fEvtCallback callback, void *context) {
    furi_assert(U2F);
    furi_assert(callback);
    U2F->callback = callback;
    U2F->context = context;
}

/* User presence is intentionally one-shot to match U2F authenticate/register semantics. */
void u2f_confirm_user_present(U2fData *U2F) {
    U2F->user_present = true;
}

bool u2f_consume_user_present(U2fData *U2F) {
    bool user_present = U2F->user_present;
    U2F->user_present = false;
    return user_present;
}

void u2f_clear_user_present(U2fData *U2F) {
    U2F->user_present = false;
}

/*
 * Validates the APDU, dispatches register/authenticate/version, and writes the
 * response back into the caller buffer used by USB HID MSG or NFC APDU paths.
 */
uint16_t u2f_msg_parse(U2fData *U2F, uint8_t *buf, uint16_t request_len,
                       uint16_t response_capacity) {
    furi_assert(U2F);
    if (!U2F->ready)
        return 0;
    if (response_capacity < 2) {
        return zf_u2f_write_status(buf, ZF_U2F_SW_WRONG_LENGTH);
    }

    uint16_t validation_status = u2f_validate_request(buf, request_len);
    if (validation_status != 0) {
        return validation_status;
    }

    if (buf[1] == U2F_CMD_REGISTER) { // Register request
        return zf_u2f_encode_register_response(U2F, buf, request_len, response_capacity);

    } else if (buf[1] == U2F_CMD_AUTHENTICATE) { // Authenticate request
        return zf_u2f_encode_authenticate_response(U2F, buf, request_len, response_capacity);

    } else if (buf[1] == U2F_CMD_VERSION) { // Get U2F version string
        if (response_capacity < 6 + ZF_U2F_STATUS_SIZE) {
            return zf_u2f_write_status(buf, ZF_U2F_SW_WRONG_LENGTH);
        }
        memcpy(&buf[0], ver_str, 6);
        zf_u2f_write_status(&buf[6], ZF_U2F_SW_NO_ERROR);
        return 8;
    } else {
        return zf_u2f_write_status(buf, ZF_U2F_SW_INS_NOT_SUPPORTED);
    }
    return 0;
}

void u2f_wink(U2fData *U2F) {
    if (U2F->callback != NULL)
        U2F->callback(U2fNotifyWink, U2F->context);
}

void u2f_set_state(U2fData *U2F, uint8_t state) {
    if (state == 0) {
        if (U2F->callback != NULL)
            U2F->callback(U2fNotifyDisconnect, U2F->context);
    } else {
        if (U2F->callback != NULL)
            U2F->callback(U2fNotifyConnect, U2F->context);
    }
    U2F->user_present = false;
}
