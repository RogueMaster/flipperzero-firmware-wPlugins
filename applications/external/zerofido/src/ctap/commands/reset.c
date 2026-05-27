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

#include "reset.h"

#include "../core/approval.h"
#include "../core/assertion_queue.h"
#include "../core/internal.h"
#include "../parse.h"
#include "../policy.h"
#include "../../u2f/adapter.h"
#include "../../u2f/persistence.h"
#include "../../zerofido_app_i.h"
#include "../../zerofido_crypto.h"
#include "../../zerofido_notify.h"
#include "../../zerofido_pin.h"
#include "../../zerofido_store.h"

/*
 * Reset requires an empty payload and a local approval gesture, then enters the
 * maintenance lock so transport requests cannot observe half-wiped state.
 */
uint8_t zf_ctap_handle_reset(ZerofidoApp *app, ZfTransportSessionId session_id, size_t request_len,
                             size_t *out_len) {
    uint8_t status = zf_ctap_require_empty_payload(request_len);
    bool maintenance_acquired = false;

    if (status != ZF_CTAP_SUCCESS) {
        return status;
    }

    status =
        zf_ctap_request_approval(app, "Reset", "ZeroFIDO", "Erase credentials and PIN", session_id);
    if (status != ZF_CTAP_SUCCESS) {
        return status;
    }

    if (!zf_ctap_begin_maintenance(app)) {
        return ZF_CTAP_ERR_NOT_ALLOWED;
    }
    maintenance_acquired = true;

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    zf_ctap_assertion_queue_clear(app);
    furi_mutex_release(app->ui_mutex);

    zf_u2f_adapter_deinit(app);
    bool wiped = u2f_data_wipe(app->storage) && zf_store_wipe_app_data(app->storage);
    ZfClientPinState next_pin_state = {0};
    ZfPinInitResult pin_init = wiped ? zerofido_pin_init_with_result(app->storage, &next_pin_state)
                                     : ZfPinInitStorageError;

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    if (wiped) {
        zf_store_clear(&app->store);
        if (pin_init == ZfPinInitOk) {
            app->pin_state = next_pin_state;
        } else {
            zf_crypto_secure_zero(&app->pin_state, sizeof(app->pin_state));
        }
    }
    furi_mutex_release(app->ui_mutex);
    zf_crypto_secure_zero(&next_pin_state, sizeof(next_pin_state));
    if (wiped) {
        zf_runtime_config_refresh_capabilities(app);
    }

    if (!wiped || pin_init != ZfPinInitOk) {
        status = ZF_CTAP_ERR_OTHER;
        goto cleanup;
    }

    if (app->capabilities.u2f_enabled) {
        (void)zf_u2f_adapter_init(app);
    }

    zerofido_notify_reset(app);
    *out_len = 0;
    status = ZF_CTAP_SUCCESS;

cleanup:
    if (maintenance_acquired) {
        zf_ctap_end_maintenance(app);
    }
    return status;
}
