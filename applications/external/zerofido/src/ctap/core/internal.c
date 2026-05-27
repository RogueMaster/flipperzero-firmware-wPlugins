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

#include "internal.h"

#include "../policy.h"
#include "../../zerofido_app_i.h"
#include "../../zerofido_pin.h"

void *zf_ctap_command_scratch(ZerofidoApp *app, size_t size) {
    return zf_app_command_scratch_acquire(app, size);
}

bool zf_ctap_begin_maintenance(ZerofidoApp *app) {
    bool acquired = false;

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    if (!app->maintenance_busy) {
        app->maintenance_busy = true;
        acquired = true;
    }
    furi_mutex_release(app->ui_mutex);
    return acquired;
}

void zf_ctap_end_maintenance(ZerofidoApp *app) {
    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    app->maintenance_busy = false;
    furi_mutex_release(app->ui_mutex);
}

/*
 * Verifies PIN/UV authorization against a private copy of app->pin_state.
 * zerofido_pin_require_auth may update retries, auth-block state, token expiry,
 * or managed permissions; those mutations are published back under ui_mutex
 * before the maintenance gate is released.
 */
uint8_t zf_ctap_require_pin_auth_with_state(ZerofidoApp *app, ZfClientPinState *pin_state,
                                            bool uv_requested, bool has_pin_auth,
                                            const uint8_t client_data_hash[ZF_CLIENT_DATA_HASH_LEN],
                                            const uint8_t *pin_auth, size_t pin_auth_len,
                                            bool has_pin_protocol, uint64_t pin_protocol,
                                            const char *rp_id, uint64_t required_permissions,
                                            bool *uv_verified) {
    uint8_t status = ZF_CTAP_ERR_OTHER;

    if (!pin_state) {
        return ZF_CTAP_ERR_OTHER;
    }
    if (!zf_ctap_begin_maintenance(app)) {
        return ZF_CTAP_ERR_NOT_ALLOWED;
    }

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    *pin_state = app->pin_state;
    furi_mutex_release(app->ui_mutex);

    status = zerofido_pin_require_auth(app->storage, pin_state, uv_requested, has_pin_auth,
                                       client_data_hash, pin_auth, pin_auth_len, has_pin_protocol,
                                       pin_protocol, rp_id, required_permissions, uv_verified);

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    app->pin_state = *pin_state;
    furi_mutex_release(app->ui_mutex);
    zf_ctap_end_maintenance(app);
    return status;
}

uint8_t zf_ctap_dispatch_require_idle(ZerofidoApp *app) {
    /*
     * This is only a pre-dispatch fast gate. Stateful handlers acquire the
     * real maintenance gate before mutating shared state, so do not block on
     * ui_mutex here: NFC readers may poll CTAP MSG repeatedly while the
     * request thread is trying to enter this function.
     */
    return app && app->maintenance_busy ? ZF_CTAP_ERR_NOT_ALLOWED : ZF_CTAP_SUCCESS;
}
