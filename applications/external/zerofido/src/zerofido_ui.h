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

#include <stdbool.h>
#include <stdint.h>

#include "zerofido_app_i.h"
#include "zerofido_store.h"

/*
 * UI functions are the synchronization boundary between protocol workers and
 * Flipper views. Request helpers create approval/selection state, workers wait
 * on the approval semaphore, and callbacks publish the terminal state.
 */
bool zerofido_ui_init(ZerofidoApp *app);
void zerofido_ui_deinit(ZerofidoApp *app);
void zerofido_ui_refresh_status(ZerofidoApp *app);
void zerofido_ui_refresh_status_line(ZerofidoApp *app);
void zerofido_ui_refresh_credentials_status(ZerofidoApp *app);
void zerofido_ui_set_status(ZerofidoApp *app, const char *text);
void zerofido_ui_set_status_locked(ZerofidoApp *app, const char *text);
void zerofido_ui_set_transport_connected(ZerofidoApp *app, bool connected);
bool zerofido_ui_request_approval(ZerofidoApp *app, ZfUiProtocol protocol, const char *operation,
                                  const char *target_id, const char *user_text,
                                  ZfTransportSessionId current_session_id, bool *approved);
bool zerofido_ui_request_assertion_selection(ZerofidoApp *app, const char *rp_id,
                                             const uint16_t *match_indices, size_t match_count,
                                             ZfTransportSessionId current_session_id,
                                             uint32_t *selected_record_index);
bool zerofido_ui_deny_pending_interaction(ZerofidoApp *app);
bool zerofido_ui_cancel_pending_interaction(ZerofidoApp *app);
bool zerofido_ui_cancel_pending_interaction_locked(ZerofidoApp *app);
bool zerofido_ui_expire_pending_interaction(ZerofidoApp *app);
ZfApprovalState zerofido_ui_get_interaction_state(ZerofidoApp *app);
