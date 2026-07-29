// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
//
// QR encoder plugin. Thin adapter over the vendored Nayuki generator -- the
// point of the file is that qrcodegen.o lives HERE and not in the app image.
// See plugins/qr_plugin_api.h for why.
#include "../qr_plugin_api.h"

#include <flipper_application/flipper_application.h>
#include <flipper_application/plugins/plugin_manager.h>

#include "qrcodegen.h"

// If the vendored encoder's buffer maths ever changes, fail the BUILD rather
// than hand the app a buffer size that is wrong at runtime. The app sizes its
// model buffer from the ABI constant and never sees qrcodegen.h.
_Static_assert(
    QR_PLUGIN_BUF_LEN == qrcodegen_BUFFER_LEN_FOR_VERSION(QR_PLUGIN_MAX_VERSION),
    "QR_PLUGIN_BUF_LEN is out of sync with qrcodegen's buffer length");

static bool qr_plugin_encode_text(const char* text, uint8_t* temp, uint8_t* out) {
    return qrcodegen_encodeText(
        text,
        temp,
        out,
        qrcodegen_Ecc_LOW,
        qrcodegen_VERSION_MIN,
        QR_PLUGIN_MAX_VERSION,
        qrcodegen_Mask_AUTO,
        true);
}

static int qr_plugin_get_size(const uint8_t* qr) {
    return qrcodegen_getSize(qr);
}

static bool qr_plugin_get_module(const uint8_t* qr, int x, int y) {
    return qrcodegen_getModule(qr, x, y);
}

static const QrPluginApi qr_plugin_api = {
    .encode_text = qr_plugin_encode_text,
    .get_size = qr_plugin_get_size,
    .get_module = qr_plugin_get_module,
};

static const FlipperAppPluginDescriptor qr_plugin_descriptor = {
    .appid = QR_PLUGIN_APP_ID,
    .ep_api_version = QR_PLUGIN_API_VERSION,
    .entry_point = &qr_plugin_api,
};

const FlipperAppPluginDescriptor* qr_plugin_ep(void) {
    return &qr_plugin_descriptor;
}
