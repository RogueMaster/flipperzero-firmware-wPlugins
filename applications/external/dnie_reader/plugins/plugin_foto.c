// Plugin de foto: JPEG2000, JPEG, pixel art y retrato del modo demo.

#include <flipper_application/flipper_application.h>

#include "../dnie_plugin_api.h"
#include "../demo_face.inc"

static const DnieFotoApi api = {
    .j2k_decode_luma = dnie_j2k_decode_luma,
    .jpeg_decode_dc = dnie_jpeg_decode_dc,
    .pixelart_prepare = dnie_pixelart_prepare,
    .pixelart_dither = dnie_pixelart_dither,
    .demo_face = DEMO_FACE,
    .demo_face_w = DEMO_FACE_W,
    .demo_face_h = DEMO_FACE_H,
};

static const FlipperAppPluginDescriptor descriptor = {
    .appid = DNIE_PLUGIN_APP_ID,
    .ep_api_version = DNIE_PLUGIN_API_VERSION,
    .entry_point = &api,
};

const FlipperAppPluginDescriptor* dnie_foto_ep(void) {
    return &descriptor;
}
