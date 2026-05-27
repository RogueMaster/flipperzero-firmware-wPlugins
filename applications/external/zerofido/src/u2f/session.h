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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <furi.h>

#include "common.h"

typedef enum {
    U2fNotifyRegister,
    U2fNotifyAuth,
    U2fNotifyAuthSuccess,
    U2fNotifyWink,
    U2fNotifyConnect,
    U2fNotifyDisconnect,
    U2fNotifyError,
} U2fNotifyEvent;

typedef struct U2fData U2fData;

typedef void (*U2fEvtCallback)(U2fNotifyEvent evt, void *context);

/*
 * U2F owns the legacy APDU-level state: attestation assets, device key,
 * monotonic counter, and one-shot user-presence flag. The transport layer
 * supplies raw APDUs and consumes the encoded APDU response in-place.
 */
U2fData *u2f_alloc(void);

bool u2f_init(U2fData *instance);

void u2f_free(U2fData *instance);

void u2f_set_event_callback(U2fData *instance, U2fEvtCallback callback, void *context);

void u2f_confirm_user_present(U2fData *instance);
bool u2f_consume_user_present(U2fData *instance);
void u2f_clear_user_present(U2fData *instance);

uint16_t u2f_msg_parse(U2fData *instance, uint8_t *buf, uint16_t request_len,
                       uint16_t response_capacity);

void u2f_wink(U2fData *instance);

void u2f_set_state(U2fData *instance, uint8_t state);

#ifdef __cplusplus
}
#endif
