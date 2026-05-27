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

#include "dispatch.h"

typedef struct ZerofidoApp ZerofidoApp;

/*
 * USB HID worker entry points implement the transport adapter contract. The
 * worker owns the USB interface while running and funnels HID packets through
 * usb_hid_session before protocol dispatch.
 */
int32_t zf_transport_usb_hid_worker(void *context);
void zf_transport_usb_hid_stop(ZerofidoApp *app);
void zf_transport_usb_hid_send_dispatch_result(ZerofidoApp *app,
                                               const ZfProtocolDispatchRequest *request,
                                               const ZfProtocolDispatchResult *result);
bool zf_transport_usb_hid_wait_for_interaction(ZerofidoApp *app,
                                               ZfTransportSessionId current_session_id,
                                               bool *approved);
void zf_transport_usb_hid_notify_interaction_changed(ZerofidoApp *app);
uint8_t zf_transport_usb_hid_poll_cbor_control(ZerofidoApp *app,
                                               ZfTransportSessionId current_session_id);
