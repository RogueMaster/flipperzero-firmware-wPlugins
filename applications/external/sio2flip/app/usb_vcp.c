/*
 * This file is part of the 8-bit ATAR SIO Emulator for Flipper Zero
 * (https://github.com/cepetr/sio2flip).
 * Copyright (c) 2025
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <furi_hal_usb_cdc.h>
#include <furi_hal_usb.h>

#include <app/app_common.h>

#include "usb_vcp.h"

#define VCP_IF_NUM 1

struct UsbVcp {
    FuriHalUsbInterface* usb_if_prev;
    SIODriver* sio;
};

// Forward declarations
static CdcCallbacks cdc_cb;
static void usb_vcp_send(void* context, const void* data, size_t size);
static size_t usb_vcp_receive(void* context, void* data, size_t size);

UsbVcp* usb_vcp_alloc(SIODriver* sio) {
    UsbVcp* vcp = (UsbVcp*)malloc(sizeof(UsbVcp));
    furi_check(vcp != NULL);

    memset(vcp, 0, sizeof(UsbVcp));

    vcp->usb_if_prev = furi_hal_usb_get_config();

    if(!furi_hal_usb_set_config(&usb_cdc_dual, NULL)) {
        FURI_LOG_E(TAG, "Failed to setup dual cdc interface");
        goto cleanup;
    }

    furi_hal_cdc_set_callbacks(VCP_IF_NUM, &cdc_cb, vcp);

    sio_driver_set_stream_callbacks(sio, usb_vcp_send, usb_vcp_receive, vcp);
    vcp->sio = sio;

    return vcp;

cleanup:
    usb_vcp_free(vcp);
    return NULL;
}

void usb_vcp_free(UsbVcp* vcp) {
    if(vcp != NULL) {
        if(vcp->sio != NULL) {
            sio_driver_set_stream_callbacks(vcp->sio, NULL, NULL, NULL);
        }

        furi_hal_cdc_set_callbacks(VCP_IF_NUM, NULL, NULL);

        furi_hal_usb_set_config(vcp->usb_if_prev, NULL);

        free(vcp);
    }
}

static void usb_vcp_on_tx_ep_complete(void* context) {
    UNUSED(context);
}

static void usb_vcp_on_rx_ep_data(void* context) {
    UsbVcp* vcp = (UsbVcp*)context;

    sio_driver_wakeup_tx(vcp->sio);
}

static void usb_vcp_on_state_changed(void* context, uint8_t state) {
    UNUSED(context);
    UNUSED(state);
}

static void usb_vcp_on_line_changed(void* context, uint8_t ctrl_lines) {
    UNUSED(context);
    UNUSED(ctrl_lines);
}

static void usb_vcp_on_config_changed(void* context, struct usb_cdc_line_coding* config) {
    UNUSED(context);
    UNUSED(config);
}

static CdcCallbacks cdc_cb = {
    .tx_ep_callback = usb_vcp_on_tx_ep_complete,
    .rx_ep_callback = usb_vcp_on_rx_ep_data,
    .state_callback = usb_vcp_on_state_changed,
    .ctrl_line_callback = usb_vcp_on_line_changed,
    .config_callback = usb_vcp_on_config_changed,
};

static void usb_vcp_send(void* context, const void* data, size_t size) {
    UsbVcp* vcp = (UsbVcp*)context;

    UNUSED(vcp);

    furi_hal_cdc_send(VCP_IF_NUM, (uint8_t*)data, size);
}

static size_t usb_vcp_receive(void* context, void* data, size_t size) {
    UsbVcp* vcp = (UsbVcp*)context;

    UNUSED(vcp);

    return furi_hal_cdc_receive(VCP_IF_NUM, (uint8_t*)data, size);
}
